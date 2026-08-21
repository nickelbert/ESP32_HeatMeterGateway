#pragma once

#include "esp_err.h"
#include <cstdint>
#include <cstddef>

typedef uint32_t nvs_handle_t;

typedef enum
{
    NVS_READONLY,
    NVS_READWRITE
} nvs_open_mode_t;

inline esp_err_t nvs_open(const char *name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle)
{
    (void)name;
    (void)open_mode;
    if (out_handle != nullptr)
    {
        *out_handle = 1;
    }
    return ESP_OK;
}

inline void nvs_close(nvs_handle_t handle)
{
    (void)handle;
}

inline esp_err_t nvs_get_str(nvs_handle_t handle, const char *key, char *out_value, size_t *length)
{
    (void)handle;
    (void)key;
    (void)out_value;
    (void)length;
    return ESP_FAIL;
}

inline esp_err_t nvs_get_u16(nvs_handle_t handle, const char *key, uint16_t *out_value)
{
    (void)handle;
    (void)key;
    (void)out_value;
    return ESP_FAIL;
}

inline esp_err_t nvs_get_u32(nvs_handle_t handle, const char *key, uint32_t *out_value)
{
    (void)handle;
    (void)key;
    (void)out_value;
    return ESP_FAIL;
}

inline esp_err_t nvs_get_u8(nvs_handle_t handle, const char *key, uint8_t *out_value)
{
    (void)handle;
    (void)key;
    (void)out_value;
    return ESP_FAIL;
}

inline esp_err_t nvs_set_str(nvs_handle_t handle, const char *key, const char *value)
{
    (void)handle;
    (void)key;
    (void)value;
    return ESP_OK;
}

inline esp_err_t nvs_set_u16(nvs_handle_t handle, const char *key, uint16_t value)
{
    (void)handle;
    (void)key;
    (void)value;
    return ESP_OK;
}

inline esp_err_t nvs_set_u32(nvs_handle_t handle, const char *key, uint32_t value)
{
    (void)handle;
    (void)key;
    (void)value;
    return ESP_OK;
}

inline esp_err_t nvs_set_u8(nvs_handle_t handle, const char *key, uint8_t value)
{
    (void)handle;
    (void)key;
    (void)value;
    return ESP_OK;
}

inline esp_err_t nvs_commit(nvs_handle_t handle)
{
    (void)handle;
    return ESP_OK;
}