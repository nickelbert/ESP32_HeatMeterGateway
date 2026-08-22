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

#ifndef IR_RX_PIN
#if CONFIG_IDF_TARGET_ESP32S3
#define IR_RX_PIN 18
#define IR_TX_PIN 17
#else
#define IR_RX_PIN 20
#define IR_TX_PIN 21
#endif
#endif

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
    static const int rxPin = IR_RX_PIN;
    static const int txPin = IR_TX_PIN;

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
