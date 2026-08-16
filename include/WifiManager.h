#pragma once

#include <string>
#include <cstdint>
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_timer.h"

class WifiManager
{
private:
    bool isConnectedState = false;
    bool isApModeActive = false;
    std::string ipAddress = "";
    esp_netif_t *staNetif = nullptr;
    esp_netif_t *apNetif = nullptr;
    esp_timer_handle_t reconnectTimer = nullptr;

    static void eventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId, void *eventData);
    static void reconnectTimerCallback(void *arg);
    void startReconnectTimer();
    void stopReconnectTimer();

public:
    void setup();
    bool isConnected() const;
    bool isApMode() const;
    std::string getIpAddress() const;
    void startAccessPoint();
    void startStation();
};
