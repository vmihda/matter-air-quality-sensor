#include "pms5003_proto.h"

#include <stddef.h>
#include <string.h>

#define START_1 0x42
#define START_2 0x4D
#define FRAME_DATA_LENGTH 0x1C
#define PM1_ATMOSPHERIC_OFFSET 10
#define PM25_ATMOSPHERIC_OFFSET 12
#define PM10_ATMOSPHERIC_OFFSET 14
#define PM_MAX_UGM3 1000
#define CMD_SLEEP_MODE 0xE4

static uint16_t read_u16(const uint8_t *bytes)
{
    return (uint16_t)((bytes[0] << 8) | bytes[1]);
}

static uint16_t sum_bytes(const uint8_t *bytes, int length)
{
    uint16_t sum = 0;
    for (int i = 0; i < length; i++) {
        sum = (uint16_t)(sum + bytes[i]);
    }
    return sum;
}

// A byte that breaks the header may itself begin the next frame.
static void restart_with(pms_parser_t *p, uint8_t byte)
{
    p->length = 0;
    if (byte == START_1) {
        p->frame[0] = byte;
        p->length = 1;
    }
}

static bool byte_fits_header(uint8_t position, uint8_t byte)
{
    switch (position) {
    case 0:
        return byte == START_1;
    case 1:
        return byte == START_2;
    case 2:
        return byte == 0x00;
    case 3:
        return byte == FRAME_DATA_LENGTH;
    default:
        return true;
    }
}

static void build_sleep_mode_cmd(uint8_t out[PMS_CMD_SIZE], uint8_t mode)
{
    out[0] = START_1;
    out[1] = START_2;
    out[2] = CMD_SLEEP_MODE;
    out[3] = 0x00;
    out[4] = mode;
    const uint16_t sum = sum_bytes(out, 5);
    out[5] = sum >> 8;
    out[6] = sum & 0xFF;
}

static void accept_byte(pms_parser_t *p, uint8_t byte)
{
    if (byte_fits_header(p->length, byte)) {
        p->frame[p->length++] = byte;
    } else {
        restart_with(p, byte);
    }
}

// A truncated frame swallows the start of the next one, so a real header may be inside the rejected bytes.
static void rescan_rejected_frame(pms_parser_t *p)
{
    uint8_t rejected[PMS_FRAME_SIZE - 1];
    memcpy(rejected, p->frame + 1, sizeof(rejected));
    p->length = 0;
    for (size_t i = 0; i < sizeof(rejected); i++) {
        accept_byte(p, rejected[i]);
    }
}

void pms_parser_init(pms_parser_t *p)
{
    p->length = 0;
}

bool pms_parser_feed(pms_parser_t *p, uint8_t byte, pms_reading_t *reading)
{
    accept_byte(p, byte);
    if (p->length < PMS_FRAME_SIZE) {
        return false;
    }

    if (sum_bytes(p->frame, PMS_FRAME_SIZE - 2) != read_u16(p->frame + PMS_FRAME_SIZE - 2)) {
        rescan_rejected_frame(p);
        return false;
    }
    p->length = 0;

    const pms_reading_t parsed = {
        .pm1 = read_u16(p->frame + PM1_ATMOSPHERIC_OFFSET),
        .pm25 = read_u16(p->frame + PM25_ATMOSPHERIC_OFFSET),
        .pm10 = read_u16(p->frame + PM10_ATMOSPHERIC_OFFSET),
    };
    if (parsed.pm1 > PM_MAX_UGM3 || parsed.pm25 > PM_MAX_UGM3 || parsed.pm10 > PM_MAX_UGM3) {
        return false;
    }
    *reading = parsed;
    return true;
}

void pms_build_sleep_cmd(uint8_t out[PMS_CMD_SIZE])
{
    build_sleep_mode_cmd(out, 0x00);
}

void pms_build_wake_cmd(uint8_t out[PMS_CMD_SIZE])
{
    build_sleep_mode_cmd(out, 0x01);
}
