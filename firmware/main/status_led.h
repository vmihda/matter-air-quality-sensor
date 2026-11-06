// Drives the status LED from the device state and handles the BOOT button actions.
#pragma once

#include <esp_err.h>

[[nodiscard]] esp_err_t status_led_start();
void status_led_show_for_a_moment();
void status_led_set_resetting();
