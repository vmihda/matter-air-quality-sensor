#include "demo_mode.h"

#include <array>
#include <atomic>
#include <cstddef>

#include <esp_log.h>
#include <esp_matter.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "boot_button.h"
#include "matter_app.h"
#include "sensors_task.h"

static const char *TAG = "demo_mode";

struct DemoState {
    const char *name;
    aq_level_t level;
    float co2_ppm;
    float pm25_ugm3;
    float tvoc_ugm3;
    float pm1_ugm3;
    float pm10_ugm3;
    bool ventilate;
    bool dust;
    bool fault;
};

static constexpr std::array<DemoState, 4> STATES{{
    {"CO2 high, Poor", AQ_POOR, 1420, 12, 310, 7, 18, true, false, false},
    {"dust high, Poor", AQ_POOR, 700, 96, 240, 61, 130, false, true, false},
    {"sensor fault, Unknown", AQ_UNKNOWN, 700, 8, 120, 5, 12, false, false, true},
    {"all fine, Good", AQ_GOOD, 650, 8, 120, 5, 12, false, false, false},
}};

static constexpr float DEMO_TEMPERATURE_C = 22;
static constexpr float DEMO_HUMIDITY_PCT = 45;

// Long enough to see each state change in Apple Home, including a slow subscription report.
static constexpr TickType_t AUTO_STEP_PERIOD = pdMS_TO_TICKS(40000);
static constexpr uint32_t AUTO_STEP_STACK_BYTES = 4096;
static constexpr UBaseType_t AUTO_STEP_PRIORITY = 3;

// Advanced from both the button task and the automatic step task.
static std::atomic<size_t> s_next_state{0};

static void apply(const DemoState &state)
{
    ESP_LOGI(TAG, "Demo state: %s", state.name);
    matter_app_set_level(state.level);

    SensorsStatus status{};
    status.co2_ppm = state.co2_ppm;
    status.pm1_ugm3 = state.pm1_ugm3;
    status.pm25_ugm3 = state.pm25_ugm3;
    status.pm10_ugm3 = state.pm10_ugm3;
    status.tvoc_ugm3 = state.tvoc_ugm3;
    status.has_tvoc = !state.fault;
    status.temperature_c = DEMO_TEMPERATURE_C;
    status.humidity_pct = DEMO_HUMIDITY_PCT;
    status.has_scd41 = !state.fault;
    status.has_pms5003 = !state.fault;
    status.output.level = state.level;
    status.output.ventilate_alert = state.ventilate;
    status.output.dust_alert = state.dust;
    status.output.fault_alert = state.fault;
    sensors_task_set_status_for_demo(status);
}

static void show_next_state()
{
    apply(STATES[s_next_state.fetch_add(1) % STATES.size()]);
}

// Boards without a reachable BOOT button still go through every state.
static void auto_step_task(void *)
{
    while (true) {
        vTaskDelay(AUTO_STEP_PERIOD);
        show_next_state();
    }
}

static void factory_reset()
{
    ESP_LOGW(TAG, "Factory reset");
    esp_matter::factory_reset();
}

esp_err_t demo_mode_start()
{
    apply(STATES.back());
    if (xTaskCreate(auto_step_task, "demo_step", AUTO_STEP_STACK_BYTES, nullptr, AUTO_STEP_PRIORITY, nullptr) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return boot_button_start(show_next_state, factory_reset);
}
