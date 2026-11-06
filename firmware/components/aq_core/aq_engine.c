#include "aq_engine.h"

#include <string.h>

#include "aq_config.h"

static void update_alert(aq_alert_t *alert, float value, float on, float off, uint64_t now_ms)
{
    if (alert->active) {
        if (value < off) {
            memset(alert, 0, sizeof(*alert));
        }
        return;
    }
    if (value < on) {
        alert->timing = false;
        return;
    }
    if (!alert->timing) {
        alert->timing = true;
        alert->since_ms = now_ms;
    }
    if (now_ms - alert->since_ms >= AQ_ALERT_HOLD_MS) {
        alert->active = true;
    }
}

static void record(aq_channel_t *channel, float value, uint64_t now_ms)
{
    channel->value = value;
    channel->has_value = true;
    channel->last_ms = now_ms;
}

static aq_level_t level_for(float value, float moderate, float poor)
{
    if (value >= poor) {
        return AQ_POOR;
    }
    return value >= moderate ? AQ_MODERATE : AQ_GOOD;
}

static bool is_stale(const aq_channel_t *channel, uint32_t stale_ms, uint64_t now_ms)
{
    return now_ms - channel->last_ms >= stale_ms;
}

static void reset_alert_if_stale(aq_channel_t *channel, bool stale)
{
    if (stale) {
        memset(&channel->alert, 0, sizeof(channel->alert));
    }
}

void aq_engine_init(aq_engine_t *e, uint64_t now_ms)
{
    memset(e, 0, sizeof(*e));
    e->co2.last_ms = now_ms;
    e->pm25.last_ms = now_ms;
}

void aq_engine_on_co2(aq_engine_t *e, float ppm, uint64_t now_ms)
{
    record(&e->co2, ppm, now_ms);
    update_alert(&e->co2.alert, ppm, AQ_CO2_POOR_PPM, AQ_CO2_ALERT_OFF_PPM, now_ms);
}

void aq_engine_on_pm25(aq_engine_t *e, float ugm3, uint64_t now_ms)
{
    record(&e->pm25, ugm3, now_ms);
    update_alert(&e->pm25.alert, ugm3, AQ_PM25_POOR_UGM3, AQ_PM25_ALERT_OFF_UGM3, now_ms);
}

aq_output_t aq_engine_evaluate(aq_engine_t *e, uint64_t now_ms)
{
    const bool co2_stale = is_stale(&e->co2, AQ_CO2_STALE_MS, now_ms);
    const bool pm25_stale = is_stale(&e->pm25, AQ_PM25_STALE_MS, now_ms);
    reset_alert_if_stale(&e->co2, co2_stale);
    reset_alert_if_stale(&e->pm25, pm25_stale);

    aq_output_t out = {
        .level = AQ_UNKNOWN,
        .ventilate_open = e->co2.alert.active,
        .dust_open = e->pm25.alert.active,
        .fault_open = co2_stale || pm25_stale,
    };

    if (!out.fault_open && e->co2.has_value && e->pm25.has_value) {
        const aq_level_t co2_level = level_for(e->co2.value, AQ_CO2_MODERATE_PPM, AQ_CO2_POOR_PPM);
        const aq_level_t pm25_level = level_for(e->pm25.value, AQ_PM25_MODERATE_UGM3, AQ_PM25_POOR_UGM3);
        out.level = co2_level > pm25_level ? co2_level : pm25_level;
    }
    return out;
}
