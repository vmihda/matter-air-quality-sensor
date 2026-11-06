// Service terminal over the board's USB port: status, settings and log level.
#pragma once

#include <esp_err.h>

[[nodiscard]] esp_err_t service_console_start();
