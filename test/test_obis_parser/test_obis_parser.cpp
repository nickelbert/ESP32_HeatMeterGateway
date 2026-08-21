#include <unity.h>
#include <string>
#include <vector>

// Include cJSON implementation for host test
#include "cJSON.c"

// Mock globals required by MeterT550
#include "../test_common.h"
#include "MeterT550.h"

ConfigManager configManager;
TelnetServer telnetServer;

static std::vector<std::string> g_mqttPublishTriggers;

void triggerMqttPublish()
{
    g_mqttPublishTriggers.push_back("PUBLISH_TRIGGERED");
}

// Include implementation under test
#include "MeterT550.cpp"

static MeterT550 *testMeter = nullptr;

void setUp(void)
{
    testMeter = new MeterT550();
    testMeter->clearJsonData();
    g_mqttPublishTriggers.clear();
}

void tearDown(void)
{
    if (testMeter != nullptr)
    {
        delete testMeter;
        testMeter = nullptr;
    }
}

void test_parse_single_energy_obis(void)
{
    testMeter->simulateData("6.8(0001.234*MWh)");

    cJSON *json = testMeter->getJsonData();
    TEST_ASSERT_NOT_NULL(json);

    cJSON *item = cJSON_GetObjectItem(json, "6.8");
    TEST_ASSERT_NOT_NULL(item);
    TEST_ASSERT_TRUE(cJSON_IsNumber(item));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 1.234, item->valuedouble);
}

void test_parse_volume_obis(void)
{
    testMeter->simulateData("6.26(00056.78*m3)");

    cJSON *json = testMeter->getJsonData();
    TEST_ASSERT_NOT_NULL(json);

    cJSON *item = cJSON_GetObjectItem(json, "6.26");
    TEST_ASSERT_NOT_NULL(item);
    TEST_ASSERT_TRUE(cJSON_IsNumber(item));
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 56.78, item->valuedouble);
}

void test_parse_dual_temperature_obis(void)
{
    testMeter->simulateData("9.4(052.4&038.1*C)");

    cJSON *json = testMeter->getJsonData();
    TEST_ASSERT_NOT_NULL(json);

    cJSON *flowTemp = cJSON_GetObjectItem(json, "9.4.0");
    TEST_ASSERT_NOT_NULL(flowTemp);
    TEST_ASSERT_TRUE(cJSON_IsNumber(flowTemp));
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 52.4, flowTemp->valuedouble);

    cJSON *returnTemp = cJSON_GetObjectItem(json, "9.4.1");
    TEST_ASSERT_NOT_NULL(returnTemp);
    TEST_ASSERT_TRUE(cJSON_IsNumber(returnTemp));
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 38.1, returnTemp->valuedouble);
}

void test_parse_previous_year_dual_temperature(void)
{
    testMeter->simulateData("9.4*01(050.2&037.0*C)");

    cJSON *json = testMeter->getJsonData();
    TEST_ASSERT_NOT_NULL(json);

    cJSON *flowPrev = cJSON_GetObjectItem(json, "9.4*01.0");
    TEST_ASSERT_NOT_NULL(flowPrev);
    TEST_ASSERT_TRUE(cJSON_IsNumber(flowPrev));
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 50.2, flowPrev->valuedouble);

    cJSON *returnPrev = cJSON_GetObjectItem(json, "9.4*01.1");
    TEST_ASSERT_NOT_NULL(returnPrev);
    TEST_ASSERT_TRUE(cJSON_IsNumber(returnPrev));
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 37.0, returnPrev->valuedouble);
}

void test_parse_previous_year_single_energy(void)
{
    testMeter->simulateData("6.8*01(0000.987*MWh)");

    cJSON *json = testMeter->getJsonData();
    TEST_ASSERT_NOT_NULL(json);

    cJSON *item = cJSON_GetObjectItem(json, "6.8*01");
    TEST_ASSERT_NOT_NULL(item);
    TEST_ASSERT_TRUE(cJSON_IsNumber(item));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.987, item->valuedouble);
}

void test_parse_error_code_and_operating_hours(void)
{
    testMeter->simulateData("F(0)");
    testMeter->simulateData("6.31(001234*h)");
    testMeter->simulateData("6.32(000002*h)");

    cJSON *json = testMeter->getJsonData();
    TEST_ASSERT_NOT_NULL(json);

    cJSON *errorCode = cJSON_GetObjectItem(json, "F");
    TEST_ASSERT_NOT_NULL(errorCode);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 0.0, errorCode->valuedouble);

    cJSON *opHours = cJSON_GetObjectItem(json, "6.31");
    TEST_ASSERT_NOT_NULL(opHours);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 1234.0, opHours->valuedouble);

    cJSON *errHours = cJSON_GetObjectItem(json, "6.32");
    TEST_ASSERT_NOT_NULL(errHours);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 2.0, errHours->valuedouble);
}

void test_parse_multiple_obis_in_single_line(void)
{
    testMeter->simulateData("6.8(0012.340*MWh)6.26(00078.90*m3)");

    cJSON *json = testMeter->getJsonData();
    TEST_ASSERT_NOT_NULL(json);

    cJSON *energy = cJSON_GetObjectItem(json, "6.8");
    TEST_ASSERT_NOT_NULL(energy);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 12.34, energy->valuedouble);

    cJSON *volume = cJSON_GetObjectItem(json, "6.26");
    TEST_ASSERT_NOT_NULL(volume);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 78.90, volume->valuedouble);
}

void test_parse_whitespace_and_newlines(void)
{
    testMeter->simulateData("   6.8(0005.123*MWh) \r");

    cJSON *json = testMeter->getJsonData();
    TEST_ASSERT_NOT_NULL(json);

    cJSON *energy = cJSON_GetObjectItem(json, "6.8");
    TEST_ASSERT_NOT_NULL(energy);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 5.123, energy->valuedouble);
}

void test_parse_malformed_lines_graceful(void)
{
    testMeter->simulateData("");
    testMeter->simulateData("   ");
    testMeter->simulateData("NO_PARENS_HERE");
    testMeter->simulateData("()");
    testMeter->simulateData("6.8()");
    testMeter->simulateData("(0012.34*MWh)");

    cJSON *json = testMeter->getJsonData();
    TEST_ASSERT_NOT_NULL(json);
    // Should have 0 items since all lines were invalid
    TEST_ASSERT_EQUAL_INT(0, cJSON_GetArraySize(json));
}

void test_clear_json_data(void)
{
    testMeter->simulateData("6.8(0001.234*MWh)");
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetArraySize(testMeter->getJsonData()));

    testMeter->clearJsonData();
    TEST_ASSERT_EQUAL_INT(0, cJSON_GetArraySize(testMeter->getJsonData()));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_parse_single_energy_obis);
    RUN_TEST(test_parse_volume_obis);
    RUN_TEST(test_parse_dual_temperature_obis);
    RUN_TEST(test_parse_previous_year_dual_temperature);
    RUN_TEST(test_parse_previous_year_single_energy);
    RUN_TEST(test_parse_error_code_and_operating_hours);
    RUN_TEST(test_parse_multiple_obis_in_single_line);
    RUN_TEST(test_parse_whitespace_and_newlines);
    RUN_TEST(test_parse_malformed_lines_graceful);
    RUN_TEST(test_clear_json_data);
    return UNITY_END();
}