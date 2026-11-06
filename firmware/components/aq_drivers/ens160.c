#include "ens160.h"

#include "esp_check.h"
#include "i2c_bus.h"

// Modules select one of the two addresses with a pin, and most use 0x53.
#define ENS160_ADDRESS_DEFAULT 0x53
#define ENS160_ADDRESS_ALTERNATE 0x52
#define ENS160_TIMEOUT_MS 100
#define MODE_CHANGE_DELAY_MS 10

static const char *TAG = "ens160";

static i2c_master_dev_handle_t s_device;

static esp_err_t read_registers(uint8_t address, uint8_t *data, size_t length)
{
    return i2c_master_transmit_receive(s_device, &address, 1, data, length, ENS160_TIMEOUT_MS);
}

static esp_err_t write_register(uint8_t address, uint8_t value)
{
    const uint8_t bytes[2] = {address, value};
    return i2c_master_transmit(s_device, bytes, sizeof(bytes), ENS160_TIMEOUT_MS);
}

static esp_err_t set_mode(uint8_t mode)
{
    ESP_RETURN_ON_ERROR(write_register(ENS160_REG_OPMODE, mode), TAG, "mode write failed");
    AQ_WAIT_MS(MODE_CHANGE_DELAY_MS);
    return ESP_OK;
}

static esp_err_t create_device(int sda_gpio, int scl_gpio)
{
    if (s_device != NULL) {
        return ESP_OK;
    }
    i2c_master_bus_handle_t bus = NULL;
    ESP_RETURN_ON_ERROR(aq_i2c_bus_get(sda_gpio, scl_gpio, &bus), TAG, "bus init failed");

    uint16_t address = ENS160_ADDRESS_DEFAULT;
    if (i2c_master_probe(bus, address, ENS160_TIMEOUT_MS) != ESP_OK) {
        address = ENS160_ADDRESS_ALTERNATE;
        if (i2c_master_probe(bus, address, ENS160_TIMEOUT_MS) != ESP_OK) {
            return ESP_ERR_NOT_FOUND;
        }
    }
    return aq_i2c_add_device(bus, address, &s_device);
}

esp_err_t ens160_init(int sda_gpio, int scl_gpio)
{
    const esp_err_t err = create_device(sda_gpio, scl_gpio);
    if (err != ESP_OK) {
        return err;
    }
    uint8_t part_id[2];
    ESP_RETURN_ON_ERROR(read_registers(ENS160_REG_PART_ID, part_id, sizeof(part_id)), TAG, "part id read failed");
    if (!ens160_is_part_id(part_id)) {
        return ESP_ERR_NOT_FOUND;
    }
    // The baseline is kept in the sensor's own memory, so no reset is needed here.
    ESP_RETURN_ON_ERROR(set_mode(ENS160_OPMODE_IDLE), TAG, "idle mode failed");
    return set_mode(ENS160_OPMODE_STANDARD);
}

esp_err_t ens160_set_ambient(float celsius, float percent)
{
    uint8_t frame[5];
    ens160_build_ambient_write(frame, celsius, percent);
    return i2c_master_transmit(s_device, frame, sizeof(frame), ENS160_TIMEOUT_MS);
}

esp_err_t ens160_read(ens160_reading_t *reading)
{
    uint8_t raw_status = 0;
    ESP_RETURN_ON_ERROR(read_registers(ENS160_REG_DEVICE_STATUS, &raw_status, 1), TAG, "status read failed");
    const ens160_status_t status = ens160_decode_status(raw_status);
    if (status.error || !status.running) {
        return ESP_ERR_INVALID_STATE;
    }
    reading->validity = status.validity;
    if (!status.new_data) {
        return ESP_ERR_NOT_FOUND;
    }
    uint8_t raw_tvoc[2];
    ESP_RETURN_ON_ERROR(read_registers(ENS160_REG_DATA_TVOC, raw_tvoc, sizeof(raw_tvoc)), TAG, "TVOC read failed");
    reading->tvoc_ppb = ens160_decode_tvoc_ppb(raw_tvoc);
    return ESP_OK;
}
