#include "TelnetServer.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <cstring>
#include <algorithm>

static const char *TAG = "TelnetServer";
static TelnetServer *telnetServerInstance = nullptr;


__attribute__((weak)) void triggerMqttPublish()
{
    ESP_LOGI(TAG, "MQTT publish trigger requested via Telnet");
}

__attribute__((weak)) void triggerMeterSimulation(const std::string &line)
{
    ESP_LOGI(TAG, "Simulated meter data received via Telnet: %s", line.c_str());
}

void TelnetServer::setup()
{
    telnetServerInstance = this;

    xTaskCreate(
        &TelnetServer::telnetTask,
        "telnetTask",
        4096,
        this,
        5,
        &taskHandle);

    ESP_LOGI(TAG, "Telnet server task started on port 23");
}

void TelnetServer::telnetTask(void *pvParameters)
{
    TelnetServer *self = static_cast<TelnetServer *>(pvParameters);

    struct sockaddr_in serverAddr = {};
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(23);

    while (1)
    {
        self->serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (self->serverSocket < 0)
        {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        int opt = 1;
        setsockopt(self->serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        int err = bind(self->serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
        if (err != 0)
        {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
            close(self->serverSocket);
            self->serverSocket = -1;
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        err = listen(self->serverSocket, 1);
        if (err != 0)
        {
            ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
            close(self->serverSocket);
            self->serverSocket = -1;
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        ESP_LOGI(TAG, "Telnet socket listening on port 23");

        while (1)
        {
            struct sockaddr_in sourceAddr;
            socklen_t addrLen = sizeof(sourceAddr);
            int sock = accept(self->serverSocket, (struct sockaddr *)&sourceAddr, &addrLen);

            if (sock < 0)
            {
                ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
                break;
            }

            // Only allow one active client at a time
            if (self->clientSocket >= 0)
            {
                const char *busyMsg = "[Telnet] Server busy. Another client is connected.\r\n";
                send(sock, busyMsg, strlen(busyMsg), 0);
                close(sock);
                continue;
            }

            self->clientSocket = sock;
            self->handleClient(sock);
            self->clientSocket = -1;
        }

        if (self->serverSocket >= 0)
        {
            close(self->serverSocket);
            self->serverSocket = -1;
        }
    }
}

void TelnetServer::handleClient(int sock)
{
    ESP_LOGI(TAG, "Client connected to Telnet server");

    const char *welcomeMsg = "[Telnet] Connected to ESP Heat Meter (OTA Debug Mode)\r\n";
    send(sock, welcomeMsg, strlen(welcomeMsg), 0);

    char rxBuffer[256];
    std::string currentLine = "";

    while (1)
    {
        int len = recv(sock, rxBuffer, sizeof(rxBuffer) - 1, 0);
        if (len <= 0)
        {
            ESP_LOGI(TAG, "Telnet client disconnected");
            break;
        }

        for (int i = 0; i < len; i++)
        {
            char c = rxBuffer[i];
            if (c == '\n')
            {
                // Trim trailing '\r'
                if (!currentLine.empty() && currentLine.back() == '\r')
                {
                    currentLine.pop_back();
                }

                if (!currentLine.empty())
                {
                    processLine(currentLine);
                    currentLine.clear();
                }
            }
            else if (c != '\r')
            {
                if (currentLine.length() < 512)
                {
                    currentLine += c;
                }
            }
        }
    }

    close(sock);
}

void TelnetServer::processLine(const std::string &line)
{
    ESP_LOGI(TAG, "Telnet command received: '%s'", line.c_str());

    std::string lower = line;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "update")
    {
        telnetPrint("[Telnet] Update command received. Ready for OTA upload\r\n");
    }
    else if (lower == "send")
    {
        telnetPrint("[Telnet] Triggering MQTT publish...\r\n");
        triggerMqttPublish();
    }
    else
    {
        triggerMeterSimulation(line);
    }
}

void TelnetServer::telnetPrint(const char *msg)
{
    if (clientSocket >= 0 && msg != nullptr)
    {
        send(clientSocket, msg, strlen(msg), 0);
    }
}

void TelnetServer::telnetPrint(const std::string &msg)
{
    telnetPrint(msg.c_str());
}

bool TelnetServer::hasClient() const
{
    return clientSocket >= 0;
}
