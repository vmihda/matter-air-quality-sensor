#include <array>

#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <sdkconfig.h>

#include "board_config.h"
#include "matter_app.h"

#if CONFIG_AQ_BRINGUP
#include "bringup.h"
#elif CONFIG_AQ_DEMO_VALUES
#include "demo_mode.h"
#include "settings_store.h"
#include "web_ui.h"
#else
#include "boot_button.h"
#include "ota_guard.h"
#include "sensors_task.h"
#include "service_console.h"
#include "settings_store.h"
#include "status_led.h"
#include "web_ui.h"
#endif

static const char *TAG = "app_main";

static void select_builtin_antenna()
{
#if BOARD_HAS_RF_SWITCH
    constexpr std::array<gpio_num_t, 2> pins{static_cast<gpio_num_t>(BOARD_PIN_RF_SWITCH_POWER),
                                             static_cast<gpio_num_t>(BOARD_PIN_RF_SWITCH_SELECT)};
    for (const gpio_num_t pin : pins) {
        gpio_reset_pin(pin);
        gpio_set_direction(pin, GPIO_MODE_OUTPUT);
        gpio_set_level(pin, 0);
    }
#endif
}

#if !CONFIG_AQ_BRINGUP && !CONFIG_AQ_DEMO_VALUES
// Lets the fast blink be seen before the device restarts.
static constexpr TickType_t RESET_SIGNAL_TIME = pdMS_TO_TICKS(1500);

static void factory_reset_with_signal()
{
    ESP_LOGW(TAG, "Factory reset");
    status_led_set_resetting();
    vTaskDelay(RESET_SIGNAL_TIME);
    esp_matter::factory_reset();
}

static void apply_device_name()
{
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == nullptr || esp_netif_set_hostname(netif, CONFIG_AQ_DEVICE_NAME) != ESP_OK) {
        ESP_LOGW(TAG, "could not set the network name");
    }
}

// A non-fatal service must not stop the sensors, the alerts or the reset button.
static void log_if_failed(const char *name, esp_err_t err)
{
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s did not start: %s", name, esp_err_to_name(err));
    }
}
#endif

static void start_firmware()
{
#if CONFIG_AQ_BRINGUP
    ESP_LOGI(TAG, "Sensor bring-up mode");
    bringup_run();
#elif CONFIG_AQ_DEMO_VALUES
    ESP_LOGI(TAG, "Demo mode for the Apple Home and device page checks");
    ESP_ERROR_CHECK(settings_store_init());
    ESP_ERROR_CHECK(matter_app_start());
    ESP_ERROR_CHECK(demo_mode_start());
    ESP_ERROR_CHECK(web_ui_start());
#else
    ESP_ERROR_CHECK(settings_store_init());
    ESP_ERROR_CHECK(matter_app_start());
    apply_device_name();
    ESP_ERROR_CHECK(sensors_task_start());
    ESP_ERROR_CHECK(status_led_start());
    ESP_ERROR_CHECK(boot_button_start(status_led_show_for_a_moment, factory_reset_with_signal));
    // Fatal on purpose: restarting unconfirmed firmware makes the bootloader roll it back.
    ESP_ERROR_CHECK(ota_guard_start());
    log_if_failed("device page", web_ui_start());
    log_if_failed("service console", service_console_start());
#endif
}

extern "C" void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    select_builtin_antenna();
    start_firmware();
}
