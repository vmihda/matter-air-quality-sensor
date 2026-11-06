// Decodes ENS160 registers and encodes its compensation input. Registers are little endian.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENS160_REG_PART_ID 0x00
#define ENS160_REG_OPMODE 0x10
#define ENS160_REG_TEMP_IN 0x13
#define ENS160_REG_DEVICE_STATUS 0x20
#define ENS160_REG_DATA_TVOC 0x22

#define ENS160_OPMODE_IDLE 0x01
#define ENS160_OPMODE_STANDARD 0x02

typedef enum {
    ENS160_VALID_NORMAL = 0,
    ENS160_VALID_WARM_UP = 1,
    ENS160_VALID_INITIAL_START_UP = 2,
    ENS160_VALID_INVALID_OUTPUT = 3,
} ens160_validity_t;

typedef struct {
    ens160_validity_t validity;
    bool new_data;
    bool error;
    bool running;
} ens160_status_t;

bool ens160_is_part_id(const uint8_t raw[2]);
ens160_status_t ens160_decode_status(uint8_t raw);

// Warm-up and first start-up values are not trusted, so they are never shown as a reading.
bool ens160_is_reportable(ens160_validity_t validity);
bool ens160_is_warming_up(ens160_validity_t validity);

// Kelvin times 64, the TEMP_IN format.
uint16_t ens160_encode_temperature(float celsius);

// Percent times 512, the RH_IN format.
uint16_t ens160_encode_humidity(float percent);

// Register address followed by TEMP_IN and RH_IN, ready for one I2C write.
void ens160_build_ambient_write(uint8_t out[5], float celsius, float percent);

uint16_t ens160_decode_tvoc_ppb(const uint8_t raw[2]);

// TVOC is calibrated on ethanol, so the result is an ethanol equivalent and not the mass of the real mixture.
float ens160_ppb_to_ugm3(uint16_t ppb);

#ifdef __cplusplus
}
#endif
