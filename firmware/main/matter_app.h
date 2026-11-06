// Matter node with the air quality, temperature, humidity and three contact sensor endpoints.
// The setters are safe to call from any task after matter_app_start(); they briefly take the Matter stack lock.
#pragma once

#include <esp_err.h>

#include "aq_engine.h"

[[nodiscard]] esp_err_t matter_app_start();
[[nodiscard]] bool matter_app_is_commissioned();
[[nodiscard]] bool matter_app_is_wifi_connected();
void matter_app_set_level(aq_level_t level);
void matter_app_set_co2(float ppm);
void matter_app_set_pm25(float ugm3);
void matter_app_set_temperature(float celsius);
void matter_app_set_humidity(float percent);
void matter_app_set_alerts(bool ventilate_open, bool dust_open, bool fault_open);
