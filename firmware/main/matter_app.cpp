#include "matter_app.h"

#include <atomic>
#include <cmath>

#include <app-common/zap-generated/cluster-enums.h>
#include <app/clusters/air-quality-server/AirQualityCluster.h>
#include <app/clusters/boolean-state-server/CodegenIntegration.h>
#include <app/clusters/relative-humidity-measurement-server/CodegenIntegration.h>
#include <app/clusters/temperature-measurement-server/CodegenIntegration.h>
#include <app/server/Server.h>
#include <data_model_provider/esp_matter_data_model_provider.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <platform/CHIPDeviceLayer.h>

using namespace esp_matter;
using namespace chip::app::Clusters;

static const char *TAG = "matter_app";

static uint16_t s_air_endpoint;
static uint16_t s_temperature_endpoint;
static uint16_t s_humidity_endpoint;
static uint16_t s_ventilate_endpoint;
static uint16_t s_dust_endpoint;
static uint16_t s_fault_endpoint;

static std::atomic<bool> s_commissioned{false};
static std::atomic<bool> s_wifi_connected{false};

// Clusters without their own server implementation keep the value in the esp-matter store.
static void update_stored_attribute(uint16_t endpoint, uint32_t cluster, uint32_t attr, esp_matter_attr_val_t value)
{
    const esp_err_t err = attribute::update(endpoint, cluster, attr, &value);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "update failed: ep=%u cluster=0x%lx attr=0x%lx err=%s", endpoint,
                 static_cast<unsigned long>(cluster), static_cast<unsigned long>(attr), esp_err_to_name(err));
    }
}

// Boolean State reports a closed contact as true, so an open alert is false.
// The caller must hold the Matter stack lock or run on the Matter thread.
static void set_contact_open(uint16_t endpoint, bool open)
{
    BooleanStateCluster *const cluster = BooleanState::FindClusterOnEndpoint(endpoint);
    if (cluster == nullptr) {
        ESP_LOGW(TAG, "no Boolean State cluster on endpoint %u", endpoint);
        return;
    }
    cluster->SetStateValue(!open);
}

static void refresh_connection_state()
{
    s_commissioned = chip::Server::GetInstance().GetFabricTable().FabricCount() > 0;
    s_wifi_connected = chip::DeviceLayer::ConnectivityMgr().IsWiFiStationConnected();
}

static void on_device_event(const chip::DeviceLayer::ChipDeviceEvent *event, intptr_t)
{
    using namespace chip::DeviceLayer::DeviceEventType;
    switch (event->Type) {
    case kServerReady:
    case kCommissioningComplete:
    case kFabricRemoved:
    case kWiFiConnectivityChange:
    case kInterfaceIpAddressChanged:
        refresh_connection_state();
        break;
    default:
        break;
    }
}

static esp_err_t on_attribute_update(attribute::callback_type_t, uint16_t, uint32_t, uint32_t, esp_matter_attr_val_t *,
                                     void *)
{
    return ESP_OK;
}

static esp_err_t on_identify(identification::callback_type_t, uint16_t, uint8_t, uint8_t, void *)
{
    return ESP_OK;
}

static esp_err_t add_air_quality_clusters(endpoint_t *endpoint)
{
    cluster_t *air_quality = cluster::get(endpoint, AirQuality::Id);
    if (air_quality == nullptr || cluster::air_quality::feature::moderate::add(air_quality) != ESP_OK) {
        return ESP_FAIL;
    }

    namespace concentration = cluster::concentration_measurement;

    concentration::config_t co2_config;
    co2_config.feature_flags = concentration::feature::numeric_measurement::get_id();
    co2_config.features.numeric_measurement.measurement_unit =
        static_cast<uint8_t>(CarbonDioxideConcentrationMeasurement::MeasurementUnitEnum::kPpm);
    cluster_t *co2 = cluster::carbon_dioxide_concentration_measurement::create(endpoint, &co2_config, CLUSTER_FLAG_SERVER);

    concentration::config_t pm25_config;
    pm25_config.feature_flags = concentration::feature::numeric_measurement::get_id();
    pm25_config.features.numeric_measurement.measurement_unit =
        static_cast<uint8_t>(Pm25ConcentrationMeasurement::MeasurementUnitEnum::kUgm3);
    cluster_t *pm25 = cluster::pm25_concentration_measurement::create(endpoint, &pm25_config, CLUSTER_FLAG_SERVER);

    return co2 != nullptr && pm25 != nullptr ? ESP_OK : ESP_FAIL;
}

static uint16_t create_contact_sensor(node_t *node)
{
    endpoint::contact_sensor::config_t config;
    endpoint_t *created = endpoint::contact_sensor::create(node, &config, ENDPOINT_FLAG_NONE, nullptr);
    return created != nullptr ? endpoint::get_id(created) : 0;
}

esp_err_t matter_app_start()
{
    node::config_t node_config;
    node_t *node = node::create(&node_config, on_attribute_update, on_identify);
    if (node == nullptr) {
        ESP_LOGE(TAG, "node create failed");
        return ESP_FAIL;
    }

    endpoint::air_quality_sensor::config_t air_config;
    endpoint_t *air = endpoint::air_quality_sensor::create(node, &air_config, ENDPOINT_FLAG_NONE, nullptr);
    if (air == nullptr || add_air_quality_clusters(air) != ESP_OK) {
        ESP_LOGE(TAG, "air quality endpoint failed");
        return ESP_FAIL;
    }
    s_air_endpoint = endpoint::get_id(air);

    endpoint::temperature_sensor::config_t temperature_config;
    endpoint_t *temperature = endpoint::temperature_sensor::create(node, &temperature_config, ENDPOINT_FLAG_NONE, nullptr);
    endpoint::humidity_sensor::config_t humidity_config;
    endpoint_t *humidity = endpoint::humidity_sensor::create(node, &humidity_config, ENDPOINT_FLAG_NONE, nullptr);
    if (temperature == nullptr || humidity == nullptr) {
        ESP_LOGE(TAG, "temperature or humidity endpoint failed");
        return ESP_FAIL;
    }
    s_temperature_endpoint = endpoint::get_id(temperature);
    s_humidity_endpoint = endpoint::get_id(humidity);

    s_ventilate_endpoint = create_contact_sensor(node);
    s_dust_endpoint = create_contact_sensor(node);
    s_fault_endpoint = create_contact_sensor(node);
    if (s_ventilate_endpoint == 0 || s_dust_endpoint == 0 || s_fault_endpoint == 0) {
        ESP_LOGE(TAG, "contact sensor endpoint failed");
        return ESP_FAIL;
    }

    // Blocks until the Matter server has initialized, so the cluster servers exist afterwards.
    const esp_err_t err = esp_matter::start(on_device_event);
    if (err != ESP_OK) {
        return err;
    }
    // The Boolean State server starts as false, which means an open contact and a false alert.
    matter_app_set_alerts(false, false, false);
    return ESP_OK;
}

bool matter_app_is_commissioned()
{
    return s_commissioned;
}

bool matter_app_is_wifi_connected()
{
    return s_wifi_connected;
}

void matter_app_set_level(aq_level_t level)
{
    const lock::ScopedChipStackLock stack_lock(portMAX_DELAY);
    // Registered by esp-matter for this path, and no finder function is exported for this cluster.
    auto *const cluster = static_cast<chip::app::Clusters::AirQualityCluster *>(
        data_model::provider::get_instance().registry().Get(chip::app::ConcreteClusterPath(s_air_endpoint, AirQuality::Id)));
    if (cluster == nullptr) {
        ESP_LOGW(TAG, "no Air Quality cluster");
        return;
    }
    cluster->SetAirQuality(static_cast<AirQuality::AirQualityEnum>(level));
}

void matter_app_set_co2(float ppm)
{
    update_stored_attribute(s_air_endpoint, CarbonDioxideConcentrationMeasurement::Id,
                            CarbonDioxideConcentrationMeasurement::Attributes::MeasuredValue::Id,
                            esp_matter_nullable_float(nullable<float>(ppm)));
}

void matter_app_set_pm25(float ugm3)
{
    update_stored_attribute(s_air_endpoint, Pm25ConcentrationMeasurement::Id,
                            Pm25ConcentrationMeasurement::Attributes::MeasuredValue::Id,
                            esp_matter_nullable_float(nullable<float>(ugm3)));
}

void matter_app_set_temperature(float celsius)
{
    const lock::ScopedChipStackLock stack_lock(portMAX_DELAY);
    // The cluster stores hundredths of a degree.
    const auto value = static_cast<int16_t>(std::lround(celsius * 100.0f));
    const CHIP_ERROR err = TemperatureMeasurement::SetMeasuredValue(s_temperature_endpoint,
                                                                    chip::app::DataModel::MakeNullable(value));
    if (err != CHIP_NO_ERROR) {
        ESP_LOGW(TAG, "temperature update failed: %" CHIP_ERROR_FORMAT, err.Format());
    }
}

void matter_app_set_humidity(float percent)
{
    const lock::ScopedChipStackLock stack_lock(portMAX_DELAY);
    // The cluster stores hundredths of a percent.
    const auto value = static_cast<uint16_t>(std::lround(percent * 100.0f));
    const CHIP_ERROR err = RelativeHumidityMeasurement::SetMeasuredValue(s_humidity_endpoint,
                                                                         chip::app::DataModel::MakeNullable(value));
    if (err != CHIP_NO_ERROR) {
        ESP_LOGW(TAG, "humidity update failed: %" CHIP_ERROR_FORMAT, err.Format());
    }
}

void matter_app_set_alerts(bool ventilate_open, bool dust_open, bool fault_open)
{
    const lock::ScopedChipStackLock stack_lock(portMAX_DELAY);
    set_contact_open(s_ventilate_endpoint, ventilate_open);
    set_contact_open(s_dust_endpoint, dust_open);
    set_contact_open(s_fault_endpoint, fault_open);
}
