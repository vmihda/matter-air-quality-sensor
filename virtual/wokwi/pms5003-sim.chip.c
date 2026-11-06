// Simulates the PMS5003 UART frame and its sleep and wake commands.
#include "wokwi-api.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FRAME_SIZE 32
#define PM25_ATMOSPHERIC_OFFSET 12

typedef struct {
  uart_dev_t uart;
  uint32_t pm25_attr;
  uint32_t failure_attr;
  bool awake;
  uint8_t frame[FRAME_SIZE];
  uint8_t command[7];
  uint8_t command_length;
} chip_state_t;

static void send_frame(void *user_data) {
  chip_state_t *chip = user_data;
  if (!chip->awake || attr_read_float(chip->failure_attr) >= 0.5f) {
    return;
  }
  uint16_t pm25 = (uint16_t)attr_read_float(chip->pm25_attr);
  memset(chip->frame, 0, FRAME_SIZE);
  chip->frame[0] = 0x42;
  chip->frame[1] = 0x4D;
  chip->frame[3] = 0x1C;
  chip->frame[PM25_ATMOSPHERIC_OFFSET] = pm25 >> 8;
  chip->frame[PM25_ATMOSPHERIC_OFFSET + 1] = pm25 & 0xFF;
  uint16_t sum = 0;
  for (int i = 0; i < FRAME_SIZE - 2; i++) {
    sum += chip->frame[i];
  }
  chip->frame[FRAME_SIZE - 2] = sum >> 8;
  chip->frame[FRAME_SIZE - 1] = sum & 0xFF;
  uart_write(chip->uart, chip->frame, FRAME_SIZE);
}

static void on_rx(void *user_data, uint8_t byte) {
  chip_state_t *chip = user_data;
  static const uint8_t header[2] = {0x42, 0x4D};
  if (chip->command_length < 2 && byte != header[chip->command_length]) {
    chip->command_length = 0;
    return;
  }
  chip->command[chip->command_length++] = byte;
  if (chip->command_length == 7) {
    chip->command_length = 0;
    if (chip->command[2] == 0xE4) {
      chip->awake = chip->command[4] == 0x01;
      printf("PMS5003 %s\n", chip->awake ? "wake" : "sleep");
    }
  }
}

void chip_init(void) {
  chip_state_t *chip = calloc(1, sizeof(chip_state_t));
  chip->pm25_attr = attr_init_float("pm25", 8);
  chip->failure_attr = attr_init_float("failure", 0);
  chip->awake = true;

  const uart_config_t uart = {
      .tx = pin_init("TX", INPUT_PULLUP),
      .rx = pin_init("RX", INPUT),
      .baud_rate = 9600,
      .rx_data = on_rx,
      .user_data = chip,
  };
  chip->uart = uart_init(&uart);

  const timer_config_t timer = {
      .callback = send_frame,
      .user_data = chip,
  };
  timer_start(timer_init(&timer), 1000000, true);
  printf("PMS5003 simulation ready\n");
}
