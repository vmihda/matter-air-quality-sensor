#include "pms5003.h"

#include "driver/uart.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "pms5003_proto.h"

#define PMS_BAUD 9600
#define PMS_RX_BUFFER_SIZE 512
#define PMS_READ_SLICE_MS 100

static const char *TAG = "pms5003";

static uart_port_t s_uart;
static pms_parser_t s_parser;

esp_err_t pms5003_init(int uart_num, int tx_gpio, int rx_gpio)
{
    const uart_config_t config = {
        .baud_rate = PMS_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    s_uart = (uart_port_t)uart_num;
    pms_parser_init(&s_parser);
    ESP_RETURN_ON_ERROR(uart_driver_install(s_uart, PMS_RX_BUFFER_SIZE, 0, 0, NULL, 0), TAG, "driver install failed");
    ESP_RETURN_ON_ERROR(uart_param_config(s_uart, &config), TAG, "param config failed");
    return uart_set_pin(s_uart, tx_gpio, rx_gpio, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

esp_err_t pms5003_set_awake(bool awake)
{
    uint8_t command[7];
    if (awake) {
        pms_build_wake_cmd(command);
    } else {
        pms_build_sleep_cmd(command);
    }
    const int written = uart_write_bytes(s_uart, command, sizeof(command));
    return written == (int)sizeof(command) ? ESP_OK : ESP_FAIL;
}

esp_err_t pms5003_read(uint16_t *pm25, uint32_t timeout_ms)
{
    // Drop frames buffered during the settling time so the result is current.
    ESP_RETURN_ON_ERROR(uart_flush_input(s_uart), TAG, "flush failed");
    pms_parser_init(&s_parser);

    const int64_t deadline_us = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    while (esp_timer_get_time() < deadline_us) {
        uint8_t chunk[32];
        const int length = uart_read_bytes(s_uart, chunk, sizeof(chunk), pdMS_TO_TICKS(PMS_READ_SLICE_MS));
        for (int i = 0; i < length; i++) {
            if (pms_parser_feed(&s_parser, chunk[i], pm25)) {
                return ESP_OK;
            }
        }
    }
    return ESP_ERR_TIMEOUT;
}
