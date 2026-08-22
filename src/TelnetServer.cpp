#include "TelnetServer.h"
#include "esp_log.h"
#include "esp_app_desc.h"
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

TelnetServer::TelnetServer()
{
    m_socketMutex = xSemaphoreCreateMutex();
}

TelnetServer::~TelnetServer()
{
    if (m_socketMutex != nullptr)
    {
        vSemaphoreDelete(m_socketMutex);
        m_socketMutex = nullptr;
    }
}

void TelnetServer::setup()
{
    telnetServerInstance = this;

    xTaskCreate(
        &TelnetServer::telnetTask,
        "telnetTask",
        8192,
        this,
        5,
        &m_taskHandle);

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
        self->m_serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (self->m_serverSocket < 0)
        {
            ESP_LOGE(TAG, "Unable to create socket: errno %d (%s)", errno, strerror(errno));
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        int opt = 1;
        setsockopt(self->m_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        int err = bind(self->m_serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
        if (err != 0)
        {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d (%s)", errno, strerror(errno));
            close(self->m_serverSocket);
            self->m_serverSocket = -1;
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        err = listen(self->m_serverSocket, 4);
        if (err != 0)
        {
            ESP_LOGE(TAG, "Error occurred during listen: errno %d (%s)", errno, strerror(errno));
            close(self->m_serverSocket);
            self->m_serverSocket = -1;
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        ESP_LOGI(TAG, "Telnet socket listening on port 23");

        while (1)
        {
            struct sockaddr_in sourceAddr;
            socklen_t addrLen = sizeof(sourceAddr);
            int sock = accept(self->m_serverSocket, (struct sockaddr *)&sourceAddr, &addrLen);

            if (sock < 0)
            {
                ESP_LOGE(TAG, "Unable to accept connection: errno %d (%s)", errno, strerror(errno));
                break;
            }

            int noDelay = 1;
            setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &noDelay, sizeof(noDelay));

            if (self->m_socketMutex && xSemaphoreTake(self->m_socketMutex, portMAX_DELAY) == pdTRUE)
            {
                self->m_clientSocket = sock;
                xSemaphoreGive(self->m_socketMutex);
            }

            self->handleClient(sock);

            if (self->m_socketMutex && xSemaphoreTake(self->m_socketMutex, portMAX_DELAY) == pdTRUE)
            {
                self->m_clientSocket = -1;
                xSemaphoreGive(self->m_socketMutex);
            }

            close(sock);
        }

        if (self->m_serverSocket >= 0)
        {
            close(self->m_serverSocket);
            self->m_serverSocket = -1;
        }
    }
}

void TelnetServer::handleClient(int sock)
{
    ESP_LOGI(TAG, "Client connected to Telnet server (socket fd: %d)", sock);

    const esp_app_desc_t *appDesc = esp_app_get_description();
    // Send RFC 854 Telnet negotiation: WILL ECHO, WILL SUPPRESS GO AHEAD, DO SUPPRESS GO AHEAD
    static const uint8_t telnetInit[] = {0xFF, 0xFB, 0x01, 0xFF, 0xFB, 0x03, 0xFF, 0xFD, 0x03};
    send(sock, reinterpret_cast<const char *>(telnetInit), sizeof(telnetInit), 0);
    std::string welcomeMsg = "\r\n==================================================\r\n"
                             " Landis+Gyr T550 Heat Meter Gateway\r\n"
                             " Version: " + std::string(appDesc->version) + " (" + appDesc->date + " " + appDesc->time + ")\r\n"
                             " Commands: 'send' (publish MQTT), 'update' (OTA)\r\n"
                             " Or paste OBIS strings: e.g. 6.8(0012.340*MWh)\r\n"
                             "==================================================\r\n\r\n";
    telnetPrint(welcomeMsg.c_str());

    char rxBuffer[256];
    std::string currentLine = "";

    while (1)
    {
        int len = recv(sock, rxBuffer, sizeof(rxBuffer) - 1, 0);
        if (len < 0)
        {
            ESP_LOGW(TAG, "Telnet recv error: errno %d (%s)", errno, strerror(errno));
            break;
        }
        else if (len == 0)
        {
            ESP_LOGI(TAG, "Telnet client closed connection cleanly");
            break;
        }

        for (int i = 0; i < len; i++)
        {
            uint8_t c = static_cast<uint8_t>(rxBuffer[i]);

            // Handle RFC 854 Telnet IAC command negotiation
            if (c == 0xFF && (i + 2 < len))
            {
                uint8_t cmd = static_cast<uint8_t>(rxBuffer[i + 1]);
                uint8_t opt = static_cast<uint8_t>(rxBuffer[i + 2]);

                if (cmd == 0xFD) // DO -> Reply WONT (0xFC)
                {
                    uint8_t resp[3] = {0xFF, 0xFC, opt};
                    send(sock, reinterpret_cast<const char *>(resp), 3, 0);
                }
                else if (cmd == 0xFB) // WILL -> Reply DONT (0xFE)
                {
                    uint8_t resp[3] = {0xFF, 0xFE, opt};
                    send(sock, reinterpret_cast<const char *>(resp), 3, 0);
                }
                i += 2;
                continue;
            }
            else if (c == 0xFF)
            {
                continue;
            }

            if (c == '\n')
            {
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
            else if (c != '\r' && ((c >= 32 && c <= 126) || c == '\t'))
            {
                if (currentLine.length() < 512)
                {
                    currentLine += static_cast<char>(c);
                }
            }
        }
    }
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
    if (msg == nullptr)
    {
        return;
    }

    if (m_socketMutex != nullptr)
    {
        if (xSemaphoreTake(m_socketMutex, pdMS_TO_TICKS(200)) == pdTRUE)
        {
            if (m_clientSocket >= 0)
            {
                int totalSent = 0;
                int msgLen = strlen(msg);
                while (totalSent < msgLen && m_clientSocket >= 0)
                {
                    int sent = send(m_clientSocket, msg + totalSent, msgLen - totalSent, 0);
                    if (sent <= 0)
                    {
                        ESP_LOGW(TAG, "Socket send failed (errno %d)", errno);
                        break;
                    }
                    totalSent += sent;
                }
            }
            xSemaphoreGive(m_socketMutex);
        }
    }
}

void TelnetServer::telnetPrint(const std::string &msg)
{
    telnetPrint(msg.c_str());
}

bool TelnetServer::hasClient() const
{
    return m_clientSocket >= 0;
}