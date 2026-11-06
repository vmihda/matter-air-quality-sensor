#include "service_console.h"

#include <cinttypes>
#include <cstdio>
#include <cstring>

#include <esp_app_desc.h>
#include <esp_console.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_netif.h>

#include "matter_app.h"
#include "sensors_task.h"
#include "settings_store.h"

static int command_status(int, char **)
{
    SensorsStatus status{};
    sensors_task_get_status(&status);
    std::printf("firmware: %s\n", esp_app_get_description()->version);
    std::printf("commissioned: %d, wifi: %d\n", matter_app_is_commissioned(), matter_app_is_wifi_connected());

    esp_netif_ip_info_t ip{};
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif != nullptr && esp_netif_get_ip_info(netif, &ip) == ESP_OK) {
        std::printf("ip: " IPSTR "\n", IP2STR(&ip.ip));
    }

    std::printf("co2: %.0f ppm (%d), pm25: %.0f ug/m3 (%d)\n", static_cast<double>(status.co2_ppm), status.has_scd41,
                static_cast<double>(status.pm25_ugm3), status.has_pms5003);
    std::printf("pm1: %.0f ug/m3, pm10: %.0f ug/m3, voc: %.0f ug/m3 (%d, warming up %d)\n",
                static_cast<double>(status.pm1_ugm3), static_cast<double>(status.pm10_ugm3),
                static_cast<double>(status.tvoc_ugm3), status.has_tvoc, status.tvoc_warming_up);
    std::printf("temperature: %.1f C, humidity: %.0f %%\n", static_cast<double>(status.temperature_c),
                static_cast<double>(status.humidity_pct));
    std::printf("level: %d, ventilate: %d, dust: %d, fault: %d\n", status.output.level, status.output.ventilate_alert,
                status.output.dust_alert, status.output.fault_alert);
    std::printf("free heap: %u, minimum: %u\n", static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_DEFAULT)),
                static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT)));
    return 0;
}

static int command_settings(int, char **)
{
    aq_settings_t s{};
    settings_store_get(&s);
    std::printf("co2: moderate %.0f, poor %.0f ppm\n", static_cast<double>(s.co2_moderate_ppm),
                static_cast<double>(s.co2_poor_ppm));
    std::printf("pm25: moderate %.0f, poor %.0f ug/m3\n", static_cast<double>(s.pm25_moderate),
                static_cast<double>(s.pm25_poor));
    std::printf("pm25 cycle: %" PRIu32 " s, temperature offset: %.1f C\n", s.pms_cycle_s,
                static_cast<double>(s.temp_offset_c));
    return 0;
}

static int command_log(int argc, char **argv)
{
    if (argc != 2) {
        std::printf("usage: log none|error|warn|info|debug\n");
        return 1;
    }
    static constexpr struct {
        const char *name;
        esp_log_level_t level;
    } LEVELS[] = {
        {"none", ESP_LOG_NONE}, {"error", ESP_LOG_ERROR}, {"warn", ESP_LOG_WARN},
        {"info", ESP_LOG_INFO}, {"debug", ESP_LOG_DEBUG},
    };
    for (const auto &entry : LEVELS) {
        if (std::strcmp(argv[1], entry.name) == 0) {
            esp_log_level_set("*", entry.level);
            return 0;
        }
    }
    std::printf("unknown level\n");
    return 1;
}

static esp_err_t register_command(const char *name, const char *help, esp_console_cmd_func_t handler)
{
    esp_console_cmd_t command{};
    command.command = name;
    command.help = help;
    command.func = handler;
    return esp_console_cmd_register(&command);
}

esp_err_t service_console_start()
{
    esp_console_repl_t *repl = nullptr;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "aq>";
    const esp_console_dev_usb_serial_jtag_config_t device_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();

    esp_err_t err = esp_console_new_repl_usb_serial_jtag(&device_config, &repl_config, &repl);
    if (err != ESP_OK) {
        return err;
    }
    static constexpr struct {
        const char *name;
        const char *help;
        esp_console_cmd_func_t handler;
    } COMMANDS[] = {
        {"status", "Show device state", command_status},
        {"settings", "Show current settings", command_settings},
        {"log", "Set the log level, limited by the build", command_log},
    };
    for (const auto &command : COMMANDS) {
        err = register_command(command.name, command.help, command.handler);
        if (err != ESP_OK) {
            return err;
        }
    }
    err = esp_console_register_help_command();
    return err == ESP_OK ? esp_console_start_repl(repl) : err;
}
