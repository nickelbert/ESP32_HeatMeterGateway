#include "MeterT550.h"
#include "ConfigManager.h"
#include "TelnetServer.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstring>
#include <cstdlib>

static const char *TAG = "MeterT550";
extern ConfigManager configManager;
extern TelnetServer telnetServer;

static MeterT550 *meterInstance = nullptr;

static uint8_t retryCount = 0;

static const uint8_t nullArray[40] = {0};
static const char reqArray[5] = {'/', '?', '!', '\r', '\n'};

// Forward hook definition for triggering MQTT publish in Phase 7
void triggerMqttPublish();

// Connect Telnet simulation hook to MeterT550
void triggerMeterSimulation(const std::string &line)
{
    if (meterInstance != nullptr)
    {
        meterInstance->simulateData(line);
    }
}

static uint32_t getMillis()
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static bool isNumeric(const std::string &str)
{
    if (str.empty())
    {
        return false;
    }

    bool hasDecimal = false;
    for (size_t i = 0; i < str.length(); i++)
    {
        char c = str[i];
        if (i == 0 && c == '-')
        {
            continue;
        }
        if (c == '.')
        {
            if (hasDecimal)
            {
                return false;
            }
            hasDecimal = true;
            continue;
        }
        if (c < '0' || c > '9')
        {
            return false;
        }
    }
    return true;
}

static std::string cleanValue(const std::string &val)
{
    std::string res = "";
    for (char c : val)
    {
        if ((c >= '0' && c <= '9') || c == '.' || c == '-' || c == ':' || c == '&' || c == ' ')
        {
            res += c;
        }
        else
        {
            break;
        }
    }
    return res;
}

void MeterT550::configureUart(uint32_t baudRate)
{
    uart_config_t uartConfig = {};
    uartConfig.baud_rate = (int)baudRate;
    uartConfig.data_bits = UART_DATA_7_BITS;
    uartConfig.parity = UART_PARITY_EVEN;
    uartConfig.stop_bits = UART_STOP_BITS_1;
    uartConfig.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uartConfig.rx_flow_ctrl_thresh = 122;
    uartConfig.source_clk = UART_SCLK_DEFAULT;

    ESP_ERROR_CHECK(uart_param_config(uartPort, &uartConfig));
}

MeterT550::MeterT550()
{
    m_sensorDataJson = cJSON_CreateObject();
}

MeterT550::~MeterT550()
{
    if (m_sensorDataJson != nullptr)
    {
        cJSON_Delete(m_sensorDataJson);
        m_sensorDataJson = nullptr;
    }
}

void MeterT550::setup()
{
    meterInstance = this;
    if (m_sensorDataJson == nullptr)
    {
        m_sensorDataJson = cJSON_CreateObject();
    }

    ESP_ERROR_CHECK(uart_driver_install(uartPort, 1024, 0, 0, nullptr, 0));
    ESP_ERROR_CHECK(uart_set_pin(uartPort, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    configureUart(300);

    xTaskCreate(
        &MeterT550::meterTask,
        "meterTask",
        4096,
        this,
        4,
        &m_taskHandle);

    ESP_LOGI(TAG, "MeterT550 task started (UART1 on RX=%d, TX=%d)", rxPin, txPin);
}

void MeterT550::meterTask(void *pvParameters)
{
    MeterT550 *self = static_cast<MeterT550 *>(pvParameters);

    while (1)
    {
        uint32_t now = getMillis();

        switch (self->m_currentState)
        {
            case MeterState::Idle:
            {
                if (configManager.m_dummyMode)
                {
                    vTaskDelay(pdMS_TO_TICKS(100));
                    break;
                }

                uint32_t intervalMs = configManager.m_readIntervalSeconds * 1000UL;
                if ((now - self->m_lastReadingTimestamp > intervalMs) || (self->m_lastReadingTimestamp == 0))
                {
                    telnetServer.telnetPrint("[Meter] Starting readout sequence\r\n");
                    ESP_LOGI(TAG, "Starting readout sequence");

                    self->m_identifier.clear();
                    self->m_receiveBuffer.clear();
                    self->m_lastReadingTimestamp = now;
                    self->m_timestampSend = now;

                    self->configureUart(300);
                    self->m_currentState = MeterState::Wakeup;
                }
                else
                {
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                break;
            }

            case MeterState::Wakeup:
            {
                self->sendWakeup();
                self->m_timestampSend = getMillis();
                self->m_currentState = MeterState::ReadIdent;
                break;
            }

            case MeterState::ReadIdent:
            {
                self->readIdent();
                vTaskDelay(pdMS_TO_TICKS(20));
                break;
            }

            case MeterState::ReceivingData:
            {
                self->receiveMeterData();
                vTaskDelay(pdMS_TO_TICKS(20));
                break;
            }
        }
    }
}

void MeterT550::sendWakeup()
{
    uart_write_bytes(uartPort, (const char *)nullArray, sizeof(nullArray));
    uart_write_bytes(uartPort, reqArray, sizeof(reqArray));
    uart_wait_tx_done(uartPort, pdMS_TO_TICKS(200));
    uart_flush_input(uartPort);
}

void MeterT550::readIdent()
{
    uint8_t ch;
    while (uart_read_bytes(uartPort, &ch, 1, 0) > 0)
    {
        m_timestampSend = getMillis();
        char c = (char)ch;

        if (c == '\n')
        {
            telnetServer.telnetPrint("[Meter] Identification: " + m_identifier + "\r\n");
            ESP_LOGI(TAG, "Identification received: %s", m_identifier.c_str());

            configureUart(19200);
            m_timestampSend = getMillis();
            m_currentState = MeterState::ReceivingData;
            return;
        }
        else if (c != '\r')
        {
            if (m_identifier.length() < 64)
            {
                m_identifier += c;
            }
        }
    }

    if ((getMillis() - m_timestampSend) > 5000)
    {
        ESP_LOGW(TAG, "Timeout waiting for identification");
        m_currentState = MeterState::Idle;
        m_identifier.clear();

        uint32_t intervalMs = configManager.m_readIntervalSeconds * 1000UL;
        if (retryCount < 1 && intervalMs > 30000)
        {
            retryCount++;
            telnetServer.telnetPrint("[Meter] Readout failed. Retrying once in 30s...\r\n");
            m_lastReadingTimestamp = getMillis() - intervalMs + 30000;
        }
        else
        {
            retryCount = 0;
            telnetServer.telnetPrint("[Meter] Readout failed. Next try in regular interval.\r\n");
        }
    }
}

void MeterT550::receiveMeterData()
{
    uint8_t ch;
    while (uart_read_bytes(uartPort, &ch, 1, 0) > 0)
    {
        m_timestampSend = getMillis();
        char c = (char)ch;

        if (c == '\n')
        {
            parseMeterLine(m_receiveBuffer);
            m_receiveBuffer.clear();
        }
        else if (c != '\r')
        {
            if (m_receiveBuffer.length() < 512)
            {
                m_receiveBuffer += c;
            }
            else
            {
                m_receiveBuffer.clear();
            }
        }
    }

        if ((getMillis() - m_timestampSend) > 2500)
    {
        telnetServer.telnetPrint("[Meter] Readout complete\r\n");
        ESP_LOGI(TAG, "Readout complete. Triggering MQTT publish");
        retryCount = 0;
        m_currentState = MeterState::Idle;
        m_receiveBuffer.clear();
        triggerMqttPublish();
    }
}

void MeterT550::parseMeterLine(const std::string &line)
{
    std::string trimmedLine = line;
    while (!trimmedLine.empty() && (trimmedLine.front() == ' ' || trimmedLine.front() == '\t'))
    {
        trimmedLine.erase(trimmedLine.begin());
    }
    while (!trimmedLine.empty() && (trimmedLine.back() == ' ' || trimmedLine.back() == '\t' || trimmedLine.back() == '\r'))
    {
        trimmedLine.pop_back();
    }

    if (trimmedLine.empty())
    {
        return;
    }

    size_t posOpen = trimmedLine.find('(');
    size_t posClose = trimmedLine.find(')');

    while (posOpen != std::string::npos && posClose != std::string::npos && posOpen < posClose)
    {
        std::string obisCode = trimmedLine.substr(0, posOpen);
        std::string strValue = trimmedLine.substr(posOpen + 1, posClose - posOpen - 1);

        while (!obisCode.empty() && (obisCode.front() == ' ' || obisCode.front() == '\t')) obisCode.erase(obisCode.begin());
        while (!obisCode.empty() && (obisCode.back() == ' ' || obisCode.back() == '\t')) obisCode.pop_back();

        while (!strValue.empty() && (strValue.front() == ' ' || strValue.front() == '\t')) strValue.erase(strValue.begin());
        while (!strValue.empty() && (strValue.back() == ' ' || strValue.back() == '\t')) strValue.pop_back();

        if (!obisCode.empty() && !strValue.empty())
        {
            std::string logMsg = "[Parse] OBIS: " + obisCode + " | Value: " + strValue + "\r\n";
            telnetServer.telnetPrint(logMsg);
            ESP_LOGI(TAG, "Parsed OBIS: %s = %s", obisCode.c_str(), strValue.c_str());

            addToDataset(obisCode, strValue);
        }

        trimmedLine.erase(0, posClose + 1);
        posOpen = trimmedLine.find('(');
        posClose = trimmedLine.find(')');
    }
}

void MeterT550::addToDataset(const std::string &obis, const std::string &value)
{
    std::string cleanObis = "";
    for (char c : obis)
    {
        if ((unsigned char)c >= 32)
        {
            cleanObis += c;
        }
    }

    std::string val = value;

    // Split dual values (e.g. forward/return temperature separated by '&')
    if (val.find('&') != std::string::npos)
    {
        size_t ampersandPos = val.find('&');
        std::string part0 = cleanValue(val.substr(0, ampersandPos));

        if (isNumeric(part0))
        {
            int partIndex = 0;
            size_t startPos = 0;
            size_t delimPos = val.find('&');

            while (delimPos != std::string::npos)
            {
                std::string partVal = val.substr(startPos, delimPos - startPos);
                std::string cleanPart = cleanValue(partVal);
                std::string key = cleanObis + "." + std::to_string(partIndex);

                cJSON_DeleteItemFromObject(m_sensorDataJson, key.c_str());
                cJSON_AddNumberToObject(m_sensorDataJson, key.c_str(), std::atof(cleanPart.c_str()));

                startPos = delimPos + 1;
                delimPos = val.find('&', startPos);
                partIndex++;
            }

            std::string lastPart = val.substr(startPos);
            std::string cleanPart = cleanValue(lastPart);
            std::string key = cleanObis + "." + std::to_string(partIndex);

            cJSON_DeleteItemFromObject(m_sensorDataJson, key.c_str());
            cJSON_AddNumberToObject(m_sensorDataJson, key.c_str(), std::atof(cleanPart.c_str()));
            return;
        }
    }

    std::string cleanVal = cleanValue(val);
    cJSON_DeleteItemFromObject(m_sensorDataJson, cleanObis.c_str());

    if (isNumeric(cleanVal))
    {
        cJSON_AddNumberToObject(m_sensorDataJson, cleanObis.c_str(), std::atof(cleanVal.c_str()));
    }
    else
    {
        cJSON_AddStringToObject(m_sensorDataJson, cleanObis.c_str(), cleanVal.c_str());
    }
}

void MeterT550::forceReading()
{
    m_lastReadingTimestamp = 0;
}

void MeterT550::simulateData(const std::string &line)
{
    parseMeterLine(line);
}

cJSON *MeterT550::getJsonData()
{
    return m_sensorDataJson;
}

void MeterT550::clearJsonData()
{
    if (m_sensorDataJson != nullptr)
    {
        cJSON_Delete(m_sensorDataJson);
        m_sensorDataJson = cJSON_CreateObject();
    }
}
