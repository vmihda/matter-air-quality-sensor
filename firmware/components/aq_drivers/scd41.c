#include "scd41.h"

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "scd4x_proto.h"

#define SCD41_ADDRESS 0x62
#define SCD41_I2C_HZ 100000
#define SCD41_TIMEOUT_MS 100

#define CMD_START_PERIODIC 0x21B1
#define CMD_READ_MEASUREMENT 0xEC05
#define CMD_STOP_PERIODIC 0x3F86
#define CMD_DATA_READY 0xE4B8
#define CMD_GET_SERIAL 0x3682
#define CMD_SET_TEMP_OFFSET 0x241D

// Execution times from the SCD4x datasheet.
#define STOP_DELAY_MS 500
#define COMMAND_DELAY_MS 1

static const char *TAG = "scd41";

static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_device;

static esp_err_t send_command(uint16_t command)
{
    const uint8_t bytes[2] = {command >> 8, command & 0xFF};
    return i2c_master_transmit(s_device, bytes, sizeof(bytes), SCD41_TIMEOUT_MS);
}

static esp_err_t read_response(uint16_t command, uint8_t *response, size_t length)
{
    ESP_RETURN_ON_ERROR(send_command(command), TAG, "command 0x%04X failed", command);
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS) + 1);
    return i2c_master_receive(s_device, response, length, SCD41_TIMEOUT_MS);
}

esp_err_t scd41_init(int sda_gpio, int scl_gpio)
{
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = -1,
        .sda_io_num = sda_gpio,
        .scl_io_num = scl_gpio,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &s_bus), TAG, "bus init failed");

    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SCD41_ADDRESS,
        .scl_speed_hz = SCD41_I2C_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_bus, &device_config, &s_device), TAG, "device add failed");

    // The sensor keeps measuring across an MCU reset, and most commands need it idle.
    ESP_RETURN_ON_ERROR(send_command(CMD_STOP_PERIODIC), TAG, "no reply from SCD41, check wiring");
    vTaskDelay(pdMS_TO_TICKS(STOP_DELAY_MS));
    return ESP_OK;
}

esp_err_t scd41_get_serial(uint64_t *serial)
{
    uint8_t raw[9];
    ESP_RETURN_ON_ERROR(read_response(CMD_GET_SERIAL, raw, sizeof(raw)), TAG, "serial read failed");
    return scd4x_decode_serial(raw, serial) ? ESP_OK : ESP_ERR_INVALID_CRC;
}

esp_err_t scd41_set_temp_offset(float offset_c)
{
    const uint16_t word = (uint16_t)(offset_c * 65535.0f / 175.0f);
    uint8_t bytes[5] = {CMD_SET_TEMP_OFFSET >> 8, CMD_SET_TEMP_OFFSET & 0xFF, word >> 8, word & 0xFF, 0};
    bytes[4] = scd4x_crc8(&bytes[2], 2);
    ESP_RETURN_ON_ERROR(i2c_master_transmit(s_device, bytes, sizeof(bytes), SCD41_TIMEOUT_MS), TAG, "offset write failed");
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS) + 1);
    return ESP_OK;
}

esp_err_t scd41_start(void)
{
    return send_command(CMD_START_PERIODIC);
}

esp_err_t scd41_read(float *co2_ppm, float *temp_c, float *rh_pct)
{
    uint8_t ready[3];
    ESP_RETURN_ON_ERROR(read_response(CMD_DATA_READY, ready, sizeof(ready)), TAG, "data ready failed");
    if (scd4x_crc8(ready, 2) != ready[2]) {
        return ESP_ERR_INVALID_CRC;
    }
    // The lower 11 bits are zero when no new measurement is available.
    if ((((uint16_t)ready[0] << 8 | ready[1]) & 0x07FF) == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    uint8_t raw[9];
    ESP_RETURN_ON_ERROR(read_response(CMD_READ_MEASUREMENT, raw, sizeof(raw)), TAG, "measurement read failed");
    return scd4x_decode_measurement(raw, co2_ppm, temp_c, rh_pct) ? ESP_OK : ESP_ERR_INVALID_CRC;
}
