#include <string.h>

#include "minitest.h"
#include "scd4x_proto.h"

static void put_word(uint8_t *raw, int word_index, uint16_t value)
{
    uint8_t *word = raw + word_index * 3;
    word[0] = value >> 8;
    word[1] = value & 0xFF;
    word[2] = scd4x_crc8(word, 2);
}

static void build_measurement(uint8_t raw[9], uint16_t co2, uint16_t temperature, uint16_t humidity)
{
    put_word(raw, 0, co2);
    put_word(raw, 1, temperature);
    put_word(raw, 2, humidity);
}

static void crc8_datasheet_vector(void)
{
    const uint8_t data[2] = {0xBE, 0xEF};
    CHECK(scd4x_crc8(data, 2) == 0x92);
}

static void decode_valid(void)
{
    uint8_t raw[9];
    float co2, temperature, humidity;
    build_measurement(raw, 1000, 26214, 32768);
    CHECK(scd4x_decode_measurement(raw, &co2, &temperature, &humidity));
    CHECK_NEAR(co2, 1000, 0.01);
    CHECK_NEAR(temperature, 25.0, 0.01);
    CHECK_NEAR(humidity, 50.0, 0.01);
}

static void decode_rejects_bad_crc(void)
{
    for (int word = 0; word < 3; word++) {
        uint8_t raw[9];
        float co2, temperature, humidity;
        build_measurement(raw, 1000, 26214, 32768);
        raw[word * 3 + 2] ^= 0xFF;
        CHECK(!scd4x_decode_measurement(raw, &co2, &temperature, &humidity));
    }
}

static void decode_rejects_zero_co2(void)
{
    uint8_t raw[9];
    float co2, temperature, humidity;
    build_measurement(raw, 0, 26214, 32768);
    CHECK(!scd4x_decode_measurement(raw, &co2, &temperature, &humidity));
}

static void decode_serial(void)
{
    uint8_t raw[9];
    uint64_t serial = 0;
    put_word(raw, 0, 0x1234);
    put_word(raw, 1, 0x5678);
    put_word(raw, 2, 0x9ABC);
    CHECK(scd4x_decode_serial(raw, &serial));
    CHECK(serial == 0x123456789ABCULL);
}

static void decode_serial_rejects_bad_crc(void)
{
    uint8_t raw[9];
    uint64_t serial = 0;
    put_word(raw, 0, 0x1234);
    put_word(raw, 1, 0x5678);
    put_word(raw, 2, 0x9ABC);
    raw[5] ^= 0xFF;
    CHECK(!scd4x_decode_serial(raw, &serial));
}

static void build_data_ready(uint8_t raw[3], uint16_t status)
{
    raw[0] = status >> 8;
    raw[1] = status & 0xFF;
    raw[2] = scd4x_crc8(raw, 2);
}

static void data_ready_when_low_bits_set(void)
{
    uint8_t raw[3];
    bool ready = false;
    build_data_ready(raw, 0x8006);
    CHECK(scd4x_decode_data_ready(raw, &ready));
    CHECK(ready);
}

static void data_not_ready_when_low_bits_clear(void)
{
    uint8_t raw[3];
    bool ready = true;
    build_data_ready(raw, 0x8000);
    CHECK(scd4x_decode_data_ready(raw, &ready));
    CHECK(!ready);
}

static void data_ready_rejects_bad_crc(void)
{
    uint8_t raw[3];
    bool ready = false;
    build_data_ready(raw, 0x8006);
    raw[2] ^= 0xFF;
    CHECK(!scd4x_decode_data_ready(raw, &ready));
}

// The example comes from the SCD4x datasheet: 5.4 degrees C is 0x07E6.
static void temperature_offset_matches_datasheet_example(void)
{
    CHECK(scd4x_encode_temp_offset(5.4f) == 0x07E6);
}

int main(void)
{
    RUN_TEST(temperature_offset_matches_datasheet_example);
    RUN_TEST(data_ready_when_low_bits_set);
    RUN_TEST(data_not_ready_when_low_bits_clear);
    RUN_TEST(data_ready_rejects_bad_crc);
    RUN_TEST(crc8_datasheet_vector);
    RUN_TEST(decode_valid);
    RUN_TEST(decode_rejects_bad_crc);
    RUN_TEST(decode_rejects_zero_co2);
    RUN_TEST(decode_serial);
    RUN_TEST(decode_serial_rejects_bad_crc);
    return MINITEST_RESULT();
}
