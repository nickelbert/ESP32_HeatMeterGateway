#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "ConfigManager.h"
#include "WifiManager.h"
#include "TelnetServer.h"
#include "WebServer.h"
#include "MeterT550.h"
#include "MqttHandler.h"

static const char *TAG = "MAIN";

ConfigManager ConfigManager;
WifiManager WifiManager;
TelnetServer TelnetServer;
WebServer WebServer;
MeterT550 MeterT550;
MqttHandler MqttHandler;

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, " Landis+Gyr T550 Gateway (ESP32-C3) starting... ");
    ESP_LOGI(TAG, "==================================================");

    ConfigManager.setup();

    ESP_LOGI(TAG, "Loaded WiFi SSID: %s", ConfigManager.wifiSsid.empty() ? "(empty)" : ConfigManager.wifiSsid.c_str());
    ESP_LOGI(TAG, "Loaded MQTT Server: %s:%u", ConfigManager.mqttServer.c_str(), ConfigManager.mqttPort);
    ESP_LOGI(TAG, "Loaded Read Interval: %lu s", ConfigManager.readIntervalSeconds);
    ESP_LOGI(TAG, "Loaded Dummy Mode: %s", ConfigManager.dummyMode ? "enabled" : "disabled");

    WifiManager.setup();
    TelnetServer.setup();
    WebServer.setup();
    MeterT550.setup();
    MqttHandler.setup();

    while (1)
    {
        ESP_LOGI(TAG, "[Main] Heartbeat tick... (WiFi: %s, IP: %s, MQTT: %s, Telnet: %s)", 
                 WifiManager.isConnected() ? "Connected" : (WifiManager.isApMode() ? "SoftAP" : "Connecting"),
                 WifiManager.getIpAddress().c_str(),
                 MqttHandler.isConnected() ? "Connected" : "Disconnected",
                 TelnetServer.hasClient() ? "Client Connected" : "No Client");

        if (TelnetServer.hasClient())
        {
            TelnetServer.telnetPrint("[ESP32] Heartbeat tick...\r\n");
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
