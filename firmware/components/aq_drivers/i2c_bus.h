// The I2C bus shared by the sensors. Not thread safe: call from one task.
#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// pdMS_TO_TICKS rounds down and a delay can end right after it starts, so two extra
// ticks guarantee at least the requested time at any tick rate.
#define AQ_WAIT_MS(ms) vTaskDelay(pdMS_TO_TICKS(ms) + 2)

// Creates the bus on the first call and returns the same bus afterwards.
esp_err_t aq_i2c_bus_get(int sda_gpio, int scl_gpio, i2c_master_bus_handle_t *bus);

esp_err_t aq_i2c_add_device(i2c_master_bus_handle_t bus, uint16_t address, i2c_master_dev_handle_t *device);
