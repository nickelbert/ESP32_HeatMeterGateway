#pragma once

#include <string>
#include <cstdint>

class ConfigManager
{
public:
    // WiFi configuration
    std::string m_wifiSsid = "";
    std::string m_wifiPassword = "";

    // MQTT broker configuration
    std::string m_mqttServer = "192.168.1.10";
    uint16_t m_mqttPort = 1883;
    std::string m_mqttUser = "";
    std::string m_mqttPassword = "";
    std::string m_mqttTopic = "ultraheat/state";

    // Meter and system configuration
    uint32_t m_readIntervalSeconds = 3600;
    bool m_dummyMode = false;

    void setup();
    void loadConfig();
    void saveConfig();
};