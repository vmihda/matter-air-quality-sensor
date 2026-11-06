#include "sensors_task.h"

#include <cstdint>
#include <optional>

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "aq_config.h"
#include "board_config.h"
#include "ens160.h"
#include "matter_app.h"
#include "pms5003.h"
#include "scd41.h"
#include "settings_store.h"

static const char *TAG = "sensors";

static constexpr TickType_t LOOP_PERIOD = pdMS_TO_TICKS(1000);
static constexpr uint32_t PMS_READ_TIMEOUT_MS = 3000;
static constexpr uint32_t SCD41_RESTART_AFTER_MS = 30000;
static constexpr uint32_t ENS160_RESTART_AFTER_MS = 30000;
static constexpr uint32_t ENS160_SILENT_LIMIT_MS = 60000;
static constexpr uint32_t ENS160_AMBIENT_PERIOD_MS = 60000;
static constexpr uint32_t TASK_STACK_BYTES = 4096;
static constexpr UBaseType_t TASK_PRIORITY = 3;

static portMUX_TYPE s_status_lock = portMUX_INITIALIZER_UNLOCKED;
static SensorsStatus s_status{};

static uint64_t now_ms()
{
    return static_cast<uint64_t>(esp_timer_get_time() / 1000);
}

class Scd41Poller {
public:
    void start(float temp_offset_c, uint64_t now)
    {
        temp_offset_c_ = temp_offset_c;
        last_success_ms_ = now;
        // The whole sequence also brings back a sensor that lost power.
        running_ = scd41_init(BOARD_PIN_I2C_SDA, BOARD_PIN_I2C_SCL) == ESP_OK &&
                   scd41_set_temp_offset(temp_offset_c) == ESP_OK && scd41_start() == ESP_OK;
        if (!running_) {
            ESP_LOGW(TAG, "SCD41 did not start, will retry");
        }
    }

    void poll(aq_engine_t &engine, SensorsStatus &values, float temp_offset_c, uint64_t now)
    {
        const bool offset_changed = temp_offset_c != temp_offset_c_;
        const bool silent_too_long = now - last_success_ms_ >= SCD41_RESTART_AFTER_MS;
        if (offset_changed || silent_too_long) {
            start(temp_offset_c, now);
        }
        if (!running_) {
            return;
        }
        const esp_err_t err = scd41_read(&values.co2_ppm, &values.temperature_c, &values.humidity_pct);
        if (err == ESP_OK) {
            last_success_ms_ = now;
            aq_engine_on_co2(&engine, values.co2_ppm, now);
        } else if (err != ESP_ERR_NOT_FOUND) {
            ESP_LOGW(TAG, "SCD41 read failed: 0x%x %s", err, esp_err_to_name(err));
        }
    }

private:
    float temp_offset_c_ = 0;
    uint64_t last_success_ms_ = 0;
    bool running_ = false;
};

class Pms5003Poller {
public:
    void poll(aq_engine_t &engine, SensorsStatus &values, uint32_t cycle_s, uint64_t now)
    {
        if (!awake_) {
            if (!aq_pms_wake_due(woke_before_, woke_at_ms_, cycle_s, now)) {
                return;
            }
            if (pms5003_set_awake(true) == ESP_OK) {
                awake_ = true;
                woke_before_ = true;
                woke_at_ms_ = now;
            }
            return;
        }
        if (now - woke_at_ms_ < AQ_PMS_SETTLE_MS) {
            return;
        }

        pms_reading_t reading{};
        const esp_err_t err = pms5003_read(&reading, PMS_READ_TIMEOUT_MS);
        if (err == ESP_OK) {
            values.pm1_ugm3 = reading.pm1;
            values.pm25_ugm3 = reading.pm25;
            values.pm10_ugm3 = reading.pm10;
            aq_engine_on_pm25(&engine, values.pm25_ugm3, now);
        } else {
            ESP_LOGW(TAG, "PMS5003 read failed: 0x%x %s", err, esp_err_to_name(err));
        }
        // With sleep disabled the fan keeps running, and the wake command is harmless.
#if AQ_PMS_USE_SLEEP
        if (pms5003_set_awake(false) != ESP_OK) {
            ESP_LOGW(TAG, "PMS5003 sleep command failed");
        }
#endif
        awake_ = false;
    }

private:
    uint64_t woke_at_ms_ = 0;
    bool woke_before_ = false;
    bool awake_ = false;
};

// VOC is an extra reading: a missing or failing ENS160 never changes the air quality level or the fault state.
class Ens160Poller {
public:
    void poll(SensorsStatus &values, uint64_t now)
    {
        if (!running_) {
            // The first read waits for the next pass, so the sensor has entered its sensing mode.
            try_start(now);
            return;
        }
        send_ambient(values, now);

        ens160_reading_t reading{};
        const esp_err_t err = ens160_read(&reading);
        if (err == ESP_OK) {
            last_success_ms_ = now;
            values.has_tvoc = ens160_is_reportable(reading.validity);
            values.tvoc_warming_up = ens160_is_warming_up(reading.validity);
            values.tvoc_ugm3 = ens160_ppb_to_ugm3(reading.tvoc_ppb);
        } else if (err == ESP_ERR_INVALID_STATE || now - last_success_ms_ >= ENS160_SILENT_LIMIT_MS) {
            // A single I2C error is retried on the next pass. Only lasting silence or stopped sensing restarts it.
            ESP_LOGW(TAG, "ENS160 stopped answering: 0x%x %s", err, esp_err_to_name(err));
            running_ = false;
            values.has_tvoc = false;
            values.tvoc_warming_up = false;
        }
    }

private:
    void try_start(uint64_t now)
    {
        if (now < next_attempt_ms_) {
            return;
        }
        next_attempt_ms_ = now + ENS160_RESTART_AFTER_MS;
        running_ = ens160_init(BOARD_PIN_I2C_SDA, BOARD_PIN_I2C_SCL) == ESP_OK;
        next_ambient_ms_ = 0;
        last_success_ms_ = now;
    }

    void send_ambient(const SensorsStatus &values, uint64_t now)
    {
        if (!values.has_scd41 || now < next_ambient_ms_) {
            return;
        }
        if (ens160_set_ambient(values.temperature_c, values.humidity_pct) == ESP_OK) {
            next_ambient_ms_ = now + ENS160_AMBIENT_PERIOD_MS;
        }
    }

    uint64_t next_attempt_ms_ = 0;
    uint64_t next_ambient_ms_ = 0;
    uint64_t last_success_ms_ = 0;
    bool running_ = false;
};

static void store_status(const SensorsStatus &status)
{
    taskENTER_CRITICAL(&s_status_lock);
    s_status = status;
    taskEXIT_CRITICAL(&s_status_lock);
}

static std::optional<float> if_present(bool present, float value)
{
    return present ? std::optional<float>{value} : std::nullopt;
}

// A sensor without fresh data reports no value, so Home never shows a frozen number.
static void publish_measurements(const SensorsStatus &values)
{
    matter_app_set_co2(if_present(values.has_scd41, values.co2_ppm));
    matter_app_set_temperature(if_present(values.has_scd41, values.temperature_c));
    matter_app_set_humidity(if_present(values.has_scd41, values.humidity_pct));
    matter_app_set_pm25(if_present(values.has_pms5003, values.pm25_ugm3));
    matter_app_set_pm10(if_present(values.has_pms5003, values.pm10_ugm3));
    matter_app_set_tvoc(if_present(values.has_tvoc, values.tvoc_ugm3));
}

static void sensors_task(void *)
{
    aq_settings_t settings{};
    settings_store_get(&settings);

    aq_engine_t engine{};
    aq_engine_init(&engine, now_ms());
    aq_engine_set_settings(&engine, &settings, now_ms());

    SensorsStatus values{};
    Scd41Poller scd41;
    Pms5003Poller pms5003;
    Ens160Poller ens160;
    scd41.start(settings.temp_offset_c, now_ms());

    aq_level_t published_level{};
    uint64_t last_publish_ms = 0;
    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        const uint64_t now = now_ms();

        settings_store_get(&settings);
        aq_engine_set_settings(&engine, &settings, now);

        scd41.poll(engine, values, settings.temp_offset_c, now);
        pms5003.poll(engine, values, settings.pms_cycle_s, now);
        ens160.poll(values, now);

        values.output = aq_engine_evaluate(&engine, now);
        values.has_scd41 = values.output.co2_valid;
        values.has_pms5003 = values.output.pm25_valid;
        store_status(values);

        if (values.output.level != published_level) {
            matter_app_set_level(values.output.level);
            published_level = values.output.level;
        }

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

#if CONFIG_AQ_DEMO_VALUES
void sensors_task_set_status_for_demo(const SensorsStatus &status)
{
    store_status(status);
    publish_measurements(status);
}
#endif

void sensors_task_get_status(SensorsStatus *out)
{
    taskENTER_CRITICAL(&s_status_lock);
    *out = s_status;
    taskEXIT_CRITICAL(&s_status_lock);
}

bool sensors_task_has_fault()
{
    SensorsStatus status{};
    sensors_task_get_status(&status);
    return status.output.fault_alert;
}

bool sensors_task_has_first_readings()
{
    SensorsStatus status{};
    sensors_task_get_status(&status);
    return status.has_scd41 && status.has_pms5003;
}

bool sensors_task_has_any_reading()
{
    SensorsStatus status{};
    sensors_task_get_status(&status);
    return status.has_scd41 || status.has_pms5003;
}
