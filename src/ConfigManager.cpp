#include "ConfigManager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "ConfigManager";

static void readNvsString(nvs_handle_t handle, const char *key, std::string &target)
{
    size_t requiredSize = 0;
    if (nvs_get_str(handle, key, nullptr, &requiredSize) == ESP_OK && requiredSize > 0)
    {
        std::string buffer(requiredSize, '\0');
        if (nvs_get_str(handle, key, &buffer[0], &requiredSize) == ESP_OK)
        {
            buffer.pop_back();
            target = buffer;
        }
    }
}

void ConfigManager::setup()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    loadConfig();
}

void ConfigManager::loadConfig()
{
    nvs_handle_t nvsHandle;
    esp_err_t err = nvs_open("config", NVS_READONLY, &nvsHandle);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "No existing configuration found in NVS, using default values");
        return;
    }

    readNvsString(nvsHandle, "wifiSsid", m_wifiSsid);
    readNvsString(nvsHandle, "wifiPass", m_wifiPassword);
    readNvsString(nvsHandle, "mqttSrv", m_mqttServer);
    nvs_get_u16(nvsHandle, "mqttPort", &m_mqttPort);
    readNvsString(nvsHandle, "mqttUsr", m_mqttUser);
    readNvsString(nvsHandle, "mqttPass", m_mqttPassword);
    readNvsString(nvsHandle, "mqttTopic", m_mqttTopic);
    nvs_get_u32(nvsHandle, "interval", &m_readIntervalSeconds);

    uint8_t dummyVal = 0;
    if (nvs_get_u8(nvsHandle, "dummyMode", &dummyVal) == ESP_OK)
    {
        m_dummyMode = (dummyVal != 0);
    }

    nvs_close(nvsHandle);
    ESP_LOGI(TAG, "Configuration loaded successfully");
}

void ConfigManager::saveConfig()
{
    nvs_handle_t nvsHandle;
    esp_err_t err = nvs_open("config", NVS_READWRITE, &nvsHandle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS handle for writing: %s", esp_err_to_name(err));
        return;
    }

    nvs_set_str(nvsHandle, "wifiSsid", m_wifiSsid.c_str());
    nvs_set_str(nvsHandle, "wifiPass", m_wifiPassword.c_str());
    nvs_set_str(nvsHandle, "mqttSrv", m_mqttServer.c_str());
    nvs_set_u16(nvsHandle, "mqttPort", m_mqttPort);
    nvs_set_str(nvsHandle, "mqttUsr", m_mqttUser.c_str());
    nvs_set_str(nvsHandle, "mqttPass", m_mqttPassword.c_str());
    nvs_set_str(nvsHandle, "mqttTopic", m_mqttTopic.c_str());
    nvs_set_u32(nvsHandle, "interval", m_readIntervalSeconds);
    nvs_set_u8(nvsHandle, "dummyMode", m_dummyMode ? 1 : 0);

    err = nvs_commit(nvsHandle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit NVS changes: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "Configuration saved successfully");
    }

    nvs_close(nvsHandle);
}