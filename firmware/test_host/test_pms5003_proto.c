#include <string.h>

#include "minitest.h"
#include "pms5003_proto.h"

// The atmospheric values start at offset 10: PM1, then PM2.5, then PM10.
static void build_frame_with_all_sizes(uint8_t frame[PMS_FRAME_SIZE], uint16_t pm1, uint16_t pm25, uint16_t pm10)
{
    memset(frame, 0, PMS_FRAME_SIZE);
    frame[0] = 0x42;
    frame[1] = 0x4D;
    frame[3] = 0x1C;
    frame[10] = pm1 >> 8;
    frame[11] = pm1 & 0xFF;
    frame[12] = pm25 >> 8;
    frame[13] = pm25 & 0xFF;
    frame[14] = pm10 >> 8;
    frame[15] = pm10 & 0xFF;
    uint16_t sum = 0;
    for (int i = 0; i < PMS_FRAME_SIZE - 2; i++) {
        sum += frame[i];
    }
    frame[30] = sum >> 8;
    frame[31] = sum & 0xFF;
}

static void build_frame(uint8_t frame[PMS_FRAME_SIZE], uint16_t pm25)
{
    build_frame_with_all_sizes(frame, 0, pm25, 0);
}

// Feeds bytes and returns how many frames were accepted; last holds the latest value.
static int feed_bytes(pms_parser_t *parser, const uint8_t *bytes, size_t length, uint16_t *last)
{
    int accepted = 0;
    for (size_t i = 0; i < length; i++) {
        pms_reading_t reading;
        if (pms_parser_feed(parser, bytes[i], &reading)) {
            *last = reading.pm25;
            accepted++;
        }
    }
    return accepted;
}

static void valid_frame(void)
{
    pms_parser_t parser;
    uint8_t frame[PMS_FRAME_SIZE];
    uint16_t value = 0;
    pms_parser_init(&parser);
    build_frame(frame, 42);
    CHECK(feed_bytes(&parser, frame, PMS_FRAME_SIZE, &value) == 1);
    CHECK(value == 42);
}

static void rejects_bad_checksum(void)
{
    pms_parser_t parser;
    uint8_t frame[PMS_FRAME_SIZE];
    uint16_t value = 0;
    pms_parser_init(&parser);
    build_frame(frame, 42);
    frame[20] ^= 0x01;
    CHECK(feed_bytes(&parser, frame, PMS_FRAME_SIZE, &value) == 0);
}

static void resyncs_mid_frame(void)
{
    pms_parser_t parser;
    uint8_t frame[PMS_FRAME_SIZE];
    uint16_t value = 0;
    pms_parser_init(&parser);
    build_frame(frame, 17);
    CHECK(feed_bytes(&parser, frame + PMS_FRAME_SIZE / 2, PMS_FRAME_SIZE / 2, &value) == 0);
    CHECK(feed_bytes(&parser, frame, PMS_FRAME_SIZE, &value) == 1);
    CHECK(value == 17);
}

static void recovers_frame_after_truncated_frame(void)
{
    pms_parser_t parser;
    uint8_t truncated[PMS_FRAME_SIZE];
    uint8_t complete[PMS_FRAME_SIZE];
    uint16_t value = 0;
    pms_parser_init(&parser);
    build_frame(truncated, 11);
    build_frame(complete, 29);
    CHECK(feed_bytes(&parser, truncated, PMS_FRAME_SIZE / 2, &value) == 0);
    CHECK(feed_bytes(&parser, complete, PMS_FRAME_SIZE, &value) == 1);
    CHECK(value == 29);
}

static void ignores_false_header(void)
{
    pms_parser_t parser;
    uint8_t frame[PMS_FRAME_SIZE];
    uint16_t value = 0;
    const uint8_t noise[] = {0x42, 0x42, 0x4D, 0x42};
    pms_parser_init(&parser);
    build_frame(frame, 23);
    feed_bytes(&parser, noise, sizeof(noise), &value);
    CHECK(feed_bytes(&parser, frame, PMS_FRAME_SIZE, &value) == 1);
    CHECK(value == 23);
}

static void rejects_bad_length(void)
{
    pms_parser_t parser;
    uint8_t bad[PMS_FRAME_SIZE];
    uint8_t good[PMS_FRAME_SIZE];
    uint16_t value = 0;
    pms_parser_init(&parser);
    build_frame(bad, 99);
    bad[3] = 0x14;
    build_frame(good, 7);
    CHECK(feed_bytes(&parser, bad, PMS_FRAME_SIZE, &value) == 0);
    CHECK(feed_bytes(&parser, good, PMS_FRAME_SIZE, &value) == 1);
    CHECK(value == 7);
}

static void rejects_out_of_range(void)
{
    pms_parser_t parser;
    uint8_t frame[PMS_FRAME_SIZE];
    uint16_t value = 0;
    pms_parser_init(&parser);
    build_frame(frame, 1001);
    CHECK(feed_bytes(&parser, frame, PMS_FRAME_SIZE, &value) == 0);
    build_frame(frame, 1000);
    CHECK(feed_bytes(&parser, frame, PMS_FRAME_SIZE, &value) == 1);
    CHECK(value == 1000);
}

static void reports_all_particle_sizes(void)
{
    pms_parser_t parser;
    uint8_t frame[PMS_FRAME_SIZE];
    pms_reading_t reading = {0};
    bool accepted = false;
    pms_parser_init(&parser);
    build_frame_with_all_sizes(frame, 5, 12, 300);
    for (int i = 0; i < PMS_FRAME_SIZE; i++) {
        accepted = pms_parser_feed(&parser, frame[i], &reading) || accepted;
    }
    CHECK(accepted);
    CHECK(reading.pm1 == 5);
    CHECK(reading.pm25 == 12);
    CHECK(reading.pm10 == 300);
}

static void rejects_out_of_range_pm1_and_pm10(void)
{
    pms_parser_t parser;
    uint8_t frame[PMS_FRAME_SIZE];
    uint16_t value = 0;
    pms_parser_init(&parser);
    build_frame_with_all_sizes(frame, 1001, 12, 20);
    CHECK(feed_bytes(&parser, frame, PMS_FRAME_SIZE, &value) == 0);
    build_frame_with_all_sizes(frame, 5, 12, 1001);
    CHECK(feed_bytes(&parser, frame, PMS_FRAME_SIZE, &value) == 0);
    build_frame_with_all_sizes(frame, 1000, 12, 1000);
    CHECK(feed_bytes(&parser, frame, PMS_FRAME_SIZE, &value) == 1);
}

static void sleep_cmd_bytes(void)
{
    const uint8_t expected[PMS_CMD_SIZE] = {0x42, 0x4D, 0xE4, 0x00, 0x00, 0x01, 0x73};
    uint8_t out[PMS_CMD_SIZE];
    pms_build_sleep_cmd(out);
    CHECK(memcmp(out, expected, PMS_CMD_SIZE) == 0);
}

static void wake_cmd_bytes(void)
{
    const uint8_t expected[PMS_CMD_SIZE] = {0x42, 0x4D, 0xE4, 0x00, 0x01, 0x01, 0x74};
    uint8_t out[PMS_CMD_SIZE];
    pms_build_wake_cmd(out);
    CHECK(memcmp(out, expected, PMS_CMD_SIZE) == 0);
}

int main(void)
{
    RUN_TEST(valid_frame);
    RUN_TEST(rejects_bad_checksum);
    RUN_TEST(resyncs_mid_frame);
    RUN_TEST(recovers_frame_after_truncated_frame);
    RUN_TEST(ignores_false_header);
    RUN_TEST(rejects_bad_length);
    RUN_TEST(rejects_out_of_range);
    RUN_TEST(reports_all_particle_sizes);
    RUN_TEST(rejects_out_of_range_pm1_and_pm10);
    RUN_TEST(sleep_cmd_bytes);
    RUN_TEST(wake_cmd_bytes);
    return MINITEST_RESULT();
}
