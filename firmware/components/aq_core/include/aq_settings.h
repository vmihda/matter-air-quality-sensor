// Settings that can change at runtime. Their defaults come from aq_config.h.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float co2_moderate_ppm;
    float co2_poor_ppm;
    float pm25_moderate;
    float pm25_poor;
    uint32_t pms_cycle_s;
    float temp_offset_c;
} aq_settings_t;

void aq_settings_defaults(aq_settings_t *s);

// False for anything outside the allowed ranges, including NaN.
bool aq_settings_validate(const aq_settings_t *s);

// Parses an application/x-www-form-urlencoded body. Every field is required and the result is validated.
bool aq_settings_from_form(const char *body, aq_settings_t *out);

// Whether the PMS5003 should wake now. Uses the current interval, so a change applies at once.
bool aq_pms_wake_due(bool woke_before, uint64_t last_wake_ms, uint32_t cycle_s, uint64_t now_ms);

#ifdef __cplusplus
}
#endif
