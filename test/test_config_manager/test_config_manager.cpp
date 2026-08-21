#include <unity.h>
#include <string>

#include "../test_common.h"
#include "ConfigManager.h"

// Include implementation under test
#include "ConfigManager.cpp"

static ConfigManager *testConfig = nullptr;

void setUp(void)
{
    testConfig = new ConfigManager();
}

void tearDown(void)
{
    if (testConfig != nullptr)
    {
        delete testConfig;
        testConfig = nullptr;
    }
}

void test_default_values(void)
{
    TEST_ASSERT_EQUAL_STRING("", testConfig->m_wifiSsid.c_str());
    TEST_ASSERT_EQUAL_STRING("", testConfig->m_wifiPassword.c_str());
    TEST_ASSERT_EQUAL_STRING("192.168.1.10", testConfig->m_mqttServer.c_str());
    TEST_ASSERT_EQUAL_UINT16(1883, testConfig->m_mqttPort);
    TEST_ASSERT_EQUAL_STRING("", testConfig->m_mqttUser.c_str());
    TEST_ASSERT_EQUAL_STRING("", testConfig->m_mqttPassword.c_str());
    TEST_ASSERT_EQUAL_STRING("ultraheat/state", testConfig->m_mqttTopic.c_str());
    TEST_ASSERT_EQUAL_UINT32(3600, testConfig->m_readIntervalSeconds);
    TEST_ASSERT_FALSE(testConfig->m_dummyMode);
}

void test_save_and_load_flow(void)
{
    testConfig->m_wifiSsid = "TestHomeSSID";
    testConfig->m_wifiPassword = "SecretPassword";
    testConfig->m_mqttServer = "10.0.0.5";
    testConfig->m_mqttPort = 1884;
    testConfig->m_mqttUser = "mqttuser";
    testConfig->m_mqttPassword = "mqttpass";
    testConfig->m_mqttTopic = "custom/heatmeter";
    testConfig->m_readIntervalSeconds = 600;
    testConfig->m_dummyMode = true;

    // Call saveConfig - should execute cleanly via NVS mock
    testConfig->saveConfig();

    TEST_ASSERT_EQUAL_STRING("TestHomeSSID", testConfig->m_wifiSsid.c_str());
    TEST_ASSERT_EQUAL_STRING("SecretPassword", testConfig->m_wifiPassword.c_str());
    TEST_ASSERT_EQUAL_STRING("10.0.0.5", testConfig->m_mqttServer.c_str());
    TEST_ASSERT_EQUAL_UINT16(1884, testConfig->m_mqttPort);
    TEST_ASSERT_EQUAL_STRING("mqttuser", testConfig->m_mqttUser.c_str());
    TEST_ASSERT_EQUAL_STRING("mqttpass", testConfig->m_mqttPassword.c_str());
    TEST_ASSERT_EQUAL_STRING("custom/heatmeter", testConfig->m_mqttTopic.c_str());
    TEST_ASSERT_EQUAL_UINT32(600, testConfig->m_readIntervalSeconds);
    TEST_ASSERT_TRUE(testConfig->m_dummyMode);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_default_values);
    RUN_TEST(test_save_and_load_flow);
    return UNITY_END();
}