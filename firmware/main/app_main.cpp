#include <driver/gpio.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include "board_config.h"

static const char *TAG = "app_main";

static void select_builtin_antenna()
{
    const gpio_num_t pins[] = {(gpio_num_t)BOARD_PIN_RF_SWITCH_POWER, (gpio_num_t)BOARD_PIN_RF_SWITCH_SELECT};
    for (gpio_num_t pin : pins) {
        gpio_reset_pin(pin);
        gpio_set_direction(pin, GPIO_MODE_OUTPUT);
        gpio_set_level(pin, 0);
    }
}

extern "C" void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    select_builtin_antenna();
    ESP_LOGI(TAG, "Air quality sensor skeleton started");
}
