// Chooses what the status LED shows and how each state blinks.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Listed from the highest priority to the lowest.
typedef enum {
    STATUS_RESETTING,
    STATUS_SENSOR_FAULT,
    STATUS_NO_WIFI,
    STATUS_WAITING,
    STATUS_STARTING,
    STATUS_OK,
} status_t;

typedef struct {
    bool resetting;
    bool sensor_fault;
    bool commissioned;
    bool wifi_connected;
    bool has_first_readings;
} status_inputs_t;

status_t status_select(const status_inputs_t *inputs);

// Returns the LED state for a status at a given time since boot.
bool status_led_is_on(status_t status, uint64_t now_ms);

#ifdef __cplusplus
}
#endif
