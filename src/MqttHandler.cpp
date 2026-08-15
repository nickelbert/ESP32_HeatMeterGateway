#include "MqttHandler.h"
#include "ConfigManager.h"
#include "TelnetServer.h"
#include "MeterT550.h"
#include "esp_log.h"
#include <algorithm>
#include <cstring>

static const char *TAG = "MqttHandler";
extern ConfigManager ConfigManager;
extern TelnetServer TelnetServer;
extern MeterT550 MeterT550;

static MqttHandler *mqttHandlerInstance = nullptr;

void triggerMqttPublish()
{
    if (mqttHandlerInstance != nullptr)
    {
        mqttHandlerInstance->sendState();
    }
}

void MqttHandler::mqttEventHandler(void *handlerArgs, esp_event_base_t base, int32_t eventId, void *eventData)
{
    MqttHandler *self = static_cast<MqttHandler *>(handlerArgs);
    esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(eventData);

    switch ((esp_mqtt_event_id_t)eventId)
    {
        case MQTT_EVENT_CONNECTED:
        {
            self->isConnectedState = true;
            TelnetServer.telnetPrint("[MQTT] Connected to broker\r\n");
            ESP_LOGI(TAG, "Connected to MQTT broker");

            esp_mqtt_client_subscribe(self->clientHandle, "ultraheat/command", 0);
            self->sendHaAutoDiscovery();
            break;
        }

        case MQTT_EVENT_DISCONNECTED:
        {
            self->isConnectedState = false;
            TelnetServer.telnetPrint("[MQTT] Disconnected from broker\r\n");
            ESP_LOGW(TAG, "Disconnected from MQTT broker");
            break;
        }

        case MQTT_EVENT_DATA:
        {
            std::string topic(event->topic, event->topic_len);
            std::string data(event->data, event->data_len);

            std::string logMsg = "[MQTT] Command received on " + topic + ": " + data + "\r\n";
            TelnetServer.telnetPrint(logMsg);
            ESP_LOGI(TAG, "Incoming MQTT message on topic %s: %s", topic.c_str(), data.c_str());

            std::string upperData = data;
            std::transform(upperData.begin(), upperData.end(), upperData.begin(), ::toupper);

            if (upperData == "READ")
            {
                TelnetServer.telnetPrint("[MQTT] Triggering forced meter readout\r\n");
                ESP_LOGI(TAG, "Forced readout triggered via MQTT command");
                MeterT550.forceReading();
            }
            break;
        }

        case MQTT_EVENT_ERROR:
        {
            ESP_LOGE(TAG, "MQTT error occurred");
            break;
        }

        default:
            break;
    }
}

void MqttHandler::setup()
{
    mqttHandlerInstance = this;

    if (ConfigManager.mqttServer.empty())
    {
        ESP_LOGW(TAG, "No MQTT broker configured");
        return;
    }

    std::string brokerUri = "mqtt://" + ConfigManager.mqttServer + ":" + std::to_string(ConfigManager.mqttPort);

    esp_mqtt_client_config_t mqttCfg = {};
    mqttCfg.broker.address.uri = brokerUri.c_str();

    if (!ConfigManager.mqttUser.empty())
    {
        mqttCfg.credentials.username = ConfigManager.mqttUser.c_str();
        mqttCfg.credentials.authentication.password = ConfigManager.mqttPassword.c_str();
    }

    mqttCfg.credentials.client_id = "T550_Waermezaehler_C3";
    mqttCfg.buffer.size = 3072;

    clientHandle = esp_mqtt_client_init(&mqttCfg);
    if (clientHandle != nullptr)
    {
        esp_mqtt_client_register_event(clientHandle, MQTT_EVENT_ANY, &MqttHandler::mqttEventHandler, this);
        esp_mqtt_client_start(clientHandle);
        ESP_LOGI(TAG, "MQTT client started (Broker: %s)", brokerUri.c_str());
    }
    else
    {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
    }
}

void MqttHandler::publishHaSensor(const std::string &obis, const std::string &name, const std::string &unit,
                                 const std::string &devClass, const std::string &stateClass, const std::string &icon)
{
    std::string safeObis = obis;
    std::replace(safeObis.begin(), safeObis.end(), '.', '_');
    std::replace(safeObis.begin(), safeObis.end(), '*', '_');

    std::string topic = "homeassistant/sensor/T550_" + safeObis + "/config";

    cJSON *doc = cJSON_CreateObject();
    cJSON_AddStringToObject(doc, "name", name.c_str());
    cJSON_AddStringToObject(doc, "state_topic", ConfigManager.mqttTopic.c_str());

    std::string valueTemplate = "{{ value_json['" + obis + "'] }}";
    cJSON_AddStringToObject(doc, "value_template", valueTemplate.c_str());

    std::string uniqueId = "t550_" + safeObis;
    cJSON_AddStringToObject(doc, "unique_id", uniqueId.c_str());

    if (!unit.empty()) cJSON_AddStringToObject(doc, "unit_of_measurement", unit.c_str());
    if (!devClass.empty()) cJSON_AddStringToObject(doc, "device_class", devClass.c_str());
    if (!stateClass.empty()) cJSON_AddStringToObject(doc, "state_class", stateClass.c_str());
    if (!icon.empty()) cJSON_AddStringToObject(doc, "icon", icon.c_str());

    cJSON *device = cJSON_CreateObject();
    cJSON *identifiers = cJSON_CreateArray();
    cJSON_AddItemToArray(identifiers, cJSON_CreateString("t550_meter"));
    cJSON_AddItemToObject(device, "identifiers", identifiers);
    cJSON_AddStringToObject(device, "name", "Landis+Gyr T550");
    cJSON_AddStringToObject(device, "manufacturer", "Landis+Gyr");
    cJSON_AddStringToObject(device, "model", "Ultraheat T550");
    cJSON_AddItemToObject(doc, "device", device);

    char *payload = cJSON_PrintUnformatted(doc);
    if (payload != nullptr)
    {
        esp_mqtt_client_publish(clientHandle, topic.c_str(), payload, 0, 1, 1);
        cJSON_free(payload);
    }
    cJSON_Delete(doc);
}

void MqttHandler::sendHaAutoDiscovery()
{
    TelnetServer.telnetPrint("[MQTT] Sending Home Assistant auto-discovery\r\n");
    ESP_LOGI(TAG, "Publishing Home Assistant auto-discovery entities");

    // 1. Current main readings
    publishHaSensor("6.8", "Heat Energy", "MWh", "energy", "total_increasing", "mdi:fire");
    publishHaSensor("6.26", "Volume", "m³", "volume", "total_increasing", "mdi:water");

    // 2. Billing date / Previous year readings
    publishHaSensor("6.8*01", "Heat Energy (Previous Year)", "MWh", "energy", "total_increasing", "mdi:fire");
    publishHaSensor("6.26*01", "Volume (Previous Year)", "m³", "volume", "total_increasing", "mdi:water");
    publishHaSensor("6.6*01", "Power (Previous Year)", "kW", "power", "measurement", "mdi:heat-wave");
    publishHaSensor("6.33*01", "Flow Rate (Previous Year)", "m³/h", "volume_flow_rate", "measurement", "mdi:pipe");

    publishHaSensor("9.4.0", "Flow Temperature", "°C", "temperature", "measurement", "mdi:thermometer-high");
    publishHaSensor("9.4.1", "Return Temperature", "°C", "temperature", "measurement", "mdi:thermometer-low");
    publishHaSensor("9.4*01.0", "Flow Temperature (Previous Year)", "°C", "temperature", "measurement", "mdi:thermometer-high");
    publishHaSensor("9.4*01.1", "Return Temperature (Previous Year)", "°C", "temperature", "measurement", "mdi:thermometer-low");
    publishHaSensor("6.32*01", "Error Hours (Previous Year)", "h", "duration", "total_increasing", "mdi:clock-alert-outline");

    // 3. Current measurements
    publishHaSensor("6.6", "Power", "kW", "power", "measurement", "mdi:heat-wave");
    publishHaSensor("6.33", "Flow Rate", "m³/h", "volume_flow_rate", "measurement", "mdi:pipe");
    publishHaSensor("6.35", "Measurement Interval", "min", "duration", "", "mdi:update");

    // 4. Diagnostics & operating hours
    publishHaSensor("F", "Error Code", "", "", "", "mdi:alert");
    publishHaSensor("6.31", "Operating Hours", "h", "duration", "total_increasing", "mdi:counter");
    publishHaSensor("6.32", "Error Hours", "h", "duration", "total_increasing", "mdi:clock-alert-outline");

    // 5. Timestamps
    publishHaSensor("6.36", "Billing Date", "", "", "", "mdi:calendar-clock");
    publishHaSensor("9.36", "Meter Timestamp", "", "", "", "mdi:clock");

    // 6. Device information
    publishHaSensor("9.20", "Serial Number", "", "", "", "mdi:barcode");
    publishHaSensor("9.21", "Asset Number", "", "", "", "mdi:barcode");
    publishHaSensor("9.24", "Nominal Flow", "m³/h", "volume_flow_rate", "", "mdi:pipe");

    // Remote read trigger button
    const char *topicButton = "homeassistant/button/T550_read/config";
    const char *payloadButton = R"({
    "name": "Read Meter Now",
    "command_topic": "ultraheat/command",
    "payload_press": "READ",
    "device_class": "update",
    "unique_id": "t550_button_read",
    "device": {
        "identifiers": ["t550_meter"],
        "name": "Landis+Gyr T550",
        "manufacturer": "Landis+Gyr",
        "model": "Ultraheat T550"
    }
    })";
    esp_mqtt_client_publish(clientHandle, topicButton, payloadButton, 0, 1, 1);

    TelnetServer.telnetPrint("[MQTT] Auto-discovery published\r\n");
}

void MqttHandler::sendState()
{
    cJSON *jsonData = MeterT550.getJsonData();
    if (jsonData == nullptr || cJSON_GetArraySize(jsonData) == 0)
    {
        TelnetServer.telnetPrint("[MQTT] No sensor data to publish\r\n");
        ESP_LOGW(TAG, "No sensor data to publish");
        return;
    }

    char *jsonString = cJSON_PrintUnformatted(jsonData);
    if (jsonString != nullptr)
    {
        TelnetServer.telnetPrint("[MQTT] Publishing state\r\n");
        ESP_LOGI(TAG, "Publishing state to topic %s: %s", ConfigManager.mqttTopic.c_str(), jsonString);

        if (clientHandle != nullptr && isConnectedState)
        {
            esp_mqtt_client_publish(clientHandle, ConfigManager.mqttTopic.c_str(), jsonString, 0, 0, 0);
        }

        cJSON_free(jsonString);
    }

    MeterT550.clearJsonData();
}

bool MqttHandler::isConnected() const
{
    return isConnectedState;
}
