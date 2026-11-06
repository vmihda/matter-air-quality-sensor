// Pin assignment. The product board is the Seeed Studio XIAO ESP32-C6.
#pragma once

#include <sdkconfig.h>

#if CONFIG_IDF_TARGET_ESP32S3
// Test board for the Apple Home and device page checks, a XIAO ESP32-S3 without sensors.
#define BOARD_PIN_I2C_SDA 5  // D4
#define BOARD_PIN_I2C_SCL 6  // D5
#define BOARD_PIN_PMS_TX 43    // D6, to PMS5003 RX
#define BOARD_PIN_PMS_RX 44    // D7, from PMS5003 TX
#define BOARD_PMS_UART_NUM 1

#define BOARD_PIN_BOOT 0
#define BOARD_PIN_LED 21
#define BOARD_LED_ACTIVE_LOW true
#else
#define BOARD_PIN_I2C_SDA 22  // D4
#define BOARD_PIN_I2C_SCL 23  // D5
#define BOARD_PIN_PMS_TX 16     // D6, to PMS5003 RX
#define BOARD_PIN_PMS_RX 17     // D7, from PMS5003 TX
#define BOARD_PMS_UART_NUM 1

#define BOARD_PIN_BOOT 9
#define BOARD_PIN_LED 15
// To confirm on the real board during the sensor check.
#define BOARD_LED_ACTIVE_LOW true

// Low on both pins selects the built-in ceramic antenna.
#define BOARD_HAS_RF_SWITCH 1
#define BOARD_PIN_RF_SWITCH_POWER 3
#define BOARD_PIN_RF_SWITCH_SELECT 14
#endif
