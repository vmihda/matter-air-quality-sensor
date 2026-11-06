#include <array>

#include <driver/gpio.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <sdkconfig.h>

#include "board_config.h"
#if CONFIG_AQ_BRINGUP
#include "bringup.h"
#elif CONFIG_AQ_DEMO_VALUES
#include "demo_mode.h"
#include "matter_app.h"
#else
#include "matter_app.h"
#endif

static const char *TAG = "app_main";

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
#endif
}

extern "C" void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    select_builtin_antenna();
    start_firmware();
}
