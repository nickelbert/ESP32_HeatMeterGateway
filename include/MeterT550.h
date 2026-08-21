#pragma once

#include <string>
#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "cJSON.h"

enum class MeterState
{
    Idle,
    Wakeup,
    ReadIdent,
    ReceivingData
};

class MeterT550
{
private:
    MeterState m_currentState = MeterState::Idle;
    uint32_t m_timestampSend = 0;
    uint32_t m_lastReadingTimestamp = 0;
    std::string m_identifier = "";
    std::string m_receiveBuffer = "";
    TaskHandle_t m_taskHandle = nullptr;
    cJSON *m_sensorDataJson = nullptr;

    static const uart_port_t uartPort = UART_NUM_1;
    static const int rxPin = 20;
    static const int txPin = 21;

    void configureUart(uint32_t baudRate);
    void sendWakeup();
    void readIdent();
    void receiveMeterData();
    void parseMeterLine(const std::string &line);
    void addToDataset(const std::string &obis, const std::string &value);

    static void meterTask(void *pvParameters);

public:
    MeterT550();
    ~MeterT550();
    void setup();
    void forceReading();
    void simulateData(const std::string &line);
    cJSON *getJsonData();
    void clearJsonData();
};
