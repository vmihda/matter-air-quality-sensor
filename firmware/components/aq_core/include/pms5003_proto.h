// Parses the PMS5003 UART frame and builds its sleep and wake commands.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PMS_FRAME_SIZE 32
#define PMS_CMD_SIZE 7

typedef struct pms_parser {
    uint8_t frame[PMS_FRAME_SIZE];
    uint8_t length;
} pms_parser_t;

// Atmospheric values in micrograms per cubic metre.
typedef struct pms_reading {
    uint16_t pm1;
    uint16_t pm25;
    uint16_t pm10;
} pms_reading_t;

void pms_parser_init(pms_parser_t *p);

// Returns true when a complete frame with a valid checksum has every value <= 1000.
bool pms_parser_feed(pms_parser_t *p, uint8_t byte, pms_reading_t *reading);

void pms_build_sleep_cmd(uint8_t out[PMS_CMD_SIZE]);
void pms_build_wake_cmd(uint8_t out[PMS_CMD_SIZE]);

#ifdef __cplusplus
}
#endif
