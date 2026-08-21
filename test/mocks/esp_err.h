#pragma once

#include <cstdint>

typedef int32_t esp_err_t;

#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NVS_NO_FREE_PAGES 0x1100
#define ESP_ERR_NVS_NEW_VERSION_FOUND 0x1101

inline const char *esp_err_to_name(esp_err_t err)
{
    return (err == ESP_OK) ? "ESP_OK" : "ESP_FAIL";
}