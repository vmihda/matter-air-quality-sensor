// Parses the PMS5003 UART frame and builds its sleep and wake commands.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PMS_FRAME_SIZE 32

typedef struct pms_parser {
    uint8_t frame[PMS_FRAME_SIZE];
    uint8_t length;
} pms_parser_t;

void pms_parser_init(pms_parser_t *p);

// Returns true when a complete frame with a valid checksum yields pm25 <= 1000.
bool pms_parser_feed(pms_parser_t *p, uint8_t byte, uint16_t *pm25_atm);

void pms_build_sleep_cmd(uint8_t out[7]);
void pms_build_wake_cmd(uint8_t out[7]);

#ifdef __cplusplus
}
#endif
