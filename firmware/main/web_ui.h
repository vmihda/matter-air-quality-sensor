// Device page over HTTP on the home network: status, settings and firmware upload.
#pragma once

#include <esp_err.h>

// Does nothing and logs an error when the unit has no page password configured.
[[nodiscard]] esp_err_t web_ui_start();
