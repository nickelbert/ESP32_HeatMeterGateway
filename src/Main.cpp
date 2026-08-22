#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_app_desc.h"
#include "ConfigManager.h"
#include "WifiManager.h"
#include "TelnetServer.h"
#include "WebServer.h"
#include "MeterT550.h"
#include "MqttHandler.h"

static const char *TAG = "MAIN";

ConfigManager configManager;
WifiManager wifiManager;
TelnetServer telnetServer;
WebServer webServer;
MeterT550 meterT550;
MqttHandler mqttHandler;

extern "C" void app_main(void)
{
    const esp_app_desc_t *appDesc = esp_app_get_description();

    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, " Landis+Gyr T550 Gateway starting... ");
    ESP_LOGI(TAG, " Firmware Version: %s (Built: %s %s)", appDesc->version, appDesc->date, appDesc->time);
    ESP_LOGI(TAG, " ESP-IDF Version:  %s", appDesc->idf_ver);
    ESP_LOGI(TAG, "==================================================");

    configManager.setup();

    ESP_LOGI(TAG, "Loaded WiFi SSID: %s", configManager.m_wifiSsid.empty() ? "(empty)" : configManager.m_wifiSsid.c_str());
    ESP_LOGI(TAG, "Loaded MQTT Server: %s:%u", configManager.m_mqttServer.c_str(), configManager.m_mqttPort);
    ESP_LOGI(TAG, "Loaded Read Interval: %lu s", configManager.m_readIntervalSeconds);
    ESP_LOGI(TAG, "Loaded Dummy Mode: %s", configManager.m_dummyMode ? "enabled" : "disabled");

    wifiManager.setup();
    telnetServer.setup();
    webServer.setup();
    meterT550.setup();
    mqttHandler.setup();

    while (1)
    {
        ESP_LOGI(TAG, "[Main] Heartbeat tick... (WiFi: %s, IP: %s, MQTT: %s, Telnet: %s)", 
                 wifiManager.isConnected() ? "Connected" : (wifiManager.isApMode() ? "SoftAP" : "Connecting"),
                 wifiManager.getIpAddress().c_str(),
                 mqttHandler.isConnected() ? "Connected" : "Disconnected",
                 telnetServer.hasClient() ? "Client Connected" : "No Client");

        if (telnetServer.hasClient())
        {
            telnetServer.telnetPrint("[ESP32] Heartbeat tick...\r\n");
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
