#include "status_policy.h"

#define RESETTING_HALF_PERIOD_MS 50u
#define FAULT_HALF_PERIOD_MS 250u
#define WAITING_HALF_PERIOD_MS 500u
#define NO_WIFI_PERIOD_MS 2000u
#define NO_WIFI_FLASH_MS 100u
#define NO_WIFI_SECOND_FLASH_START_MS 300u
#define OK_PERIOD_MS 30000u
#define OK_FLASH_MS 50u

static bool square_wave(uint64_t now_ms, uint32_t half_period_ms)
{
    return (now_ms / half_period_ms) % 2 == 0;
}

status_t status_select(const status_inputs_t *inputs)
{
    if (inputs->resetting) {
        return STATUS_RESETTING;
    }
    if (inputs->sensor_fault) {
        return STATUS_SENSOR_FAULT;
    }
    if (!inputs->commissioned) {
        return STATUS_WAITING;
    }
    if (!inputs->wifi_connected) {
        return STATUS_NO_WIFI;
    }
    return inputs->has_first_readings ? STATUS_OK : STATUS_STARTING;
}

bool status_led_is_on(status_t status, uint64_t now_ms)
{
    switch (status) {
    case STATUS_RESETTING:
        return square_wave(now_ms, RESETTING_HALF_PERIOD_MS);
    case STATUS_SENSOR_FAULT:
        return square_wave(now_ms, FAULT_HALF_PERIOD_MS);
    case STATUS_WAITING:
        return square_wave(now_ms, WAITING_HALF_PERIOD_MS);
    case STATUS_NO_WIFI: {
        const uint32_t phase = (uint32_t)(now_ms % NO_WIFI_PERIOD_MS);
        return phase < NO_WIFI_FLASH_MS ||
               (phase >= NO_WIFI_SECOND_FLASH_START_MS && phase < NO_WIFI_SECOND_FLASH_START_MS + NO_WIFI_FLASH_MS);
    }
    case STATUS_STARTING:
        return true;
    case STATUS_OK:
        return now_ms % OK_PERIOD_MS < OK_FLASH_MS;
    }
    return false;
}
