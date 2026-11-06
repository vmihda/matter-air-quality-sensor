// Matter node with the air quality, temperature and humidity endpoints.
// The setters are safe to call from any task after matter_app_start(); they briefly take the Matter stack lock.
#pragma once

#include <optional>

#include <esp_err.h>

#include "aq_engine.h"

[[nodiscard]] esp_err_t matter_app_start();
[[nodiscard]] bool matter_app_is_commissioned();
[[nodiscard]] bool matter_app_is_wifi_connected();
void matter_app_set_level(aq_level_t level);
// An empty value tells Home that there is no reading, instead of showing an old number or clean air.
void matter_app_set_co2(std::optional<float> ppm);
void matter_app_set_pm25(std::optional<float> ugm3);
void matter_app_set_pm10(std::optional<float> ugm3);
void matter_app_set_tvoc(std::optional<float> ugm3);
void matter_app_set_temperature(std::optional<float> celsius);
void matter_app_set_humidity(std::optional<float> percent);
