#include "WifiManager.h"
#include "ConfigManager.h"
#include "esp_log.h"
#include "esp_event.h"
#include <cstring>

static const char *TAG = "WifiManager";
extern ConfigManager ConfigManager;

static int retryCount = 0;
static const int maxRetryAttempts = 10;

void WifiManager::eventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId, void *eventData)
{
    WifiManager *self = static_cast<WifiManager *>(arg);

    if (eventBase == WIFI_EVENT)
    {
        if (eventId == WIFI_EVENT_STA_START)
        {
            ESP_LOGI(TAG, "WiFi station started, connecting to AP...");
            esp_wifi_connect();
        }
        else if (eventId == WIFI_EVENT_STA_DISCONNECTED)
        {
            self->isConnectedState = false;
            self->ipAddress = "";

            if (retryCount < maxRetryAttempts)
            {
                retryCount++;
                ESP_LOGW(TAG, "WiFi disconnected. Retrying connection (%d/%d)...", retryCount, maxRetryAttempts);
                esp_wifi_connect();
            }
            else
            {
                ESP_LOGE(TAG, "WiFi connection failed after %d retries. Starting fallback AP mode...", maxRetryAttempts);
                self->startAccessPoint();
            }
        }
        else if (eventId == WIFI_EVENT_AP_STACONNECTED)
        {
            wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)eventData;
            ESP_LOGI(TAG, "Client joined softAP (AID: %d)", event->aid);
        }
        else if (eventId == WIFI_EVENT_AP_STADISCONNECTED)
        {
            wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)eventData;
            ESP_LOGI(TAG, "Client left softAP (AID: %d)", event->aid);
        }
    }
    else if (eventBase == IP_EVENT)
    {
        if (eventId == IP_EVENT_STA_GOT_IP)
        {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)eventData;
            char ipBuf[16];
            esp_ip4addr_ntoa(&event->ip_info.ip, ipBuf, sizeof(ipBuf));
            self->isConnectedState = true;
            self->ipAddress = ipBuf;
            retryCount = 0;
            self->stopReconnectTimer();
            if (self->isApModeActive)
            {
                ESP_LOGI(TAG, "Reconnected to home network! Disabling fallback AP...");
                esp_wifi_set_mode(WIFI_MODE_STA);
                self->isApModeActive = false;
            }
            ESP_LOGI(TAG, "WiFi connected! IP Address: %s", self->ipAddress.c_str());
        }
    }
}

void WifiManager::reconnectTimerCallback(void *arg)
{
    WifiManager *self = static_cast<WifiManager *>(arg);
    if (!self->isConnectedState && !ConfigManager.wifiSsid.empty())
    {
        ESP_LOGI(TAG, "Background reconnect attempt to SSID: %s", ConfigManager.wifiSsid.c_str());
        esp_wifi_connect();
    }
}

void WifiManager::startReconnectTimer()
{
    if (reconnectTimer == nullptr)
    {
        esp_timer_create_args_t timerArgs = {};
        timerArgs.callback = &WifiManager::reconnectTimerCallback;
        timerArgs.arg = this;
        timerArgs.name = "wifiReconnect";
        ESP_ERROR_CHECK(esp_timer_create(&timerArgs, &reconnectTimer));
        // 15.000.000 microseconds
        ESP_ERROR_CHECK(esp_timer_start_periodic(reconnectTimer, 15000000));
        ESP_LOGI(TAG, "Background reconnect timer started (15s interval)");
    }
}
void WifiManager::stopReconnectTimer()
{
    if (reconnectTimer != nullptr)
    {
        esp_timer_stop(reconnectTimer);
        esp_timer_delete(reconnectTimer);
        reconnectTimer = nullptr;
        ESP_LOGI(TAG, "Background reconnect timer stopped");
    }
}

void WifiManager::setup()
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    staNetif = esp_netif_create_default_wifi_sta();
    apNetif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &WifiManager::eventHandler,
        this,
        nullptr));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &WifiManager::eventHandler,
        this,
        nullptr));

    if (ConfigManager.wifiSsid.empty())
    {
        ESP_LOGW(TAG, "No WiFi SSID configured. Starting SoftAP setup mode directly...");
        startAccessPoint();
    }
    else
    {
        startStation();
    }
}

void WifiManager::startStation()
{
    isApModeActive = false;
    isConnectedState = false;
    retryCount = 0;

    wifi_config_t staConfig = {};
    std::strncpy((char *)staConfig.sta.ssid, ConfigManager.wifiSsid.c_str(), sizeof(staConfig.sta.ssid) - 1);
    std::strncpy((char *)staConfig.sta.password, ConfigManager.wifiPassword.c_str(), sizeof(staConfig.sta.password) - 1);
    staConfig.sta.threshold.authmode = ConfigManager.wifiPassword.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &staConfig));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to SSID: %s", ConfigManager.wifiSsid.c_str());
}

void WifiManager::startAccessPoint()
{
    isApModeActive = true;
    isConnectedState = false;
    wifi_config_t apConfig = {};
    const char *apSsid = "ESP-HeatMeter-Setup";
    std::strncpy((char *)apConfig.ap.ssid, apSsid, sizeof(apConfig.ap.ssid) - 1);
    apConfig.ap.ssid_len = std::strlen(apSsid);
    apConfig.ap.channel = 1;
    apConfig.ap.max_connection = 4;
    apConfig.ap.authmode = WIFI_AUTH_OPEN;
    if (ConfigManager.wifiSsid.empty())
    {
        // Mode AP
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &apConfig));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_LOGI(TAG, "SoftAP started: SSID '%s' (IP: 192.168.4.1)", apSsid);
    }
    else
    {
        // Dual-Mode AP STA
        wifi_config_t staConfig = {};
        std::strncpy((char *)staConfig.sta.ssid, ConfigManager.wifiSsid.c_str(), sizeof(staConfig.sta.ssid) - 1);
        std::strncpy((char *)staConfig.sta.password, ConfigManager.wifiPassword.c_str(), sizeof(staConfig.sta.password) - 1);
        staConfig.sta.threshold.authmode = ConfigManager.wifiPassword.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &apConfig));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &staConfig));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_LOGI(TAG, "Fallback APSTA mode started: AP '%s' active, searching for '%s'", 
                 apSsid, ConfigManager.wifiSsid.c_str());
        startReconnectTimer();
    }
}

bool WifiManager::isConnected() const
{
    return isConnectedState;
}

bool WifiManager::isApMode() const
{
    return isApModeActive;
}

std::string WifiManager::getIpAddress() const
{
    return ipAddress;
}
