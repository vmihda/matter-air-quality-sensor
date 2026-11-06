#include "sensors_task.h"

#include <atomic>
#include <cstdint>

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "aq_config.h"
#include "aq_engine.h"
#include "board_config.h"
#include "matter_app.h"
#include "pms5003.h"
#include "scd41.h"

static const char *TAG = "sensors";

static constexpr TickType_t LOOP_PERIOD = pdMS_TO_TICKS(1000);
static constexpr uint32_t PMS_READ_TIMEOUT_MS = 3000;
static constexpr uint32_t SCD41_RESTART_AFTER_MS = 30000;
static constexpr uint32_t TASK_STACK_BYTES = 4096;
static constexpr UBaseType_t TASK_PRIORITY = 3;

static std::atomic<bool> s_has_fault{false};
static std::atomic<bool> s_has_first_readings{false};

struct Measurements {
    float co2_ppm = 0;
    float pm25_ugm3 = 0;
    float temperature_c = 0;
    float humidity_pct = 0;
    bool has_co2 = false;
    bool has_pm25 = false;
};

static uint64_t now_ms()
{
    return static_cast<uint64_t>(esp_timer_get_time() / 1000);
}

// Runs the whole sequence so a sensor that lost power comes back in periodic mode.
static bool start_scd41()
{
    if (scd41_init(BOARD_PIN_SCD41_SDA, BOARD_PIN_SCD41_SCL) != ESP_OK ||
        scd41_set_temp_offset(AQ_TEMP_OFFSET_C) != ESP_OK || scd41_start() != ESP_OK) {
        ESP_LOGW(TAG, "SCD41 did not start, will retry");
        return false;
    }
    return true;
}

class Scd41Poller {
public:
    void poll(aq_engine_t &engine, Measurements &values, uint64_t now)
    {
        if (now - last_attempt_ms_ >= SCD41_RESTART_AFTER_MS && now - last_success_ms_ >= SCD41_RESTART_AFTER_MS) {
            last_attempt_ms_ = now;
            running_ = start_scd41();
        }
        if (!running_) {
            return;
        }
        const esp_err_t err = scd41_read(&values.co2_ppm, &values.temperature_c, &values.humidity_pct);
        if (err == ESP_OK) {
            last_success_ms_ = now;
            values.has_co2 = true;
            aq_engine_on_co2(&engine, values.co2_ppm, now);
        } else if (err != ESP_ERR_NOT_FOUND) {
            ESP_LOGW(TAG, "SCD41 read failed: %s", esp_err_to_name(err));
        }
    }

    void start(uint64_t now)
    {
        last_attempt_ms_ = now;
        last_success_ms_ = now;
        running_ = start_scd41();
    }

private:
    uint64_t last_attempt_ms_ = 0;
    uint64_t last_success_ms_ = 0;
    bool running_ = false;
};

class Pms5003Poller {
public:
    void poll(aq_engine_t &engine, Measurements &values, uint64_t now)
    {
        if (!awake_) {
            if (now < next_wake_ms_) {
                return;
            }
            awake_ = pms5003_set_awake(true) == ESP_OK;
            woke_at_ms_ = now;
            return;
        }
        if (now - woke_at_ms_ < AQ_PMS_SETTLE_MS) {
            return;
        }

        uint16_t pm25 = 0;
        const esp_err_t err = pms5003_read(&pm25, PMS_READ_TIMEOUT_MS);
        if (err == ESP_OK) {
            values.pm25_ugm3 = pm25;
            values.has_pm25 = true;
            aq_engine_on_pm25(&engine, values.pm25_ugm3, now);
        } else {
            ESP_LOGW(TAG, "PMS5003 read failed: %s", esp_err_to_name(err));
        }
        next_wake_ms_ = woke_at_ms_ + AQ_PMS_CYCLE_MS;
#if AQ_PMS_USE_SLEEP
        awake_ = pms5003_set_awake(false) != ESP_OK;
#else
        woke_at_ms_ = next_wake_ms_ - AQ_PMS_SETTLE_MS;
#endif
    }

private:
    uint64_t next_wake_ms_ = 0;
    uint64_t woke_at_ms_ = 0;
    bool awake_ = false;
};

static bool alerts_differ(const aq_output_t &a, const aq_output_t &b)
{
    return a.ventilate_open != b.ventilate_open || a.dust_open != b.dust_open || a.fault_open != b.fault_open;
}

static void publish_measurements(const Measurements &values)
{
    if (values.has_co2) {
        matter_app_set_co2(values.co2_ppm);
        matter_app_set_temperature(values.temperature_c);
        matter_app_set_humidity(values.humidity_pct);
    }
    if (values.has_pm25) {
        matter_app_set_pm25(values.pm25_ugm3);
    }
}

static void sensors_task(void *)
{
    aq_engine_t engine;
    aq_engine_init(&engine, now_ms());
    Measurements values;
    Scd41Poller scd41;
    Pms5003Poller pms5003;
    scd41.start(now_ms());

    aq_output_t published{};
    uint64_t last_publish_ms = 0;
    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        const uint64_t now = now_ms();
        scd41.poll(engine, values, now);
        pms5003.poll(engine, values, now);

        const aq_output_t output = aq_engine_evaluate(&engine, now);
        s_has_fault = output.fault_open;
        s_has_first_readings = values.has_co2 && values.has_pm25;

        if (alerts_differ(output, published)) {
            matter_app_set_alerts(output.ventilate_open, output.dust_open, output.fault_open);
        }
        if (output.level != published.level) {
            matter_app_set_level(output.level);
        }
        published = output;

        if (now - last_publish_ms >= AQ_PUBLISH_PERIOD_MS) {
            last_publish_ms = now;
            publish_measurements(values);
        }
        vTaskDelayUntil(&last_wake, LOOP_PERIOD);
    }
}

esp_err_t sensors_task_start()
{
    const esp_err_t err = pms5003_init(BOARD_PMS_UART_NUM, BOARD_PIN_PMS_TX, BOARD_PIN_PMS_RX);
    if (err != ESP_OK) {
        return err;
    }
    return xTaskCreate(sensors_task, "sensors", TASK_STACK_BYTES, nullptr, TASK_PRIORITY, nullptr) == pdPASS
               ? ESP_OK
               : ESP_ERR_NO_MEM;
}

bool sensors_task_has_fault()
{
    return s_has_fault;
}

bool sensors_task_has_first_readings()
{
    return s_has_first_readings;
}
