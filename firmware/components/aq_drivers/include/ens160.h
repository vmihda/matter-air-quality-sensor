// ENS160 driver over the ESP-IDF I2C master API. Not thread safe: call from one task.
#pragma once

#include <stdint.h>

#include "ens160_proto.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    ens160_validity_t validity;
    uint16_t tvoc_ppb;
} ens160_reading_t;

// Finds the sensor at either of its addresses and starts gas sensing. Safe to call again
// to recover a sensor that lost power. Returns ESP_ERR_NOT_FOUND when it does not answer.
esp_err_t ens160_init(int sda_gpio, int scl_gpio);

// Temperature and humidity for the sensor's compensation.
esp_err_t ens160_set_ambient(float celsius, float percent);

// Returns ESP_ERR_NOT_FOUND when no new data is ready, ESP_ERR_INVALID_STATE when sensing has stopped.
esp_err_t ens160_read(ens160_reading_t *reading);

#ifdef __cplusplus
}
#endif
