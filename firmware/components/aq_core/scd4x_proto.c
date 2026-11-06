#include "scd4x_proto.h"

static bool decode_words(const uint8_t raw[9], uint16_t words[3])
{
    for (int i = 0; i < 3; i++) {
        const uint8_t *word = raw + i * 3;
        if (scd4x_crc8(word, 2) != word[2]) {
            return false;
        }
        words[i] = (uint16_t)((word[0] << 8) | word[1]);
    }
    return true;
}

uint8_t scd4x_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

bool scd4x_decode_measurement(const uint8_t raw[9], float *co2_ppm, float *temp_c, float *rh_pct)
{
    uint16_t words[3];
    if (!decode_words(raw, words) || words[0] == 0) {
        return false;
    }
    *co2_ppm = (float)words[0];
    *temp_c = -45.0f + 175.0f * (float)words[1] / 65535.0f;
    *rh_pct = 100.0f * (float)words[2] / 65535.0f;
    return true;
}

bool scd4x_decode_serial(const uint8_t raw[9], uint64_t *serial)
{
    uint16_t words[3];
    if (!decode_words(raw, words)) {
        return false;
    }
    *serial = ((uint64_t)words[0] << 32) | ((uint64_t)words[1] << 16) | words[2];
    return true;
}
