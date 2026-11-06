#include "status_led.h"

#include <atomic>
#include <cstdint>

#include <driver/gpio.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "board_config.h"
#include "matter_app.h"
#include "sensors_task.h"
#include "status_policy.h"

static constexpr TickType_t REFRESH_PERIOD = pdMS_TO_TICKS(20);
static constexpr uint32_t TASK_STACK_BYTES = 2560;
static constexpr UBaseType_t TASK_PRIORITY = 2;
static constexpr uint64_t SHOW_DURATION_MS = 3000;

static std::atomic<bool> s_resetting{false};
static std::atomic<uint64_t> s_show_until_ms{0};

static uint64_t now_ms()
{
    return static_cast<uint64_t>(esp_timer_get_time() / 1000);
}

static void write_led(bool on)
{
    gpio_set_level(static_cast<gpio_num_t>(BOARD_PIN_LED), on == BOARD_LED_ACTIVE_LOW ? 0 : 1);
}

static void status_led_task(void *)
{
    while (true) {
        const status_inputs_t inputs = {
            .resetting = s_resetting,
            .sensor_fault = sensors_task_has_fault(),
            .commissioned = matter_app_is_commissioned(),
            .wifi_connected = matter_app_is_wifi_connected(),
            .has_first_readings = sensors_task_has_first_readings(),
        };
        const status_t status = status_select(&inputs);
        const uint64_t now = now_ms();
        // A button press lights the LED steadily so a healthy device is easy to confirm.
        const bool show_ok = status == STATUS_OK && now < s_show_until_ms;
        write_led(show_ok || status_led_is_on(status, now));
        vTaskDelay(REFRESH_PERIOD);
    }
}

esp_err_t status_led_start()
{
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << BOARD_PIN_LED,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    const esp_err_t err = gpio_config(&config);
    if (err != ESP_OK) {
        return err;
    }
    return xTaskCreate(status_led_task, "status_led", TASK_STACK_BYTES, nullptr, TASK_PRIORITY, nullptr) == pdPASS
               ? ESP_OK
               : ESP_ERR_NO_MEM;
}

void status_led_show_for_a_moment()
{
    s_show_until_ms = now_ms() + SHOW_DURATION_MS;
}

void status_led_set_resetting()
{
    s_resetting = true;
}
