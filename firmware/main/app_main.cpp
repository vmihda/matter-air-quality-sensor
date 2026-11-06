#include <array>

#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <sdkconfig.h>

#include "board_config.h"
#include "boot_button.h"
#include "bringup.h"
#include "demo_mode.h"
#include "matter_app.h"
#include "sensors_task.h"
#include "status_led.h"

static const char *TAG = "app_main";

// Lets the fast blink be seen before the device restarts.
static constexpr TickType_t RESET_SIGNAL_TIME = pdMS_TO_TICKS(1500);

static void select_builtin_antenna()
{
    constexpr std::array<gpio_num_t, 2> pins{static_cast<gpio_num_t>(BOARD_PIN_RF_SWITCH_POWER),
                                             static_cast<gpio_num_t>(BOARD_PIN_RF_SWITCH_SELECT)};
    for (const gpio_num_t pin : pins) {
        gpio_reset_pin(pin);
        gpio_set_direction(pin, GPIO_MODE_OUTPUT);
        gpio_set_level(pin, 0);
    }
}

static void factory_reset_with_signal()
{
    ESP_LOGW(TAG, "Factory reset");
    status_led_set_resetting();
    vTaskDelay(RESET_SIGNAL_TIME);
    esp_matter::factory_reset();
}

static void start_firmware()
{
#if CONFIG_AQ_BRINGUP
    ESP_LOGI(TAG, "Sensor bring-up mode");
    bringup_run();
#elif CONFIG_AQ_DEMO_VALUES
    ESP_LOGI(TAG, "Demo mode for the Apple Home check");
    ESP_ERROR_CHECK(matter_app_start());
    ESP_ERROR_CHECK(demo_mode_start());
#else
    ESP_ERROR_CHECK(matter_app_start());
    ESP_ERROR_CHECK(sensors_task_start());
    ESP_ERROR_CHECK(status_led_start());
    ESP_ERROR_CHECK(boot_button_start(status_led_show_for_a_moment, factory_reset_with_signal));
#endif
}

extern "C" void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    select_builtin_antenna();
    start_firmware();
}
