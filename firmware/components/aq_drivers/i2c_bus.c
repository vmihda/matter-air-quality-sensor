#include "i2c_bus.h"

#include <stddef.h>

// The slowest rate of the sensors on this bus.
#define BUS_HZ 100000

static i2c_master_bus_handle_t s_bus;

esp_err_t aq_i2c_bus_get(int sda_gpio, int scl_gpio, i2c_master_bus_handle_t *bus)
{
    if (s_bus == NULL) {
        const i2c_master_bus_config_t config = {
            .i2c_port = -1,
            .sda_io_num = sda_gpio,
            .scl_io_num = scl_gpio,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,
        };
        const esp_err_t err = i2c_new_master_bus(&config, &s_bus);
        if (err != ESP_OK) {
            return err;
        }
    }
    *bus = s_bus;
    return ESP_OK;
}

esp_err_t aq_i2c_add_device(i2c_master_bus_handle_t bus, uint16_t address, i2c_master_dev_handle_t *device)
{
    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = BUS_HZ,
    };
    return i2c_master_bus_add_device(bus, &config, device);
}
