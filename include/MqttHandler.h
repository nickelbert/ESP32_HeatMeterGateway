#pragma once

#include <string>
#include "mqtt_client.h"
#include "cJSON.h"

class MqttHandler
{
private:
    esp_mqtt_client_handle_t m_clientHandle = nullptr;
    bool m_isConnectedState = false;

    void publishHaSensor(const std::string &obis, const std::string &name, const std::string &unit,
                         const std::string &devClass, const std::string &stateClass, const std::string &icon);   

public:
    static void mqttEventHandler(void *handlerArgs, esp_event_base_t base, int32_t eventId, void *eventData);
    void setup();
    void sendHaAutoDiscovery();
    void sendState();
    bool isConnected() const;
    void sendUpdateState(const std::string &latestVersion, const std::string &releaseUrl, const std::string &title);
    void sendHaUpdateDiscovery();
};
