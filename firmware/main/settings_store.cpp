#include "settings_store.h"

#include <cstdint>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <nvs.h>

static const char *TAG = "settings";
static const char *NVS_NAMESPACE = "aq_settings";
static const char *NVS_KEY = "v1";

static SemaphoreHandle_t s_mutex;
static aq_settings_t s_settings;

static bool load_from_nvs(aq_settings_t *out)
{
    nvs_handle_t handle{};
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    size_t length = sizeof(*out);
    const esp_err_t err = nvs_get_blob(handle, NVS_KEY, out, &length);
    nvs_close(handle);
    return err == ESP_OK && length == sizeof(*out) && aq_settings_validate(out);
}

static esp_err_t save_to_nvs(const aq_settings_t *settings)
{
    nvs_handle_t handle{};
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_blob(handle, NVS_KEY, settings, sizeof(*settings));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

esp_err_t settings_store_init()
{
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == nullptr) {
        return ESP_ERR_NO_MEM;
    }
    if (!load_from_nvs(&s_settings)) {
        ESP_LOGW(TAG, "using default settings");
        aq_settings_defaults(&s_settings);
    }
    return ESP_OK;
}

void settings_store_get(aq_settings_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_settings;
    xSemaphoreGive(s_mutex);
}

esp_err_t settings_store_set(const aq_settings_t *settings)
{
    if (!aq_settings_validate(settings)) {
        return ESP_ERR_INVALID_ARG;
    }
    const esp_err_t err = save_to_nvs(settings);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "save failed: %s", esp_err_to_name(err));
        return err;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_settings = *settings;
    xSemaphoreGive(s_mutex);
    return ESP_OK;
}
