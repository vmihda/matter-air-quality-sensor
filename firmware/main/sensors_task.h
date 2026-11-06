// Owns the sensors: reads them, runs the alert engine and publishes the level and measurements to Matter.
#pragma once

#include <esp_err.h>
#include <sdkconfig.h>

#include "aq_engine.h"

struct SensorsStatus {
    float co2_ppm;
    float pm1_ugm3;
    float pm25_ugm3;
    float pm10_ugm3;
    float tvoc_ugm3;
    float temperature_c;
    float humidity_pct;
    bool has_scd41;
    bool has_pms5003;
    bool has_tvoc;
    bool tvoc_warming_up;
    aq_output_t output;
};

[[nodiscard]] esp_err_t sensors_task_start();

// Safe to call from any task.
void sensors_task_get_status(SensorsStatus *out);
#if CONFIG_AQ_DEMO_VALUES
// Lets the demo firmware run the real publish path and device page without sensors.
void sensors_task_set_status_for_demo(const SensorsStatus &status);
#endif
[[nodiscard]] bool sensors_task_has_fault();
[[nodiscard]] bool sensors_task_has_first_readings();
[[nodiscard]] bool sensors_task_has_any_reading();
