#include <stdio.h>

#include "aq_engine.h"
#include "aq_settings.h"
#include "minitest.h"

static aq_settings_t valid_settings(void)
{
    aq_settings_t s;
    aq_settings_defaults(&s);
    return s;
}

static void defaults_match_the_spec(void)
{
    const aq_settings_t s = valid_settings();
    CHECK_NEAR(s.co2_moderate_ppm, 800, 0.001);
    CHECK_NEAR(s.co2_poor_ppm, 1000, 0.001);
    CHECK_NEAR(s.pm25_moderate, 15, 0.001);
    CHECK_NEAR(s.pm25_poor, 35, 0.001);
    CHECK(s.pms_cycle_s == 300);
    CHECK_NEAR(s.temp_offset_c, 4, 0.001);
    CHECK(aq_settings_validate(&s));
}

static void rejects_cycle_out_of_range(void)
{
    aq_settings_t s = valid_settings();
    s.pms_cycle_s = 119;
    CHECK(!aq_settings_validate(&s));
    s.pms_cycle_s = 120;
    CHECK(aq_settings_validate(&s));
    s.pms_cycle_s = 3600;
    CHECK(aq_settings_validate(&s));
    s.pms_cycle_s = 3601;
    CHECK(!aq_settings_validate(&s));
}

static void rejects_thresholds_out_of_order(void)
{
    aq_settings_t s = valid_settings();
    s.co2_moderate_ppm = 1100;
    CHECK(!aq_settings_validate(&s));
    s = valid_settings();
    s.pm25_moderate = 40;
    CHECK(!aq_settings_validate(&s));
}

static void rejects_values_out_of_range(void)
{
    aq_settings_t s = valid_settings();
    s.co2_moderate_ppm = 399;
    CHECK(!aq_settings_validate(&s));
    s = valid_settings();
    s.co2_poor_ppm = 5001;
    CHECK(!aq_settings_validate(&s));
    s = valid_settings();
    s.pm25_moderate = -1;
    CHECK(!aq_settings_validate(&s));
    s = valid_settings();
    s.pm25_poor = 1001;
    CHECK(!aq_settings_validate(&s));
    s = valid_settings();
    s.temp_offset_c = -0.1f;
    CHECK(!aq_settings_validate(&s));
    s = valid_settings();
    s.temp_offset_c = 20.1f;
    CHECK(!aq_settings_validate(&s));
}

static void rejects_not_a_number(void)
{
    aq_settings_t s = valid_settings();
    s.co2_poor_ppm = NAN;
    CHECK(!aq_settings_validate(&s));
}

static const char *const VALID_FORM =
    "co2_moderate=850&co2_poor=1100&pm25_moderate=12&pm25_poor=30"
    "&cycle_s=600&temp_offset=2.5";

static void parses_a_complete_form(void)
{
    aq_settings_t s;
    CHECK(aq_settings_from_form(VALID_FORM, &s));
    CHECK_NEAR(s.co2_moderate_ppm, 850, 0.001);
    CHECK_NEAR(s.co2_poor_ppm, 1100, 0.001);
    CHECK_NEAR(s.pm25_moderate, 12, 0.001);
    CHECK_NEAR(s.pm25_poor, 30, 0.001);
    CHECK(s.pms_cycle_s == 600);
    CHECK_NEAR(s.temp_offset_c, 2.5, 0.001);
}

static void form_rejects_missing_field(void)
{
    aq_settings_t s;
    CHECK(!aq_settings_from_form("co2_moderate=850&co2_poor=1100", &s));
}

static void form_rejects_garbage_number(void)
{
    aq_settings_t s;
    CHECK(!aq_settings_from_form(
        "co2_moderate=abc&co2_poor=1100&pm25_moderate=12&pm25_poor=30"
        "&cycle_s=600&temp_offset=2.5",
        &s));
}

static void form_rejects_trailing_garbage(void)
{
    aq_settings_t s;
    CHECK(!aq_settings_from_form(
        "co2_moderate=850x&co2_poor=1100&pm25_moderate=12&pm25_poor=30"
        "&cycle_s=600&temp_offset=2.5",
        &s));
}

static void form_rejects_negative_integer(void)
{
    aq_settings_t s;
    CHECK(!aq_settings_from_form(
        "co2_moderate=850&co2_poor=1100&pm25_moderate=12&pm25_poor=30"
        "&cycle_s=-600&temp_offset=2.5",
        &s));
}

static void form_rejects_invalid_ranges(void)
{
    aq_settings_t s;
    CHECK(!aq_settings_from_form(
        "co2_moderate=850&co2_poor=1100&pm25_moderate=12&pm25_poor=30"
        "&cycle_s=60&temp_offset=2.5",
        &s));
}

static void custom_threshold_applies(void)
{
    aq_engine_t e;
    aq_settings_t s = valid_settings();
    s.co2_poor_ppm = 1200;
    aq_engine_init(&e, 0);
    aq_engine_set_settings(&e, &s, 0);
    aq_engine_on_pm25(&e, 0, 0);
    aq_engine_on_co2(&e, 1000, 0);
    CHECK(aq_engine_evaluate(&e, 0).level == AQ_MODERATE);
    aq_engine_on_co2(&e, 1200, 0);
    CHECK(aq_engine_evaluate(&e, 0).level == AQ_POOR);
}

static void alert_clears_below_the_custom_moderate_level(void)
{
    aq_engine_t e;
    aq_settings_t s = valid_settings();
    s.co2_moderate_ppm = 700;
    aq_engine_init(&e, 0);
    aq_engine_set_settings(&e, &s, 0);
    aq_engine_on_pm25(&e, 0, 0);
    aq_engine_on_co2(&e, 1000, 0);
    aq_engine_on_pm25(&e, 0, 300000);
    aq_engine_on_co2(&e, 1000, 300000);
    CHECK(aq_engine_evaluate(&e, 300000).ventilate_alert);
    aq_engine_on_co2(&e, 700, 300001);
    CHECK(aq_engine_evaluate(&e, 300001).ventilate_alert);
    aq_engine_on_co2(&e, 699, 300002);
    CHECK(!aq_engine_evaluate(&e, 300002).ventilate_alert);
}

static void stale_limit_follows_cycle(void)
{
    aq_engine_t e;
    aq_settings_t s = valid_settings();
    s.pms_cycle_s = 120;
    aq_engine_init(&e, 0);
    aq_engine_set_settings(&e, &s, 0);
    aq_engine_on_pm25(&e, 0, 0);
    aq_engine_on_co2(&e, 400, 359999);
    CHECK(!aq_engine_evaluate(&e, 359999).fault_alert);
    aq_engine_on_co2(&e, 400, 360000);
    CHECK(aq_engine_evaluate(&e, 360000).fault_alert);
}

static void threshold_change_restarts_pending_timer(void)
{
    aq_engine_t e;
    aq_settings_t s = valid_settings();
    aq_engine_init(&e, 0);
    aq_engine_on_pm25(&e, 0, 0);
    aq_engine_on_co2(&e, 1100, 0);
    s.co2_poor_ppm = 1200;
    aq_engine_set_settings(&e, &s, 295000);
    aq_engine_on_pm25(&e, 0, 300000);
    aq_engine_on_co2(&e, 1300, 300000);
    CHECK(!aq_engine_evaluate(&e, 300000).ventilate_alert);
    aq_engine_on_pm25(&e, 0, 600000);
    aq_engine_on_co2(&e, 1300, 600000);
    CHECK(aq_engine_evaluate(&e, 600000).ventilate_alert);
}

static void unrelated_change_keeps_pending_timer(void)
{
    aq_engine_t e;
    aq_settings_t s = valid_settings();
    aq_engine_init(&e, 0);
    aq_engine_on_pm25(&e, 0, 0);
    aq_engine_on_co2(&e, 1100, 0);
    s.pm25_poor = 40;
    aq_engine_set_settings(&e, &s, 295000);
    aq_engine_on_pm25(&e, 0, 300000);
    aq_engine_on_co2(&e, 1100, 300000);
    CHECK(aq_engine_evaluate(&e, 300000).ventilate_alert);
}

static void threshold_change_keeps_active_alert(void)
{
    aq_engine_t e;
    aq_settings_t s = valid_settings();
    aq_engine_init(&e, 0);
    aq_engine_on_pm25(&e, 0, 0);
    aq_engine_on_co2(&e, 1100, 0);
    aq_engine_on_pm25(&e, 0, 300000);
    aq_engine_on_co2(&e, 1100, 300000);
    s.co2_poor_ppm = 1200;
    aq_engine_set_settings(&e, &s, 300000);
    CHECK(aq_engine_evaluate(&e, 300000).ventilate_alert);
}

static void shorter_cycle_does_not_fault_a_healthy_sensor(void)
{
    aq_engine_t e;
    aq_settings_t s = valid_settings();
    s.pms_cycle_s = 3600;
    aq_engine_init(&e, 0);
    aq_engine_set_settings(&e, &s, 0);
    aq_engine_on_pm25(&e, 0, 0);
    s.pms_cycle_s = 120;
    aq_engine_set_settings(&e, &s, 3000000);
    aq_engine_on_co2(&e, 400, 3000000);
    CHECK(!aq_engine_evaluate(&e, 3000000).fault_alert);
    aq_engine_on_co2(&e, 400, 3359999);
    CHECK(!aq_engine_evaluate(&e, 3359999).fault_alert);
    aq_engine_on_co2(&e, 400, 3360000);
    CHECK(aq_engine_evaluate(&e, 3360000).fault_alert);
}

static void wake_is_due_before_the_first_reading(void)
{
    CHECK(aq_pms_wake_due(false, 0, 300, 0));
}

static void wake_follows_the_current_cycle(void)
{
    CHECK(!aq_pms_wake_due(true, 30000, 3600, 31000));
    CHECK(!aq_pms_wake_due(true, 30000, 120, 149999));
    CHECK(aq_pms_wake_due(true, 30000, 120, 150000));
}

int main(void)
{
    RUN_TEST(threshold_change_restarts_pending_timer);
    RUN_TEST(unrelated_change_keeps_pending_timer);
    RUN_TEST(threshold_change_keeps_active_alert);
    RUN_TEST(shorter_cycle_does_not_fault_a_healthy_sensor);
    RUN_TEST(wake_is_due_before_the_first_reading);
    RUN_TEST(wake_follows_the_current_cycle);
    RUN_TEST(defaults_match_the_spec);
    RUN_TEST(rejects_cycle_out_of_range);
    RUN_TEST(rejects_thresholds_out_of_order);
    RUN_TEST(rejects_values_out_of_range);
    RUN_TEST(rejects_not_a_number);
    RUN_TEST(parses_a_complete_form);
    RUN_TEST(form_rejects_missing_field);
    RUN_TEST(form_rejects_garbage_number);
    RUN_TEST(form_rejects_trailing_garbage);
    RUN_TEST(form_rejects_negative_integer);
    RUN_TEST(form_rejects_invalid_ranges);
    RUN_TEST(custom_threshold_applies);
    RUN_TEST(alert_clears_below_the_custom_moderate_level);
    RUN_TEST(stale_limit_follows_cycle);
    return MINITEST_RESULT();
}
