#include "demo_mode.h"

#include <array>
#include <cstddef>

#include <esp_log.h>
#include <esp_matter.h>

#include "boot_button.h"
#include "matter_app.h"

static const char *TAG = "demo_mode";

struct DemoState {
    const char *name;
    aq_level_t level;
    bool ventilate;
    bool dust;
    bool fault;
};

static constexpr std::array<DemoState, 4> STATES{{
    {"ventilate open, Poor", AQ_POOR, true, false, false},
    {"dust open, Poor", AQ_POOR, false, true, false},
    {"fault open, Unknown", AQ_UNKNOWN, false, false, true},
    {"all closed, Good", AQ_GOOD, false, false, false},
}};

static size_t s_next_state = 0;

static void apply(const DemoState &state)
{
    ESP_LOGI(TAG, "Demo state: %s", state.name);
    matter_app_set_level(state.level);
    matter_app_set_alerts(state.ventilate, state.dust, state.fault);
}

static void show_next_state()
{
    apply(STATES[s_next_state]);
    s_next_state = (s_next_state + 1) % STATES.size();
}

static void factory_reset()
{
    ESP_LOGW(TAG, "Factory reset");
    esp_matter::factory_reset();
}

esp_err_t demo_mode_start()
{
    matter_app_set_co2(650);
    matter_app_set_pm25(8);
    matter_app_set_temperature(22);
    matter_app_set_humidity(45);
    apply(STATES.back());
    return boot_button_start(show_next_state, factory_reset);
}
