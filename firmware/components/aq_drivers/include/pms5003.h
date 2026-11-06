// PMS5003 driver over UART. Not thread safe: call from one task.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t pms5003_init(int uart_num, int tx_gpio, int rx_gpio);

// Sleep stops the fan. After waking, the sensor needs 30 s before data is stable.
esp_err_t pms5003_set_awake(bool awake);

// Returns ESP_ERR_TIMEOUT when no valid frame arrives in time.
esp_err_t pms5003_read(uint16_t *pm25, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
