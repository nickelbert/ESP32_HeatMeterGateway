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

    readNvsString(nvsHandle, "wifiSsid", wifiSsid);
    readNvsString(nvsHandle, "wifiPass", wifiPassword);
    readNvsString(nvsHandle, "mqttSrv", mqttServer);
    nvs_get_u16(nvsHandle, "mqttPort", &mqttPort);
    readNvsString(nvsHandle, "mqttUsr", mqttUser);
    readNvsString(nvsHandle, "mqttPass", mqttPassword);
    readNvsString(nvsHandle, "mqttTopic", mqttTopic);
    nvs_get_u32(nvsHandle, "interval", &readIntervalSeconds);

    uint8_t dummyVal = 0;
    if (nvs_get_u8(nvsHandle, "dummyMode", &dummyVal) == ESP_OK)
    {
        dummyMode = (dummyVal != 0);
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

    nvs_set_str(nvsHandle, "wifiSsid", wifiSsid.c_str());
    nvs_set_str(nvsHandle, "wifiPass", wifiPassword.c_str());
    nvs_set_str(nvsHandle, "mqttSrv", mqttServer.c_str());
    nvs_set_u16(nvsHandle, "mqttPort", mqttPort);
    nvs_set_str(nvsHandle, "mqttUsr", mqttUser.c_str());
    nvs_set_str(nvsHandle, "mqttPass", mqttPassword.c_str());
    nvs_set_str(nvsHandle, "mqttTopic", mqttTopic.c_str());
    nvs_set_u32(nvsHandle, "interval", readIntervalSeconds);
    nvs_set_u8(nvsHandle, "dummyMode", dummyMode ? 1 : 0);

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