#include "ens160_proto.h"

#define PART_ID 0x0160

#define STATUS_RUNNING_BIT 0x80
#define STATUS_ERROR_BIT 0x40
#define STATUS_VALIDITY_SHIFT 2
#define STATUS_VALIDITY_MASK 0x03
#define STATUS_NEW_DATA_BIT 0x02

#define KELVIN_OFFSET 273.15f
#define TEMPERATURE_SCALE 64.0f
#define HUMIDITY_SCALE 512.0f
#define HUMIDITY_MAX_PCT 100.0f

// Ethanol molar mass in g/mol, and the molar volume of a gas in L/mol at 25 degrees C and 1 atm.
#define ETHANOL_MOLAR_MASS 46.0684f
#define MOLAR_VOLUME 24.45f

static uint16_t read_u16_le(const uint8_t bytes[2])
{
    return (uint16_t)(bytes[0] | (bytes[1] << 8));
}

bool ens160_is_part_id(const uint8_t raw[2])
{
    return read_u16_le(raw) == PART_ID;
}

ens160_status_t ens160_decode_status(uint8_t raw)
{
    const ens160_status_t status = {
        .validity = (ens160_validity_t)((raw >> STATUS_VALIDITY_SHIFT) & STATUS_VALIDITY_MASK),
        .new_data = (raw & STATUS_NEW_DATA_BIT) != 0,
        .error = (raw & STATUS_ERROR_BIT) != 0,
        .running = (raw & STATUS_RUNNING_BIT) != 0,
    };
    return status;
}

bool ens160_is_reportable(ens160_validity_t validity)
{
    return validity == ENS160_VALID_NORMAL;
}

bool ens160_is_warming_up(ens160_validity_t validity)
{
    return validity == ENS160_VALID_WARM_UP || validity == ENS160_VALID_INITIAL_START_UP;
}

uint16_t ens160_encode_temperature(float celsius)
{
    const float scaled = (celsius + KELVIN_OFFSET) * TEMPERATURE_SCALE;
    // Written so that NaN gives 0.
    if (!(scaled > 0.0f)) {
        return 0;
    }
    return scaled >= (float)UINT16_MAX ? UINT16_MAX : (uint16_t)(scaled + 0.5f);
}

uint16_t ens160_encode_humidity(float percent)
{
    // Written so that NaN gives 0.
    if (!(percent > 0.0f)) {
        return 0;
    }
    const float limited = percent > HUMIDITY_MAX_PCT ? HUMIDITY_MAX_PCT : percent;
    return (uint16_t)(limited * HUMIDITY_SCALE + 0.5f);
}

void ens160_build_ambient_write(uint8_t out[5], float celsius, float percent)
{
    const uint16_t temperature = ens160_encode_temperature(celsius);
    const uint16_t humidity = ens160_encode_humidity(percent);
    out[0] = ENS160_REG_TEMP_IN;
    out[1] = temperature & 0xFF;
    out[2] = temperature >> 8;
    out[3] = humidity & 0xFF;
    out[4] = humidity >> 8;
}

uint16_t ens160_decode_tvoc_ppb(const uint8_t raw[2])
{
    return read_u16_le(raw);
}

float ens160_ppb_to_ugm3(uint16_t ppb)
{
    return (float)ppb * ETHANOL_MOLAR_MASS / MOLAR_VOLUME;
}
