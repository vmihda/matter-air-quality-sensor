#include "pms5003_proto.h"

#define START_1 0x42
#define START_2 0x4D
#define FRAME_DATA_LENGTH 0x1C
#define PM25_ATMOSPHERIC_OFFSET 12
#define PM25_MAX_UGM3 1000
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

static void build_sleep_mode_cmd(uint8_t out[7], uint8_t mode)
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

void pms_parser_init(pms_parser_t *p)
{
    p->length = 0;
}

bool pms_parser_feed(pms_parser_t *p, uint8_t byte, uint16_t *pm25_atm)
{
    if (!byte_fits_header(p->length, byte)) {
        restart_with(p, byte);
        return false;
    }
    p->frame[p->length++] = byte;
    if (p->length < PMS_FRAME_SIZE) {
        return false;
    }
    p->length = 0;

    if (sum_bytes(p->frame, PMS_FRAME_SIZE - 2) != read_u16(p->frame + PMS_FRAME_SIZE - 2)) {
        return false;
    }
    const uint16_t pm25 = read_u16(p->frame + PM25_ATMOSPHERIC_OFFSET);
    if (pm25 > PM25_MAX_UGM3) {
        return false;
    }
    *pm25_atm = pm25;
    return true;
}

void pms_build_sleep_cmd(uint8_t out[7])
{
    build_sleep_mode_cmd(out, 0x00);
}

void pms_build_wake_cmd(uint8_t out[7])
{
    build_sleep_mode_cmd(out, 0x01);
}
