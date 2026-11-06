// Owns the sensors: reads them, runs the alert engine and publishes the results to Matter.
#pragma once

#include <esp_err.h>

[[nodiscard]] esp_err_t sensors_task_start();

// Safe to call from any task.
[[nodiscard]] bool sensors_task_has_fault();
[[nodiscard]] bool sensors_task_has_first_readings();
