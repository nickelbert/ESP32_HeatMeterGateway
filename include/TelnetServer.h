#pragma once

#include <string>
#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class TelnetServer
{
private:
    int m_serverSocket = -1;
    int m_clientSocket = -1;
    TaskHandle_t m_taskHandle = nullptr;

    static void telnetTask(void *pvParameters);
    void handleClient(int sock);
    void processLine(const std::string &line);

public:
    void setup();
    void telnetPrint(const char *msg);
    void telnetPrint(const std::string &msg);
    bool hasClient() const;
};
