#include "aq_settings.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "aq_config.h"

#define CO2_MIN_PPM 400.0f
#define CO2_MAX_PPM 5000.0f
#define PM25_MIN_UGM3 0.0f
#define PM25_MAX_UGM3 1000.0f
#define TEMP_OFFSET_MIN_C 0.0f
#define TEMP_OFFSET_MAX_C 20.0f
#define CYCLE_MIN_S 120u
#define CYCLE_MAX_S 3600u
#define VALUE_BUFFER_SIZE 16

typedef enum { FIELD_FLOAT, FIELD_UINT } field_type_t;

typedef struct {
    const char *name;
    size_t offset;
    field_type_t type;
} field_t;

static const field_t FIELDS[] = {
    {"co2_moderate", offsetof(aq_settings_t, co2_moderate_ppm), FIELD_FLOAT},
    {"co2_poor", offsetof(aq_settings_t, co2_poor_ppm), FIELD_FLOAT},
    {"pm25_moderate", offsetof(aq_settings_t, pm25_moderate), FIELD_FLOAT},
    {"pm25_poor", offsetof(aq_settings_t, pm25_poor), FIELD_FLOAT},
    {"cycle_s", offsetof(aq_settings_t, pms_cycle_s), FIELD_UINT},
    {"temp_offset", offsetof(aq_settings_t, temp_offset_c), FIELD_FLOAT},
};
#define FIELD_COUNT (sizeof(FIELDS) / sizeof(FIELDS[0]))

// Written so that NaN fails the check.
static bool float_in_range(float value, float min, float max)
{
    return value >= min && value <= max;
}

static bool uint_in_range(uint32_t value, uint32_t min, uint32_t max)
{
    return value >= min && value <= max;
}

static const char *find_value(const char *body, const char *name, size_t *length)
{
    const size_t name_length = strlen(name);
    const char *cursor = body;
    while (*cursor != '\0') {
        const char *end = strchr(cursor, '&');
        const size_t pair_length = end != NULL ? (size_t)(end - cursor) : strlen(cursor);
        if (pair_length > name_length && strncmp(cursor, name, name_length) == 0 && cursor[name_length] == '=') {
            *length = pair_length - name_length - 1;
            return cursor + name_length + 1;
        }
        cursor += pair_length;
        if (*cursor == '&') {
            cursor++;
        }
    }
    return NULL;
}

static bool parse_field(const field_t *field, const char *text, aq_settings_t *out)
{
    char *end = NULL;
    void *target = (char *)out + field->offset;
    if (field->type == FIELD_FLOAT) {
        *(float *)target = strtof(text, &end);
    } else {
        // strtoul accepts a sign and would wrap a negative value around.
        if (text[0] < '0' || text[0] > '9') {
            return false;
        }
        *(uint32_t *)target = (uint32_t)strtoul(text, &end, 10);
    }
    return end != text && *end == '\0';
}

void aq_settings_defaults(aq_settings_t *s)
{
    s->co2_moderate_ppm = AQ_CO2_MODERATE_PPM;
    s->co2_poor_ppm = AQ_CO2_POOR_PPM;
    s->pm25_moderate = AQ_PM25_MODERATE_UGM3;
    s->pm25_poor = AQ_PM25_POOR_UGM3;
    s->pms_cycle_s = AQ_PMS_CYCLE_MS / 1000u;
    s->temp_offset_c = AQ_TEMP_OFFSET_C;
}

bool aq_settings_validate(const aq_settings_t *s)
{
    return float_in_range(s->co2_moderate_ppm, CO2_MIN_PPM, CO2_MAX_PPM) &&
           float_in_range(s->co2_poor_ppm, s->co2_moderate_ppm, CO2_MAX_PPM) &&
           float_in_range(s->pm25_moderate, PM25_MIN_UGM3, PM25_MAX_UGM3) &&
           float_in_range(s->pm25_poor, s->pm25_moderate, PM25_MAX_UGM3) &&
           float_in_range(s->temp_offset_c, TEMP_OFFSET_MIN_C, TEMP_OFFSET_MAX_C) &&
           uint_in_range(s->pms_cycle_s, CYCLE_MIN_S, CYCLE_MAX_S);
}

bool aq_pms_wake_due(bool woke_before, uint64_t last_wake_ms, uint32_t cycle_s, uint64_t now_ms)
{
    return !woke_before || now_ms - last_wake_ms >= (uint64_t)cycle_s * 1000u;
}

bool aq_settings_from_form(const char *body, aq_settings_t *out)
{
    aq_settings_t parsed;
    for (size_t i = 0; i < FIELD_COUNT; i++) {
        size_t length = 0;
        const char *value = find_value(body, FIELDS[i].name, &length);
        char text[VALUE_BUFFER_SIZE];
        if (value == NULL || length == 0 || length >= sizeof(text)) {
            return false;
        }
        memcpy(text, value, length);
        text[length] = '\0';
        if (!parse_field(&FIELDS[i], text, &parsed)) {
            return false;
        }
    }
    if (!aq_settings_validate(&parsed)) {
        return false;
    }
    *out = parsed;
    return true;
}
