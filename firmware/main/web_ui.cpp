#include "web_ui.h"

#include <array>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <esp_app_desc.h>
#include <esp_app_format.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mbedtls/base64.h>
#include <sdkconfig.h>

#include "sensors_task.h"
#include "settings_store.h"

static const char *TAG = "web_ui";

// The page, gzip-compressed at build time from web_ui_page.html.
extern const char PAGE_GZ_START[] asm("_binary_web_ui_page_html_gz_start");
extern const char PAGE_GZ_END[] asm("_binary_web_ui_page_html_gz_end");

static constexpr size_t AUTH_MAX_BYTES = 128;
static constexpr size_t SETTINGS_BODY_MAX_BYTES = 512;
static constexpr size_t STATUS_JSON_BYTES = 640;
static constexpr size_t UPLOAD_CHUNK_BYTES = 1024;
static constexpr size_t APP_HEADER_BYTES =
    sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t);
static constexpr int MAX_RECV_TIMEOUTS = 3;
static constexpr TickType_t FAILED_LOGIN_DELAY = pdMS_TO_TICKS(500);
static constexpr uint64_t RESTART_DELAY_US = 1000000;
// The upload and JSON buffers live on the handler stack.
static constexpr size_t SERVER_STACK_BYTES = 8192;
static constexpr uint16_t SERVER_MAX_SOCKETS = 3;
static constexpr uint16_t SERVER_RECV_TIMEOUT_S = 10;

static std::array<char, AUTH_MAX_BYTES> s_expected_auth;
// Requests run one at a time in the server task, so this needs no lock.
static bool s_restart_pending = false;

static bool build_expected_auth()
{
    std::array<char, AUTH_MAX_BYTES> credentials{};
    const int length = std::snprintf(credentials.data(), credentials.size(), "admin:%s", CONFIG_AQ_WEB_PASSWORD);
    if (length <= 0 || static_cast<size_t>(length) >= credentials.size()) {
        return false;
    }
    static constexpr char PREFIX[] = "Basic ";
    std::memcpy(s_expected_auth.data(), PREFIX, sizeof(PREFIX) - 1);
    size_t written = 0;
    return mbedtls_base64_encode(reinterpret_cast<unsigned char *>(s_expected_auth.data()) + sizeof(PREFIX) - 1,
                                 s_expected_auth.size() - sizeof(PREFIX), &written,
                                 reinterpret_cast<const unsigned char *>(credentials.data()),
                                 static_cast<size_t>(length)) == 0;
}

static bool equal_in_constant_time(const char *a, const char *b, size_t length)
{
    unsigned char difference = 0;
    for (size_t i = 0; i < length; i++) {
        difference |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return difference == 0;
}

static esp_err_t send_error(httpd_req_t *req, const char *status, const char *text)
{
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    return httpd_resp_sendstr(req, text);
}

static bool is_authorized(httpd_req_t *req)
{
    std::array<char, AUTH_MAX_BYTES> received{};
    const size_t expected_length = std::strlen(s_expected_auth.data());
    const bool has_header = httpd_req_get_hdr_value_str(req, "Authorization", received.data(), received.size()) == ESP_OK;
    if (has_header && std::strlen(received.data()) == expected_length &&
        equal_in_constant_time(received.data(), s_expected_auth.data(), expected_length)) {
        return true;
    }
    vTaskDelay(FAILED_LOGIN_DELAY);
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Air quality sensor\"");
    send_error(req, "401 Unauthorized", "Потрібен пароль");
    return false;
}

// A browser cannot add this header to a cross-site request without a preflight, which
// the device does not answer, so other websites cannot change settings through the page.
static bool has_request_header(httpd_req_t *req)
{
    if (httpd_req_get_hdr_value_len(req, "X-AQ-Request") > 0) {
        return true;
    }
    httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Неприпустимий запит");
    return false;
}

// Receives up to `capacity` bytes, tolerating brief pauses from the sender.
static int receive_some(httpd_req_t *req, char *buffer, size_t capacity)
{
    for (int attempt = 0; attempt < MAX_RECV_TIMEOUTS; attempt++) {
        const int received = httpd_req_recv(req, buffer, capacity);
        if (received != HTTPD_SOCK_ERR_TIMEOUT) {
            return received;
        }
    }
    return HTTPD_SOCK_ERR_TIMEOUT;
}

static esp_err_t handle_page(httpd_req_t *req)
{
    if (!is_authorized(req)) {
        return ESP_OK;
    }
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(req, "X-Frame-Options", "DENY");
    return httpd_resp_send(req, PAGE_GZ_START, PAGE_GZ_END - PAGE_GZ_START);
}

static const char *level_name(aq_level_t level)
{
    switch (level) {
    case AQ_GOOD:
        return "good";
    case AQ_MODERATE:
        return "moderate";
    case AQ_POOR:
        return "poor";
    case AQ_UNKNOWN:
        break;
    }
    return "unknown";
}

// The number as JSON, or null when the sensor has no current reading.
static std::array<char, 16> format_optional(bool has_value, float value, int decimals)
{
    std::array<char, 16> out{};
    if (has_value) {
        std::snprintf(out.data(), out.size(), "%.*f", decimals, static_cast<double>(value));
    } else {
        std::snprintf(out.data(), out.size(), "null");
    }
    return out;
}

static esp_err_t handle_status(httpd_req_t *req)
{
    if (!is_authorized(req)) {
        return ESP_OK;
    }
    SensorsStatus status{};
    sensors_task_get_status(&status);
    aq_settings_t settings{};
    settings_store_get(&settings);

    const auto co2 = format_optional(status.has_scd41, status.co2_ppm, 0);
    const auto pm1 = format_optional(status.has_pms5003, status.pm1_ugm3, 0);
    const auto pm25 = format_optional(status.has_pms5003, status.pm25_ugm3, 0);
    const auto pm10 = format_optional(status.has_pms5003, status.pm10_ugm3, 0);
    const auto voc = format_optional(status.has_tvoc, status.tvoc_ugm3, 0);
    const auto temperature = format_optional(status.has_scd41, status.temperature_c, 1);
    const auto humidity = format_optional(status.has_scd41, status.humidity_pct, 0);

    std::array<char, STATUS_JSON_BYTES> json{};
    const int length = std::snprintf(
        json.data(), json.size(),
        "{\"level\":\"%s\",\"co2\":%s,\"pm1\":%s,\"pm25\":%s,\"pm10\":%s,\"voc\":%s,\"voc_warming\":%s,\"temp\":%s,\"rh\":%s,\"ventilate\":%s,\"dust\":%s,\"fault\":%s,"
        "\"version\":\"%s\",\"settings\":{\"co2_moderate\":%.0f,\"co2_poor\":%.0f,"
        "\"pm25_moderate\":%.0f,\"pm25_poor\":%.0f,\"co2_hold_s\":%" PRIu32 ",\"pm25_hold_s\":%" PRIu32 ",\"cycle_s\":%" PRIu32
        ",\"temp_offset\":%.1f}}",
        level_name(status.output.level), co2.data(), pm1.data(), pm25.data(), pm10.data(), voc.data(),
        status.tvoc_warming_up ? "true" : "false", temperature.data(), humidity.data(),
        status.output.ventilate_alert ? "true" : "false", status.output.dust_alert ? "true" : "false",
        status.output.fault_alert ? "true" : "false", esp_app_get_description()->version,
        static_cast<double>(settings.co2_moderate_ppm), static_cast<double>(settings.co2_poor_ppm),
        static_cast<double>(settings.pm25_moderate), static_cast<double>(settings.pm25_poor),
        settings.co2_hold_s, settings.pm25_hold_s, settings.pms_cycle_s,
        static_cast<double>(settings.temp_offset_c));
    if (length <= 0 || static_cast<size_t>(length) >= json.size()) {
        return send_error(req, HTTPD_500, "Помилка пристрою");
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json.data(), length);
}

static esp_err_t handle_settings(httpd_req_t *req)
{
    if (!is_authorized(req) || !has_request_header(req)) {
        return ESP_OK;
    }
    std::array<char, SETTINGS_BODY_MAX_BYTES> body{};
    if (req->content_len == 0 || req->content_len >= body.size()) {
        return send_error(req, HTTPD_400, "Неправильний розмір запиту");
    }
    size_t total = 0;
    while (total < req->content_len) {
        const int received = receive_some(req, body.data() + total, req->content_len - total);
        if (received <= 0) {
            return ESP_FAIL;
        }
        total += static_cast<size_t>(received);
    }

    aq_settings_t parsed{};
    if (!aq_settings_from_form(body.data(), &parsed)) {
        return send_error(req, HTTPD_400, "перевірте введені значення");
    }
    if (settings_store_set(&parsed) != ESP_OK) {
        return send_error(req, HTTPD_500, "помилка запису в пам’ять пристрою");
    }
    return httpd_resp_sendstr(req, "ok");
}

static bool is_compatible_image(const char *data, size_t length)
{
    if (length < APP_HEADER_BYTES) {
        return false;
    }
    esp_image_header_t image{};
    std::memcpy(&image, data, sizeof(image));
    esp_app_desc_t description{};
    std::memcpy(&description, data + sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t),
                sizeof(description));
    return image.magic == ESP_IMAGE_HEADER_MAGIC && image.chip_id == CONFIG_IDF_FIRMWARE_CHIP_ID &&
           description.magic_word == ESP_APP_DESC_MAGIC_WORD &&
           std::strncmp(description.project_name, esp_app_get_description()->project_name,
                        sizeof(description.project_name)) == 0;
}

static void restart_callback(void *)
{
    esp_restart();
}

static esp_err_t schedule_restart()
{
    const esp_timer_create_args_t args = {
        .callback = restart_callback,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "fw_restart",
        .skip_unhandled_events = false,
    };
    esp_timer_handle_t timer = nullptr;
    const esp_err_t err = esp_timer_create(&args, &timer);
    return err == ESP_OK ? esp_timer_start_once(timer, RESTART_DELAY_US) : err;
}

static esp_err_t write_upload(httpd_req_t *req, esp_ota_handle_t ota, std::array<char, UPLOAD_CHUNK_BYTES> &chunk,
                              size_t buffered)
{
    size_t total = buffered;
    size_t pending = buffered;
    while (true) {
        const esp_err_t err = esp_ota_write(ota, chunk.data(), pending);
        if (err != ESP_OK) {
            return err;
        }
        if (total >= req->content_len) {
            return ESP_OK;
        }
        const int received = receive_some(req, chunk.data(), chunk.size());
        if (received <= 0) {
            return ESP_FAIL;
        }
        pending = static_cast<size_t>(received);
        total += pending;
    }
}

static esp_err_t handle_firmware(httpd_req_t *req)
{
    if (!is_authorized(req) || !has_request_header(req)) {
        return ESP_OK;
    }
    if (s_restart_pending) {
        return send_error(req, "503 Service Unavailable", "пристрій уже перезавантажується");
    }
    const esp_partition_t *target = esp_ota_get_next_update_partition(nullptr);
    if (target == nullptr || req->content_len < APP_HEADER_BYTES || req->content_len > target->size) {
        return send_error(req, HTTPD_400, "файл не підходить за розміром");
    }

    std::array<char, UPLOAD_CHUNK_BYTES> chunk{};
    size_t received_total = 0;
    while (received_total < APP_HEADER_BYTES) {
        const int received = receive_some(req, chunk.data() + received_total, chunk.size() - received_total);
        if (received <= 0) {
            return ESP_FAIL;
        }
        received_total += static_cast<size_t>(received);
    }
    // Checked before anything is erased, so a wrong file leaves the update slot untouched.
    if (!is_compatible_image(chunk.data(), received_total)) {
        return send_error(req, HTTPD_400, "це не прошивка цього пристрою");
    }

    esp_ota_handle_t ota = 0;
    if (esp_ota_begin(target, OTA_WITH_SEQUENTIAL_WRITES, &ota) != ESP_OK) {
        return send_error(req, HTTPD_500, "не вдалося почати запис");
    }
    if (write_upload(req, ota, chunk, received_total) != ESP_OK) {
        esp_ota_abort(ota);
        return send_error(req, HTTPD_500, "завантаження перервалося");
    }
    // esp_ota_end verifies the image checksum.
    if (esp_ota_end(ota) != ESP_OK || esp_ota_set_boot_partition(target) != ESP_OK) {
        return send_error(req, HTTPD_400, "файл пошкоджено");
    }
    ESP_LOGW(TAG, "firmware uploaded, restarting");
    s_restart_pending = true;
    httpd_resp_sendstr(req, "ok");
    return schedule_restart();
}

esp_err_t web_ui_start()
{
    if (std::strlen(CONFIG_AQ_WEB_PASSWORD) == 0) {
        ESP_LOGE(TAG, "no page password configured, the page stays off");
        return ESP_OK;
    }
    if (!build_expected_auth()) {
        ESP_LOGE(TAG, "page password is too long, the page stays off");
        return ESP_ERR_INVALID_SIZE;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = SERVER_STACK_BYTES;
    config.max_open_sockets = SERVER_MAX_SOCKETS;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = SERVER_RECV_TIMEOUT_S;

    httpd_handle_t server = nullptr;
    const esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        return err;
    }
    const std::array<httpd_uri_t, 4> routes{{
        {.uri = "/", .method = HTTP_GET, .handler = handle_page, .user_ctx = nullptr},
        {.uri = "/api/status", .method = HTTP_GET, .handler = handle_status, .user_ctx = nullptr},
        {.uri = "/api/settings", .method = HTTP_POST, .handler = handle_settings, .user_ctx = nullptr},
        {.uri = "/api/firmware", .method = HTTP_POST, .handler = handle_firmware, .user_ctx = nullptr},
    }};
    for (const httpd_uri_t &route : routes) {
        const esp_err_t route_err = httpd_register_uri_handler(server, &route);
        if (route_err != ESP_OK) {
            return route_err;
        }
    }
    return ESP_OK;
}
