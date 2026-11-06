// Turns sensor readings into an air quality level and alert states.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The enum values match the Matter AirQualityEnum.
typedef enum {
    AQ_UNKNOWN = 0,
    AQ_GOOD = 1,
    AQ_MODERATE = 3,
    AQ_POOR = 4,
} aq_level_t;

typedef struct {
    aq_level_t level;
    bool ventilate_open;
    bool dust_open;
    bool fault_open;
} aq_output_t;

typedef struct {
    bool active;
    bool timing;
    uint64_t since_ms;
} aq_alert_t;

typedef struct {
    float value;
    bool has_value;
    uint64_t last_ms;
    aq_alert_t alert;
} aq_channel_t;

typedef struct aq_engine {
    aq_channel_t co2;
    aq_channel_t pm25;
} aq_engine_t;

void aq_engine_init(aq_engine_t *e, uint64_t now_ms);
void aq_engine_on_co2(aq_engine_t *e, float ppm, uint64_t now_ms);
void aq_engine_on_pm25(aq_engine_t *e, float ugm3, uint64_t now_ms);
aq_output_t aq_engine_evaluate(aq_engine_t *e, uint64_t now_ms);

#ifdef __cplusplus
}
#endif
