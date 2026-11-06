// Simulates the subset of the Sensirion SCD41 I2C protocol used by this project.
#include "wokwi-api.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define SCD41_ADDRESS 0x62

typedef struct {
  uint32_t co2_attr;
  uint32_t temperature_attr;
  uint32_t humidity_attr;
  uint32_t failure_attr;
  uint16_t command;
  uint8_t command_bytes;
  uint8_t response[9];
  uint8_t response_index;
} chip_state_t;

static uint8_t crc8(uint8_t high, uint8_t low) {
  uint8_t crc = 0xFF;
  uint8_t bytes[2] = {high, low};
  for (int i = 0; i < 2; i++) {
    crc ^= bytes[i];
    for (int bit = 0; bit < 8; bit++) {
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    }
  }
  return crc;
}

static void put_word(chip_state_t *chip, int word_index, uint16_t value) {
  uint8_t high = value >> 8;
  uint8_t low = value & 0xFF;
  chip->response[word_index * 3] = high;
  chip->response[word_index * 3 + 1] = low;
  chip->response[word_index * 3 + 2] = crc8(high, low);
}

static void prepare_response(chip_state_t *chip) {
  chip->response_index = 0;
  if (chip->command == 0xEC05) { // read_measurement
    float temperature = attr_read_float(chip->temperature_attr);
    float humidity = attr_read_float(chip->humidity_attr);
    put_word(chip, 0, (uint16_t)attr_read_float(chip->co2_attr));
    put_word(chip, 1, (uint16_t)((temperature + 45.0f) * 65535.0f / 175.0f));
    put_word(chip, 2, (uint16_t)(humidity * 65535.0f / 100.0f));
  } else if (chip->command == 0x3682) { // get_serial_number
    put_word(chip, 0, 0x1234);
    put_word(chip, 1, 0x5678);
    put_word(chip, 2, 0x9ABC);
  }
}

static bool on_connect(void *user_data, uint32_t address, bool read) {
  chip_state_t *chip = user_data;
  if (!read) {
    chip->command_bytes = 0;
  }
  return attr_read_float(chip->failure_attr) < 0.5f;
}

static uint8_t on_read(void *user_data) {
  chip_state_t *chip = user_data;
  return chip->response_index < 9 ? chip->response[chip->response_index++] : 0xFF;
}

static bool on_write(void *user_data, uint8_t data) {
  chip_state_t *chip = user_data;
  chip->command = (uint16_t)((chip->command << 8) | data);
  if (++chip->command_bytes == 2) {
    prepare_response(chip);
  }
  return true;
}

void chip_init(void) {
  chip_state_t *chip = calloc(1, sizeof(chip_state_t));
  chip->co2_attr = attr_init_float("co2", 600);
  chip->temperature_attr = attr_init_float("temperature", 22);
  chip->humidity_attr = attr_init_float("humidity", 45);
  chip->failure_attr = attr_init_float("failure", 0);

  const i2c_config_t i2c = {
      .address = SCD41_ADDRESS,
      .scl = pin_init("SCL", INPUT_PULLUP),
      .sda = pin_init("SDA", INPUT_PULLUP),
      .connect = on_connect,
      .read = on_read,
      .write = on_write,
      .user_data = chip,
  };
  i2c_init(&i2c);
  printf("SCD41 simulation ready at 0x%02X\n", SCD41_ADDRESS);
}
