// Confirms a freshly updated firmware, or rolls back if it never comes up properly.
#pragma once

#include <esp_err.h>

[[nodiscard]] esp_err_t ota_guard_start();
