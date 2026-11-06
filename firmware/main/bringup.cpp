#include "bringup.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "board_config.h"
#include "ens160.h"
#include "pms5003.h"
#include "scd41.h"

static const char *TAG = "bringup";

static constexpr uint32_t PMS_READ_TIMEOUT_MS = 3000;
static constexpr uint32_t CHECK_PERIOD_MS = 5000;
static constexpr int PMS_SLEEP_TEST_AFTER_CHECKS = 12;
static constexpr int PMS_SLEEP_TEST_LENGTH_CHECKS = 2;

static void check_scd41()
{
    float co2{};
    float temperature{};
    float humidity{};
    const esp_err_t err = scd41_read(&co2, &temperature, &humidity);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "SCD41 CO2=%.0f ppm T=%.1f C RH=%.1f %%", co2, temperature, humidity);
    } else if (err != ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "SCD41 read failed: %s", esp_err_to_name(err));
    }
}

static void check_pms5003()
{
    pms_reading_t reading{};
    const esp_err_t err = pms5003_read(&reading, PMS_READ_TIMEOUT_MS);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "PMS5003 PM1=%u PM2.5=%u PM10=%u ug/m3", reading.pm1, reading.pm25, reading.pm10);
    } else {
        ESP_LOGE(TAG, "PMS5003 read failed: %s", esp_err_to_name(err));
    }
}

static void check_ens160()
{
    ens160_reading_t reading{};
    const esp_err_t err = ens160_read(&reading);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "ENS160 TVOC=%u ppb validity=%d", reading.tvoc_ppb, reading.validity);
    } else if (err != ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "ENS160 read failed: %s", esp_err_to_name(err));
    }
}

void bringup_run()
{
    ESP_ERROR_CHECK(scd41_init(BOARD_PIN_I2C_SDA, BOARD_PIN_I2C_SCL));
    uint64_t serial{};
    ESP_ERROR_CHECK(scd41_get_serial(&serial));
    ESP_LOGI(TAG, "SCD41 serial=%012llX", static_cast<unsigned long long>(serial));
    ESP_ERROR_CHECK(scd41_start());

    ESP_ERROR_CHECK(pms5003_init(BOARD_PMS_UART_NUM, BOARD_PIN_PMS_TX, BOARD_PIN_PMS_RX));
    ESP_ERROR_CHECK(pms5003_set_awake(true));
    // Optional, so its absence does not stop the check of the other two sensors.
    const bool has_ens160 = ens160_init(BOARD_PIN_I2C_SDA, BOARD_PIN_I2C_SCL) == ESP_OK;
    ESP_LOGI(TAG, "ENS160 %s", has_ens160 ? "found" : "not found");

    // Periodically sleeps the PMS5003 so the fan stop can be checked by ear.
    for (int check = 0;; check++) {
        const int phase = check % (PMS_SLEEP_TEST_AFTER_CHECKS + PMS_SLEEP_TEST_LENGTH_CHECKS);
        if (phase == PMS_SLEEP_TEST_AFTER_CHECKS) {
            ESP_LOGI(TAG, "PMS5003 sleep requested, the fan should stop");
            ESP_ERROR_CHECK(pms5003_set_awake(false));
        } else if (phase == 0 && check != 0) {
            ESP_LOGI(TAG, "PMS5003 wake requested, the fan should start");
            ESP_ERROR_CHECK(pms5003_set_awake(true));
        }
        check_scd41();
        if (has_ens160) {
            check_ens160();
        }
        if (phase < PMS_SLEEP_TEST_AFTER_CHECKS) {
            check_pms5003();
        }
        vTaskDelay(pdMS_TO_TICKS(CHECK_PERIOD_MS));
    }
}
