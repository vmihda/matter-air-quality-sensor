#include "aq_engine.h"

#include <string.h>

#include "aq_config.h"

static void update_alert(aq_alert_t *alert, float value, float on, float off, uint64_t hold_ms, uint64_t now_ms)
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
    if (now_ms - alert->since_ms >= hold_ms) {
        alert->active = true;
    }
}

static void record(aq_channel_t *channel, float value, uint64_t now_ms)
{
    channel->value = value;
    channel->has_value = true;
    channel->last_ms = now_ms;
    channel->faulted = false;
}

static aq_level_t level_for(float value, float moderate, float poor)
{
    if (value >= poor) {
        return AQ_POOR;
    }
    return value >= moderate ? AQ_MODERATE : AQ_GOOD;
}

// Latched, so that later settings cannot make a dead sensor's old value look fresh again.
static bool update_stale(aq_channel_t *channel, uint64_t stale_ms, uint64_t now_ms)
{
    if (now_ms - channel->last_ms >= stale_ms) {
        channel->faulted = true;
    }
    if (channel->faulted) {
        memset(&channel->alert, 0, sizeof(channel->alert));
    }
    return channel->faulted;
}

void aq_engine_init(aq_engine_t *e, uint64_t now_ms)
{
    memset(e, 0, sizeof(*e));
    e->co2.last_ms = now_ms;
    e->pm25.last_ms = now_ms;
    aq_settings_defaults(&e->settings);
}

void aq_engine_set_settings(aq_engine_t *e, const aq_settings_t *s, uint64_t now_ms)
{
    if (s->co2_poor_ppm != e->settings.co2_poor_ppm) {
        e->co2.alert.timing = false;
    }
    if (s->pm25_poor != e->settings.pm25_poor) {
        e->pm25.alert.timing = false;
    }
    if (s->pms_cycle_s != e->settings.pms_cycle_s && !e->pm25.faulted) {
        e->pm25.last_ms = now_ms;
    }
    e->settings = *s;
}

void aq_engine_on_co2(aq_engine_t *e, float ppm, uint64_t now_ms)
{
    record(&e->co2, ppm, now_ms);
    update_alert(&e->co2.alert, ppm, e->settings.co2_poor_ppm, e->settings.co2_moderate_ppm, AQ_ALERT_HOLD_MS, now_ms);
}

void aq_engine_on_pm25(aq_engine_t *e, float ugm3, uint64_t now_ms)
{
    record(&e->pm25, ugm3, now_ms);
    update_alert(&e->pm25.alert, ugm3, e->settings.pm25_poor, e->settings.pm25_moderate, AQ_ALERT_HOLD_MS, now_ms);
}

aq_output_t aq_engine_evaluate(aq_engine_t *e, uint64_t now_ms)
{
    const uint64_t pm25_stale_ms = (uint64_t)e->settings.pms_cycle_s * AQ_PM25_STALE_CYCLES * 1000u;
    const bool co2_stale = update_stale(&e->co2, AQ_CO2_STALE_MS, now_ms);
    const bool pm25_stale = update_stale(&e->pm25, pm25_stale_ms, now_ms);

    aq_output_t out = {
        .level = AQ_UNKNOWN,
        .ventilate_alert = e->co2.alert.active,
        .dust_alert = e->pm25.alert.active,
        .fault_alert = co2_stale || pm25_stale,
        .co2_valid = e->co2.has_value && !co2_stale,
        .pm25_valid = e->pm25.has_value && !pm25_stale,
    };

    if (out.co2_valid && out.pm25_valid) {
        const aq_level_t co2_level = level_for(e->co2.value, e->settings.co2_moderate_ppm, e->settings.co2_poor_ppm);
        const aq_level_t pm25_level = level_for(e->pm25.value, e->settings.pm25_moderate, e->settings.pm25_poor);
        out.level = co2_level > pm25_level ? co2_level : pm25_level;
    }
    return out;
}
