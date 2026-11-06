#include "minitest.h"
#include "status_policy.h"

static void resetting_wins_over_everything(void)
{
    const status_inputs_t inputs = {.resetting = true, .sensor_fault = true, .commissioned = true};
    CHECK(status_select(&inputs) == STATUS_RESETTING);
}

static void fault_wins_over_no_wifi(void)
{
    const status_inputs_t inputs = {.sensor_fault = true, .commissioned = true};
    CHECK(status_select(&inputs) == STATUS_SENSOR_FAULT);
}

static void no_wifi_only_after_commissioning(void)
{
    const status_inputs_t inputs = {.commissioned = true};
    CHECK(status_select(&inputs) == STATUS_NO_WIFI);
}

static void waiting_before_commissioning(void)
{
    const status_inputs_t inputs = {.commissioned = false};
    CHECK(status_select(&inputs) == STATUS_WAITING);
}

static void starting_until_first_readings(void)
{
    const status_inputs_t inputs = {.commissioned = true, .wifi_connected = true};
    CHECK(status_select(&inputs) == STATUS_STARTING);
}

static void ok_when_everything_works(void)
{
    const status_inputs_t inputs = {.commissioned = true, .wifi_connected = true, .has_first_readings = true};
    CHECK(status_select(&inputs) == STATUS_OK);
}

static void starting_is_always_on(void)
{
    CHECK(status_led_is_on(STATUS_STARTING, 0));
    CHECK(status_led_is_on(STATUS_STARTING, 123456));
}

static void waiting_blinks_once_a_second(void)
{
    CHECK(status_led_is_on(STATUS_WAITING, 0));
    CHECK(status_led_is_on(STATUS_WAITING, 499));
    CHECK(!status_led_is_on(STATUS_WAITING, 500));
    CHECK(!status_led_is_on(STATUS_WAITING, 999));
    CHECK(status_led_is_on(STATUS_WAITING, 1000));
}

static void no_wifi_shows_two_flashes_every_two_seconds(void)
{
    CHECK(status_led_is_on(STATUS_NO_WIFI, 50));
    CHECK(!status_led_is_on(STATUS_NO_WIFI, 200));
    CHECK(status_led_is_on(STATUS_NO_WIFI, 350));
    CHECK(!status_led_is_on(STATUS_NO_WIFI, 500));
    CHECK(!status_led_is_on(STATUS_NO_WIFI, 1999));
    CHECK(status_led_is_on(STATUS_NO_WIFI, 2050));
}

static void fault_blinks_fast(void)
{
    CHECK(status_led_is_on(STATUS_SENSOR_FAULT, 0));
    CHECK(!status_led_is_on(STATUS_SENSOR_FAULT, 250));
    CHECK(status_led_is_on(STATUS_SENSOR_FAULT, 500));
}

static void resetting_blinks_faster_than_fault(void)
{
    CHECK(status_led_is_on(STATUS_RESETTING, 0));
    CHECK(!status_led_is_on(STATUS_RESETTING, 50));
    CHECK(status_led_is_on(STATUS_RESETTING, 100));
}

static void ok_flashes_briefly_every_thirty_seconds(void)
{
    CHECK(status_led_is_on(STATUS_OK, 0));
    CHECK(status_led_is_on(STATUS_OK, 49));
    CHECK(!status_led_is_on(STATUS_OK, 50));
    CHECK(!status_led_is_on(STATUS_OK, 29999));
    CHECK(status_led_is_on(STATUS_OK, 30000));
}

int main(void)
{
    RUN_TEST(resetting_wins_over_everything);
    RUN_TEST(fault_wins_over_no_wifi);
    RUN_TEST(no_wifi_only_after_commissioning);
    RUN_TEST(waiting_before_commissioning);
    RUN_TEST(starting_until_first_readings);
    RUN_TEST(ok_when_everything_works);
    RUN_TEST(starting_is_always_on);
    RUN_TEST(waiting_blinks_once_a_second);
    RUN_TEST(no_wifi_shows_two_flashes_every_two_seconds);
    RUN_TEST(fault_blinks_fast);
    RUN_TEST(resetting_blinks_faster_than_fault);
    RUN_TEST(ok_flashes_briefly_every_thirty_seconds);
    return MINITEST_RESULT();
}
