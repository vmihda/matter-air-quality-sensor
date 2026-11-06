#include "aq_engine.h"
#include "minitest.h"

// Feeds both sensors at the same instant and evaluates immediately. Staleness is
// only checked during evaluation, so long gaps between calls do not count as faults.
static aq_output_t feed(aq_engine_t *e, float co2, float pm25, uint64_t t)
{
    aq_engine_on_co2(e, co2, t);
    aq_engine_on_pm25(e, pm25, t);
    return aq_engine_evaluate(e, t);
}

static void activate_ventilate(aq_engine_t *e, uint64_t *t)
{
    aq_engine_init(e, 0);
    feed(e, 1000, 0, 0);
    feed(e, 1000, 0, 300000);
    *t = 300000;
}

static void level_unknown_until_both_sensors_report(void)
{
    aq_engine_t e;
    aq_engine_init(&e, 0);
    aq_engine_on_co2(&e, 400, 0);
    aq_output_t out = aq_engine_evaluate(&e, 0);
    CHECK(out.level == AQ_UNKNOWN);
    CHECK(!out.fault_alert);
}

static void co2_level_boundaries(void)
{
    aq_engine_t e;
    aq_engine_init(&e, 0);
    CHECK(feed(&e, 799.9f, 0, 0).level == AQ_GOOD);
    CHECK(feed(&e, 800, 0, 0).level == AQ_MODERATE);
    CHECK(feed(&e, 999.9f, 0, 0).level == AQ_MODERATE);
    CHECK(feed(&e, 1000, 0, 0).level == AQ_POOR);
}

static void pm25_level_boundaries(void)
{
    aq_engine_t e;
    aq_engine_init(&e, 0);
    CHECK(feed(&e, 400, 14.9f, 0).level == AQ_GOOD);
    CHECK(feed(&e, 400, 15, 0).level == AQ_MODERATE);
    CHECK(feed(&e, 400, 34.9f, 0).level == AQ_MODERATE);
    CHECK(feed(&e, 400, 35, 0).level == AQ_POOR);
}

static void level_is_worst_of_both(void)
{
    aq_engine_t e;
    aq_engine_init(&e, 0);
    CHECK(feed(&e, 1000, 15, 0).level == AQ_POOR);
    CHECK(feed(&e, 800, 35, 0).level == AQ_POOR);
}

static void ventilate_activates_after_hold(void)
{
    aq_engine_t e;
    aq_engine_init(&e, 0);
    CHECK(!feed(&e, 1000, 0, 0).ventilate_alert);
    CHECK(!feed(&e, 1000, 0, 299999).ventilate_alert);
    CHECK(feed(&e, 1000, 0, 300000).ventilate_alert);
}

static void ventilate_timer_restarts_on_dip(void)
{
    aq_engine_t e;
    aq_engine_init(&e, 0);
    feed(&e, 1000, 0, 0);
    feed(&e, 999.9f, 0, 100000);
    feed(&e, 1000, 0, 200000);
    CHECK(!feed(&e, 1000, 0, 499999).ventilate_alert);
    CHECK(feed(&e, 1000, 0, 500000).ventilate_alert);
}

static void ventilate_stays_active_between_thresholds(void)
{
    aq_engine_t e;
    uint64_t t;
    activate_ventilate(&e, &t);
    CHECK(feed(&e, 800, 0, t + 1).ventilate_alert);
}

static void ventilate_clears_below_off(void)
{
    aq_engine_t e;
    uint64_t t;
    activate_ventilate(&e, &t);
    CHECK(!feed(&e, 799.9f, 0, t + 1).ventilate_alert);
}

static void ventilate_reactivates_only_after_full_hold(void)
{
    aq_engine_t e;
    uint64_t t;
    activate_ventilate(&e, &t);
    feed(&e, 700, 0, t + 1);
    t += 2;
    CHECK(!feed(&e, 1000, 0, t).ventilate_alert);
    CHECK(!feed(&e, 1000, 0, t + 299999).ventilate_alert);
    CHECK(feed(&e, 1000, 0, t + 300000).ventilate_alert);
}

static void dust_uses_own_thresholds(void)
{
    aq_engine_t e;
    aq_engine_init(&e, 0);
    feed(&e, 400, 35, 0);
    CHECK(feed(&e, 400, 35, 300000).dust_alert);
    CHECK(feed(&e, 400, 15, 300001).dust_alert);
    CHECK(!feed(&e, 400, 14.9f, 300002).dust_alert);
}

static void dust_does_not_activate_ventilate(void)
{
    aq_engine_t e;
    aq_engine_init(&e, 0);
    feed(&e, 400, 35, 0);
    aq_output_t out = feed(&e, 400, 35, 300000);
    CHECK(out.dust_alert);
    CHECK(!out.ventilate_alert);
}

static void no_fault_during_startup_grace(void)
{
    aq_engine_t e;
    aq_engine_init(&e, 0);
    CHECK(!aq_engine_evaluate(&e, 59999).fault_alert);
}

static void fault_after_co2_stale(void)
{
    aq_engine_t e;
    aq_engine_init(&e, 0);
    feed(&e, 1000, 0, 0);
    aq_engine_on_pm25(&e, 0, 60000);
    aq_output_t out = aq_engine_evaluate(&e, 60000);
    CHECK(out.fault_alert);
    CHECK(out.level == AQ_UNKNOWN);
    CHECK(!out.ventilate_alert);
}

static void fault_after_pm25_stale(void)
{
    aq_engine_t e;
    aq_engine_init(&e, 0);
    feed(&e, 400, 35, 0);
    aq_engine_on_co2(&e, 400, 899999);
    CHECK(!aq_engine_evaluate(&e, 899999).fault_alert);
    aq_engine_on_co2(&e, 400, 900000);
    aq_output_t out = aq_engine_evaluate(&e, 900000);
    CHECK(out.fault_alert);
    CHECK(!out.dust_alert);
}

static void other_alert_survives_single_sensor_fault(void)
{
    aq_engine_t e;
    aq_engine_init(&e, 0);
    feed(&e, 1000, 0, 0);
    aq_engine_on_co2(&e, 1000, 900000);
    aq_output_t out = aq_engine_evaluate(&e, 900000);
    CHECK(out.ventilate_alert);
    CHECK(out.fault_alert);
}

static void recovery_clears_fault_and_restarts_hold(void)
{
    aq_engine_t e;
    uint64_t t;
    activate_ventilate(&e, &t);
    aq_engine_on_pm25(&e, 0, t + 60000);
    CHECK(aq_engine_evaluate(&e, t + 60000).fault_alert);
    aq_output_t out = feed(&e, 1000, 0, t + 60001);
    CHECK(!out.fault_alert);
    CHECK(!out.ventilate_alert);
    CHECK(!feed(&e, 1000, 0, t + 60001 + 299999).ventilate_alert);
    CHECK(feed(&e, 1000, 0, t + 60001 + 300000).ventilate_alert);
}

static void no_sensor_is_valid_before_its_first_reading(void)
{
    aq_engine_t e;
    aq_engine_init(&e, 0);
    const aq_output_t out = aq_engine_evaluate(&e, 0);
    CHECK(!out.co2_valid);
    CHECK(!out.pm25_valid);
}

static void output_names_the_valid_sensors(void)
{
    aq_engine_t e;
    aq_engine_init(&e, 0);
    feed(&e, 400, 0, 0);
    aq_engine_on_pm25(&e, 0, 60000);
    aq_output_t out = aq_engine_evaluate(&e, 60000);
    CHECK(!out.co2_valid);
    CHECK(out.pm25_valid);
    aq_engine_on_co2(&e, 400, 60001);
    out = aq_engine_evaluate(&e, 60001);
    CHECK(out.co2_valid);
    CHECK(!out.fault_alert);
}

static void pm25_interval_change_keeps_an_established_fault(void)
{
    aq_engine_t e;
    aq_engine_init(&e, 0);
    aq_settings_t settings = e.settings;
    feed(&e, 400, 5, 0);
    aq_engine_on_co2(&e, 400, 900000);
    CHECK(!aq_engine_evaluate(&e, 900000).pm25_valid);

    settings.pms_cycle_s = 600;
    aq_engine_set_settings(&e, &settings, 900001);
    aq_engine_on_co2(&e, 400, 900002);
    const aq_output_t out = aq_engine_evaluate(&e, 900002);
    CHECK(!out.pm25_valid);
    CHECK(out.fault_alert);
    CHECK(out.level == AQ_UNKNOWN);
}

static void pm25_sample_clears_a_fault_after_interval_change(void)
{
    aq_engine_t e;
    aq_engine_init(&e, 0);
    aq_settings_t settings = e.settings;
    feed(&e, 400, 5, 0);
    aq_engine_on_co2(&e, 400, 900000);
    aq_engine_evaluate(&e, 900000);
    settings.pms_cycle_s = 600;
    aq_engine_set_settings(&e, &settings, 900001);
    const aq_output_t out = feed(&e, 400, 5, 900002);
    CHECK(out.pm25_valid);
    CHECK(out.level == AQ_GOOD);
}

static void pm25_interval_change_gives_a_healthy_sensor_time(void)
{
    aq_engine_t e;
    aq_engine_init(&e, 0);
    aq_settings_t settings = e.settings;
    settings.pms_cycle_s = 3600;
    aq_engine_set_settings(&e, &settings, 0);
    feed(&e, 400, 5, 0);
    aq_engine_on_co2(&e, 400, 3000000);
    CHECK(!aq_engine_evaluate(&e, 3000000).fault_alert);

    settings.pms_cycle_s = 120;
    aq_engine_set_settings(&e, &settings, 3000000);
    aq_engine_on_co2(&e, 400, 3000001);
    CHECK(!aq_engine_evaluate(&e, 3000001).fault_alert);
}

int main(void)
{
    RUN_TEST(level_unknown_until_both_sensors_report);
    RUN_TEST(co2_level_boundaries);
    RUN_TEST(pm25_level_boundaries);
    RUN_TEST(level_is_worst_of_both);
    RUN_TEST(ventilate_activates_after_hold);
    RUN_TEST(ventilate_timer_restarts_on_dip);
    RUN_TEST(ventilate_stays_active_between_thresholds);
    RUN_TEST(ventilate_clears_below_off);
    RUN_TEST(ventilate_reactivates_only_after_full_hold);
    RUN_TEST(dust_uses_own_thresholds);
    RUN_TEST(dust_does_not_activate_ventilate);
    RUN_TEST(no_fault_during_startup_grace);
    RUN_TEST(fault_after_co2_stale);
    RUN_TEST(fault_after_pm25_stale);
    RUN_TEST(other_alert_survives_single_sensor_fault);
    RUN_TEST(recovery_clears_fault_and_restarts_hold);
    RUN_TEST(output_names_the_valid_sensors);
    RUN_TEST(no_sensor_is_valid_before_its_first_reading);
    RUN_TEST(pm25_interval_change_keeps_an_established_fault);
    RUN_TEST(pm25_sample_clears_a_fault_after_interval_change);
    RUN_TEST(pm25_interval_change_gives_a_healthy_sensor_time);
    return MINITEST_RESULT();
}
