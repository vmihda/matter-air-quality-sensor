// Current settings, kept in NVS so they survive a power loss. Safe to use from any task.
#pragma once

#include <esp_err.h>

#include "aq_settings.h"

[[nodiscard]] esp_err_t settings_store_init();
void settings_store_get(aq_settings_t *out);

// Returns ESP_ERR_INVALID_ARG when the values fail validation.
[[nodiscard]] esp_err_t settings_store_set(const aq_settings_t *settings);
