// Pin assignment for the Seeed Studio XIAO ESP32-C6.
#pragma once

#define BOARD_PIN_SCD41_SDA 22  // D4
#define BOARD_PIN_SCD41_SCL 23  // D5
#define BOARD_PIN_PMS_TX 16     // D6, to PMS5003 RX
#define BOARD_PIN_PMS_RX 17     // D7, from PMS5003 TX
#define BOARD_PMS_UART_NUM 1

#define BOARD_PIN_BOOT 9
#define BOARD_PIN_LED 15

// Low on both pins selects the built-in ceramic antenna.
#define BOARD_PIN_RF_SWITCH_POWER 3
#define BOARD_PIN_RF_SWITCH_SELECT 14
