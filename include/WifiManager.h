#pragma once

#include <string>
#include <cstdint>
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_timer.h"

class WifiManager
{
private:
    static constexpr int kMaxRetryAttempts = 10;
    static constexpr uint64_t kReconnectIntervalUs = 60000000ULL; // 60 seconds

    bool m_isConnectedState = false;
    bool m_isApModeActive = false;
    int m_retryCount = 0;
    int m_apClientCount = 0;
    std::string m_ipAddress = "";
    esp_netif_t *m_staNetif = nullptr;
    esp_netif_t *m_apNetif = nullptr;
    esp_timer_handle_t m_reconnectTimer = nullptr;

    static void eventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId, void *eventData);
    static void reconnectTimerCallback(void *arg);
    void startReconnectTimer();
    void stopReconnectTimer();

public:
    void setup();
    bool isConnected() const;
    bool isApMode() const;
    int getApClientCount() const;
    std::string getIpAddress() const;
    void startAccessPoint();
    void startStation();
};

