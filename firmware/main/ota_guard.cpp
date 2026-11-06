#include "ota_guard.h"

#include <cstdint>

#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "matter_app.h"
#include "sensors_task.h"

static const char *TAG = "ota_guard";

static constexpr uint32_t CONFIRM_TIMEOUT_S = 300;
static constexpr uint32_t TASK_STACK_BYTES = 3072;
static constexpr UBaseType_t TASK_PRIORITY = 2;

// Firmware still waiting for confirmation blocks further uploads, so every path must end
// with it either confirmed or replaced.
static void give_up_on_firmware()
{
    if (!esp_ota_check_rollback_is_possible()) {
        ESP_LOGE(TAG, "no previous firmware to return to, keeping this one");
        if (esp_ota_mark_app_valid_cancel_rollback() != ESP_OK) {
            esp_restart();
        }
        return;
    }
    ESP_LOGE(TAG, "new firmware did not come up, rolling back");
    esp_ota_mark_app_invalid_rollback_and_reboot();
    // Only reached if that failed. The bootloader treats an unconfirmed restart as a failed update.
    esp_restart();
}

static void ota_guard_task(void *)
{
    for (uint32_t waited_s = 0; waited_s < CONFIRM_TIMEOUT_S; waited_s++) {
        if (matter_app_is_wifi_connected() && sensors_task_has_any_reading() &&
            esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
            ESP_LOGW(TAG, "new firmware confirmed");
            vTaskDelete(nullptr);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    give_up_on_firmware();
    vTaskDelete(nullptr);
}

esp_err_t ota_guard_start()
{
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) != ESP_OK ||
        state != ESP_OTA_IMG_PENDING_VERIFY) {
        return ESP_OK;
    }
    return xTaskCreate(ota_guard_task, "ota_guard", TASK_STACK_BYTES, nullptr, TASK_PRIORITY, nullptr) == pdPASS
               ? ESP_OK
               : ESP_ERR_NO_MEM;
}
