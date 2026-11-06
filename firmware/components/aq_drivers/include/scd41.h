// SCD41 driver over the ESP-IDF I2C master API. Not thread safe: call from one task.
#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Leaves measurement stopped, including after a reboot of the MCU alone. Safe to call again
// to recover a sensor that lost power or stopped answering.
esp_err_t scd41_init(int sda_gpio, int scl_gpio);

// Must run while measurement is stopped.
esp_err_t scd41_get_serial(uint64_t *serial);

// Must run while measurement is stopped.
esp_err_t scd41_set_temp_offset(float offset_c);

// Starts periodic mode, new data every 5 s.
esp_err_t scd41_start(void);

// Returns ESP_ERR_NOT_FOUND when no new data is ready, ESP_ERR_INVALID_CRC on corrupted data.
esp_err_t scd41_read(float *co2_ppm, float *temp_c, float *rh_pct);

#ifdef __cplusplus
}
#endif
