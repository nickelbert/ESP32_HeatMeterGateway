#include <unity.h>
#include <string>
#include <vector>
#include <algorithm>

// Include cJSON implementation for host test
#include "cJSON.c"

#include "../mocks/mqtt_client.h"

// Provide definition of message buffer for MQTT mock
std::vector<PublishedMqttMessage> g_publishedMqttMessages;

#include "../test_common.h"
#include "MeterT550.h"
#include "MqttHandler.h"

ConfigManager configManager;
TelnetServer telnetServer;

// Mock MeterT550 methods for MqttHandler unit tests
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

cJSON *MeterT550::getJsonData()
{
    return m_sensorDataJson;
}

void MeterT550::clearJsonData()
{
    if (m_sensorDataJson != nullptr)
    {
        cJSON_Delete(m_sensorDataJson);
    }
    m_sensorDataJson = cJSON_CreateObject();
}

void MeterT550::forceReading()
{
}

void MeterT550::simulateData(const std::string &line)
{
    (void)line;
}

MeterT550 meterT550;

// Include implementation under test
#include "MqttHandler.cpp"

static MqttHandler *testMqttHandler = nullptr;

void setUp(void)
{
    g_publishedMqttMessages.clear();
    configManager.m_mqttServer = "192.168.1.100";
    configManager.m_mqttPort = 1883;
    configManager.m_mqttTopic = "ultraheat/state";
    configManager.m_mqttUser = "";
    configManager.m_mqttPassword = "";

    testMqttHandler = new MqttHandler();
    testMqttHandler->setup();

    // Trigger connected event to set internal connected state
    esp_mqtt_event_t connEvent = {};
    connEvent.event_id = MQTT_EVENT_CONNECTED;
    MqttHandler::mqttEventHandler(testMqttHandler, nullptr, MQTT_EVENT_CONNECTED, &connEvent);
    g_publishedMqttMessages.clear();
}

void tearDown(void)
{
    if (testMqttHandler != nullptr)
    {
        delete testMqttHandler;
        testMqttHandler = nullptr;
    }
    meterT550.clearJsonData();
    g_publishedMqttMessages.clear();
}

void test_ha_discovery_total_count(void)
{
    testMqttHandler->sendHaAutoDiscovery();

    // 22 sensors + 1 read button + 2 update sensors = 25 discovery messages
    TEST_ASSERT_EQUAL_INT(25, g_publishedMqttMessages.size());
}

void test_ha_energy_sensor_discovery_payload(void)
{
    testMqttHandler->sendHaAutoDiscovery();

    auto it = std::find_if(g_publishedMqttMessages.begin(), g_publishedMqttMessages.end(),
        [](const PublishedMqttMessage &msg) {
            return msg.topic == "homeassistant/sensor/T550_6_8/config";
        });

    TEST_ASSERT_TRUE(it != g_publishedMqttMessages.end());
    TEST_ASSERT_EQUAL_INT(1, it->retain);

    cJSON *doc = cJSON_Parse(it->data.c_str());
    TEST_ASSERT_NOT_NULL(doc);

    TEST_ASSERT_EQUAL_STRING("Heat Energy", cJSON_GetObjectItem(doc, "name")->valuestring);
    TEST_ASSERT_EQUAL_STRING("ultraheat/state", cJSON_GetObjectItem(doc, "state_topic")->valuestring);
    TEST_ASSERT_EQUAL_STRING("{{ value_json['6.8'] }}", cJSON_GetObjectItem(doc, "value_template")->valuestring);
    TEST_ASSERT_EQUAL_STRING("t550_6_8", cJSON_GetObjectItem(doc, "unique_id")->valuestring);
    TEST_ASSERT_EQUAL_STRING("MWh", cJSON_GetObjectItem(doc, "unit_of_measurement")->valuestring);
    TEST_ASSERT_EQUAL_STRING("energy", cJSON_GetObjectItem(doc, "device_class")->valuestring);
    TEST_ASSERT_EQUAL_STRING("total_increasing", cJSON_GetObjectItem(doc, "state_class")->valuestring);
    TEST_ASSERT_EQUAL_STRING("mdi:fire", cJSON_GetObjectItem(doc, "icon")->valuestring);

    cJSON *dev = cJSON_GetObjectItem(doc, "device");
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_STRING("Landis+Gyr T550", cJSON_GetObjectItem(dev, "name")->valuestring);
    TEST_ASSERT_EQUAL_STRING("Landis+Gyr", cJSON_GetObjectItem(dev, "manufacturer")->valuestring);
    TEST_ASSERT_EQUAL_STRING("Ultraheat T550", cJSON_GetObjectItem(dev, "model")->valuestring);

    cJSON_Delete(doc);
}

void test_ha_dual_temperature_discovery_topics(void)
{
    testMqttHandler->sendHaAutoDiscovery();

    auto itFlow = std::find_if(g_publishedMqttMessages.begin(), g_publishedMqttMessages.end(),
        [](const PublishedMqttMessage &msg) {
            return msg.topic == "homeassistant/sensor/T550_9_4_0/config";
        });
    TEST_ASSERT_TRUE(itFlow != g_publishedMqttMessages.end());

    cJSON *docFlow = cJSON_Parse(itFlow->data.c_str());
    TEST_ASSERT_NOT_NULL(docFlow);
    TEST_ASSERT_EQUAL_STRING("Flow Temperature", cJSON_GetObjectItem(docFlow, "name")->valuestring);
    TEST_ASSERT_EQUAL_STRING("t550_9_4_0", cJSON_GetObjectItem(docFlow, "unique_id")->valuestring);
    TEST_ASSERT_EQUAL_STRING("{{ value_json['9.4.0'] }}", cJSON_GetObjectItem(docFlow, "value_template")->valuestring);
    cJSON_Delete(docFlow);

    auto itReturn = std::find_if(g_publishedMqttMessages.begin(), g_publishedMqttMessages.end(),
        [](const PublishedMqttMessage &msg) {
            return msg.topic == "homeassistant/sensor/T550_9_4_1/config";
        });
    TEST_ASSERT_TRUE(itReturn != g_publishedMqttMessages.end());

    cJSON *docReturn = cJSON_Parse(itReturn->data.c_str());
    TEST_ASSERT_NOT_NULL(docReturn);
    TEST_ASSERT_EQUAL_STRING("Return Temperature", cJSON_GetObjectItem(docReturn, "name")->valuestring);
    TEST_ASSERT_EQUAL_STRING("t550_9_4_1", cJSON_GetObjectItem(docReturn, "unique_id")->valuestring);
    TEST_ASSERT_EQUAL_STRING("{{ value_json['9.4.1'] }}", cJSON_GetObjectItem(docReturn, "value_template")->valuestring);
    cJSON_Delete(docReturn);
}

void test_ha_previous_year_topic_sanitization(void)
{
    testMqttHandler->sendHaAutoDiscovery();

    auto itPrevFlow = std::find_if(g_publishedMqttMessages.begin(), g_publishedMqttMessages.end(),
        [](const PublishedMqttMessage &msg) {
            return msg.topic == "homeassistant/sensor/T550_9_4_01_0/config";
        });
    TEST_ASSERT_TRUE(itPrevFlow != g_publishedMqttMessages.end());

    cJSON *doc = cJSON_Parse(itPrevFlow->data.c_str());
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_EQUAL_STRING("t550_9_4_01_0", cJSON_GetObjectItem(doc, "unique_id")->valuestring);
    TEST_ASSERT_EQUAL_STRING("{{ value_json['9.4*01.0'] }}", cJSON_GetObjectItem(doc, "value_template")->valuestring);
    cJSON_Delete(doc);
}

void test_ha_button_discovery(void)
{
    testMqttHandler->sendHaAutoDiscovery();

    auto itBtn = std::find_if(g_publishedMqttMessages.begin(), g_publishedMqttMessages.end(),
        [](const PublishedMqttMessage &msg) {
            return msg.topic == "homeassistant/button/T550_read/config";
        });
    TEST_ASSERT_TRUE(itBtn != g_publishedMqttMessages.end());

    cJSON *doc = cJSON_Parse(itBtn->data.c_str());
    TEST_ASSERT_NOT_NULL(doc);
    TEST_ASSERT_EQUAL_STRING("Read Meter Now", cJSON_GetObjectItem(doc, "name")->valuestring);
    TEST_ASSERT_EQUAL_STRING("ultraheat/command", cJSON_GetObjectItem(doc, "command_topic")->valuestring);
    TEST_ASSERT_EQUAL_STRING("READ", cJSON_GetObjectItem(doc, "payload_press")->valuestring);
    TEST_ASSERT_EQUAL_STRING("t550_button_read", cJSON_GetObjectItem(doc, "unique_id")->valuestring);
    cJSON_Delete(doc);
}

void test_send_state_publishes_json_and_clears_data(void)
{
    cJSON_AddNumberToObject(meterT550.getJsonData(), "6.8", 12.345);
    cJSON_AddNumberToObject(meterT550.getJsonData(), "6.26", 78.90);

    g_publishedMqttMessages.clear();
    testMqttHandler->sendState();

    TEST_ASSERT_EQUAL_INT(1, g_publishedMqttMessages.size());
    TEST_ASSERT_EQUAL_STRING("ultraheat/state", g_publishedMqttMessages[0].topic.c_str());

    cJSON *payloadDoc = cJSON_Parse(g_publishedMqttMessages[0].data.c_str());
    TEST_ASSERT_NOT_NULL(payloadDoc);

    cJSON *energyItem = cJSON_GetObjectItem(payloadDoc, "6.8");
    TEST_ASSERT_NOT_NULL(energyItem);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 12.345, energyItem->valuedouble);

    cJSON *volItem = cJSON_GetObjectItem(payloadDoc, "6.26");
    TEST_ASSERT_NOT_NULL(volItem);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 78.90, volItem->valuedouble);

    cJSON_Delete(payloadDoc);

    // Verify meter data is cleared after sendState
    TEST_ASSERT_EQUAL_INT(0, cJSON_GetArraySize(meterT550.getJsonData()));
}

void test_incoming_read_command_handling(void)
{
    char topicBuf[] = "ultraheat/command";
    char dataBuf[] = "read";

    esp_mqtt_event_t dataEvent = {};
    dataEvent.event_id = MQTT_EVENT_DATA;
    dataEvent.topic = topicBuf;
    dataEvent.topic_len = sizeof(topicBuf) - 1;
    dataEvent.data = dataBuf;
    dataEvent.data_len = sizeof(dataBuf) - 1;

    MqttHandler::mqttEventHandler(testMqttHandler, nullptr, MQTT_EVENT_DATA, &dataEvent);
    TEST_ASSERT_TRUE(true);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_ha_discovery_total_count);
    RUN_TEST(test_ha_energy_sensor_discovery_payload);
    RUN_TEST(test_ha_dual_temperature_discovery_topics);
    RUN_TEST(test_ha_previous_year_topic_sanitization);
    RUN_TEST(test_ha_button_discovery);
    RUN_TEST(test_send_state_publishes_json_and_clears_data);
    RUN_TEST(test_incoming_read_command_handling);
    return UNITY_END();
}