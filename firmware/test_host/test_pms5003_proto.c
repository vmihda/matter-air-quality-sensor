#include <string.h>

#include "minitest.h"
#include "pms5003_proto.h"

#define FRAME_SIZE 32

static void build_frame(uint8_t frame[FRAME_SIZE], uint16_t pm25)
{
    memset(frame, 0, FRAME_SIZE);
    frame[0] = 0x42;
    frame[1] = 0x4D;
    frame[3] = 0x1C;
    frame[12] = pm25 >> 8;
    frame[13] = pm25 & 0xFF;
    uint16_t sum = 0;
    for (int i = 0; i < FRAME_SIZE - 2; i++) {
        sum += frame[i];
    }
    frame[30] = sum >> 8;
    frame[31] = sum & 0xFF;
}

// Feeds bytes and returns how many frames were accepted; last holds the latest value.
static int feed_bytes(pms_parser_t *parser, const uint8_t *bytes, size_t length, uint16_t *last)
{
    int accepted = 0;
    for (size_t i = 0; i < length; i++) {
        uint16_t value;
        if (pms_parser_feed(parser, bytes[i], &value)) {
            *last = value;
            accepted++;
        }
    }
    return accepted;
}

static void valid_frame(void)
{
    pms_parser_t parser;
    uint8_t frame[FRAME_SIZE];
    uint16_t value = 0;
    pms_parser_init(&parser);
    build_frame(frame, 42);
    CHECK(feed_bytes(&parser, frame, FRAME_SIZE, &value) == 1);
    CHECK(value == 42);
}

static void rejects_bad_checksum(void)
{
    pms_parser_t parser;
    uint8_t frame[FRAME_SIZE];
    uint16_t value = 0;
    pms_parser_init(&parser);
    build_frame(frame, 42);
    frame[20] ^= 0x01;
    CHECK(feed_bytes(&parser, frame, FRAME_SIZE, &value) == 0);
}

static void resyncs_mid_frame(void)
{
    pms_parser_t parser;
    uint8_t frame[FRAME_SIZE];
    uint16_t value = 0;
    pms_parser_init(&parser);
    build_frame(frame, 17);
    CHECK(feed_bytes(&parser, frame + FRAME_SIZE / 2, FRAME_SIZE / 2, &value) == 0);
    CHECK(feed_bytes(&parser, frame, FRAME_SIZE, &value) == 1);
    CHECK(value == 17);
}

static void ignores_false_header(void)
{
    pms_parser_t parser;
    uint8_t frame[FRAME_SIZE];
    uint16_t value = 0;
    const uint8_t noise[] = {0x42, 0x42, 0x4D, 0x42};
    pms_parser_init(&parser);
    build_frame(frame, 23);
    feed_bytes(&parser, noise, sizeof(noise), &value);
    CHECK(feed_bytes(&parser, frame, FRAME_SIZE, &value) == 1);
    CHECK(value == 23);
}

static void rejects_bad_length(void)
{
    pms_parser_t parser;
    uint8_t bad[FRAME_SIZE];
    uint8_t good[FRAME_SIZE];
    uint16_t value = 0;
    pms_parser_init(&parser);
    build_frame(bad, 99);
    bad[3] = 0x14;
    build_frame(good, 7);
    CHECK(feed_bytes(&parser, bad, FRAME_SIZE, &value) == 0);
    CHECK(feed_bytes(&parser, good, FRAME_SIZE, &value) == 1);
    CHECK(value == 7);
}

static void rejects_out_of_range(void)
{
    pms_parser_t parser;
    uint8_t frame[FRAME_SIZE];
    uint16_t value = 0;
    pms_parser_init(&parser);
    build_frame(frame, 1001);
    CHECK(feed_bytes(&parser, frame, FRAME_SIZE, &value) == 0);
    build_frame(frame, 1000);
    CHECK(feed_bytes(&parser, frame, FRAME_SIZE, &value) == 1);
    CHECK(value == 1000);
}

static void sleep_cmd_bytes(void)
{
    const uint8_t expected[7] = {0x42, 0x4D, 0xE4, 0x00, 0x00, 0x01, 0x73};
    uint8_t out[7];
    pms_build_sleep_cmd(out);
    CHECK(memcmp(out, expected, 7) == 0);
}

static void wake_cmd_bytes(void)
{
    const uint8_t expected[7] = {0x42, 0x4D, 0xE4, 0x00, 0x01, 0x01, 0x74};
    uint8_t out[7];
    pms_build_wake_cmd(out);
    CHECK(memcmp(out, expected, 7) == 0);
}

int main(void)
{
    RUN_TEST(valid_frame);
    RUN_TEST(rejects_bad_checksum);
    RUN_TEST(resyncs_mid_frame);
    RUN_TEST(ignores_false_header);
    RUN_TEST(rejects_bad_length);
    RUN_TEST(rejects_out_of_range);
    RUN_TEST(sleep_cmd_bytes);
    RUN_TEST(wake_cmd_bytes);
    return MINITEST_RESULT();
}
