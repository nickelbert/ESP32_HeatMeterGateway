#pragma once

#include <string>
#include <cstdint>

class ConfigManager
{
public:
    // WiFi configuration
    std::string wifiSsid = "";
    std::string wifiPassword = "";

    // MQTT broker configuration
    std::string mqttServer = "192.168.1.10";
    uint16_t mqttPort = 1883;
    std::string mqttUser = "";
    std::string mqttPassword = "";
    std::string mqttTopic = "ultraheat/state";

    // Meter and system configuration
    uint32_t readIntervalSeconds = 3600;
    bool dummyMode = false;

    void setup();
    void loadConfig();
    void saveConfig();
};