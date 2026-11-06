#include "boot_button.h"

#include <cstdint>

#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "board_config.h"

static constexpr uint32_t POLL_MS = 20;
static constexpr uint32_t DEBOUNCE_MS = 40;
static constexpr uint32_t LONG_PRESS_MS = 5000;
static constexpr uint32_t TASK_STACK_BYTES = 4096;
static constexpr UBaseType_t TASK_PRIORITY = 2;

static boot_button_callback_t s_on_short_press;
static boot_button_callback_t s_on_long_press;

static void button_task(void *)
{
    const gpio_num_t pin = static_cast<gpio_num_t>(BOARD_PIN_BOOT);
    uint32_t held_ms = 0;
    bool long_press_fired = false;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
        if (gpio_get_level(pin) == 0) {
            held_ms += POLL_MS;
            if (held_ms >= LONG_PRESS_MS && !long_press_fired) {
                long_press_fired = true;
                s_on_long_press();
            }
            continue;
        }
        if (held_ms >= DEBOUNCE_MS && !long_press_fired) {
            s_on_short_press();
        }
        held_ms = 0;
        long_press_fired = false;
    }
}

esp_err_t boot_button_start(boot_button_callback_t on_short_press, boot_button_callback_t on_long_press)
{
    s_on_short_press = on_short_press;
    s_on_long_press = on_long_press;

    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << BOARD_PIN_BOOT,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    const esp_err_t err = gpio_config(&config);
    if (err != ESP_OK) {
        return err;
    }
    return xTaskCreate(button_task, "boot_button", TASK_STACK_BYTES, nullptr, TASK_PRIORITY, nullptr) == pdPASS
               ? ESP_OK
               : ESP_ERR_NO_MEM;
}
