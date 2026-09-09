#pragma once

#include <string>
#include <cstdint>

#ifndef DEFAULT_GITHUB_REPO
#define DEFAULT_GITHUB_REPO "nickelbert/ESP32_HeatMeterGateway"
#endif

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

    // GitHub OTA configuration
    std::string m_githubRepo = DEFAULT_GITHUB_REPO;
    bool m_githubAutoCheck = false;
    bool m_githubAutoUpdate = false;
    bool m_githubIncludePrerelease = false;
    bool m_githubIncludeNightly = false;

    void setup();
    void loadConfig();
    void saveConfig();
};