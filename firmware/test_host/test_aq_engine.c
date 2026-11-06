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

static void open_ventilate(aq_engine_t *e, uint64_t *t)
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
    CHECK(!out.fault_open);
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

static void ventilate_opens_after_hold(void)
{
    aq_engine_t e;
    aq_engine_init(&e, 0);
    CHECK(!feed(&e, 1000, 0, 0).ventilate_open);
    CHECK(!feed(&e, 1000, 0, 299999).ventilate_open);
    CHECK(feed(&e, 1000, 0, 300000).ventilate_open);
}

static void ventilate_timer_restarts_on_dip(void)
{
    aq_engine_t e;
    aq_engine_init(&e, 0);
    feed(&e, 1000, 0, 0);
    feed(&e, 999.9f, 0, 100000);
    feed(&e, 1000, 0, 200000);
    CHECK(!feed(&e, 1000, 0, 499999).ventilate_open);
    CHECK(feed(&e, 1000, 0, 500000).ventilate_open);
}

static void ventilate_stays_open_between_thresholds(void)
{
    aq_engine_t e;
    uint64_t t;
    open_ventilate(&e, &t);
    CHECK(feed(&e, 800, 0, t + 1).ventilate_open);
}

static void ventilate_closes_below_off(void)
{
    aq_engine_t e;
    uint64_t t;
    open_ventilate(&e, &t);
    CHECK(!feed(&e, 799.9f, 0, t + 1).ventilate_open);
}

static void ventilate_reopens_only_after_full_hold(void)
{
    aq_engine_t e;
    uint64_t t;
    open_ventilate(&e, &t);
    feed(&e, 700, 0, t + 1);
    t += 2;
    CHECK(!feed(&e, 1000, 0, t).ventilate_open);
    CHECK(!feed(&e, 1000, 0, t + 299999).ventilate_open);
    CHECK(feed(&e, 1000, 0, t + 300000).ventilate_open);
}

static void dust_uses_own_thresholds(void)
{
    aq_engine_t e;
    aq_engine_init(&e, 0);
    feed(&e, 400, 35, 0);
    CHECK(feed(&e, 400, 35, 300000).dust_open);
    CHECK(feed(&e, 400, 15, 300001).dust_open);
    CHECK(!feed(&e, 400, 14.9f, 300002).dust_open);
}

static void dust_does_not_open_ventilate(void)
{
    aq_engine_t e;
    aq_engine_init(&e, 0);
    feed(&e, 400, 35, 0);
    aq_output_t out = feed(&e, 400, 35, 300000);
    CHECK(out.dust_open);
    CHECK(!out.ventilate_open);
}

static void no_fault_during_startup_grace(void)
{
    aq_engine_t e;
    aq_engine_init(&e, 0);
    CHECK(!aq_engine_evaluate(&e, 59999).fault_open);
}

static void fault_after_co2_stale(void)
{
    aq_engine_t e;
    aq_engine_init(&e, 0);
    feed(&e, 1000, 0, 0);
    aq_engine_on_pm25(&e, 0, 60000);
    aq_output_t out = aq_engine_evaluate(&e, 60000);
    CHECK(out.fault_open);
    CHECK(out.level == AQ_UNKNOWN);
    CHECK(!out.ventilate_open);
}

static void fault_after_pm25_stale(void)
{
    aq_engine_t e;
    aq_engine_init(&e, 0);
    feed(&e, 400, 35, 0);
    aq_engine_on_co2(&e, 400, 300000);
    aq_output_t out = aq_engine_evaluate(&e, 300000);
    CHECK(out.fault_open);
    CHECK(!out.dust_open);
}

static void other_alert_survives_single_sensor_fault(void)
{
    aq_engine_t e;
    aq_engine_init(&e, 0);
    feed(&e, 1000, 0, 0);
    aq_engine_on_co2(&e, 1000, 300000);
    aq_output_t out = aq_engine_evaluate(&e, 300000);
    CHECK(out.ventilate_open);
    CHECK(out.fault_open);
}

static void recovery_clears_fault_and_restarts_hold(void)
{
    aq_engine_t e;
    uint64_t t;
    open_ventilate(&e, &t);
    aq_engine_on_pm25(&e, 0, t + 60000);
    CHECK(aq_engine_evaluate(&e, t + 60000).fault_open);
    aq_output_t out = feed(&e, 1000, 0, t + 60001);
    CHECK(!out.fault_open);
    CHECK(!out.ventilate_open);
    CHECK(!feed(&e, 1000, 0, t + 60001 + 299999).ventilate_open);
    CHECK(feed(&e, 1000, 0, t + 60001 + 300000).ventilate_open);
}

int main(void)
{
    RUN_TEST(level_unknown_until_both_sensors_report);
    RUN_TEST(co2_level_boundaries);
    RUN_TEST(pm25_level_boundaries);
    RUN_TEST(level_is_worst_of_both);
    RUN_TEST(ventilate_opens_after_hold);
    RUN_TEST(ventilate_timer_restarts_on_dip);
    RUN_TEST(ventilate_stays_open_between_thresholds);
    RUN_TEST(ventilate_closes_below_off);
    RUN_TEST(ventilate_reopens_only_after_full_hold);
    RUN_TEST(dust_uses_own_thresholds);
    RUN_TEST(dust_does_not_open_ventilate);
    RUN_TEST(no_fault_during_startup_grace);
    RUN_TEST(fault_after_co2_stale);
    RUN_TEST(fault_after_pm25_stale);
    RUN_TEST(other_alert_survives_single_sensor_fault);
    RUN_TEST(recovery_clears_fault_and_restarts_hold);
    return MINITEST_RESULT();
}
