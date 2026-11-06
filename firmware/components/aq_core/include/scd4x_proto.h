// Decodes SCD4x I2C responses. Each 16-bit word is followed by a CRC-8 byte.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t scd4x_crc8(const uint8_t *data, size_t len);

// Returns false on a CRC error or a CO2 value of zero, which the sensor reports when it has no reading.
bool scd4x_decode_measurement(const uint8_t raw[9], float *co2_ppm, float *temp_c, float *rh_pct);

bool scd4x_decode_serial(const uint8_t raw[9], uint64_t *serial);

#ifdef __cplusplus
}
#endif
