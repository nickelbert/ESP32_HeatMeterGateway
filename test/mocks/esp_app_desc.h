#pragma once

typedef struct
{
    char version[32];
    char project_name[32];
    char time[16];
    char date[16];
    char idf_ver[32];
} esp_app_desc_t;

inline const esp_app_desc_t *esp_app_get_description(void)
{
    static const esp_app_desc_t desc = {
        "1.0.0-test",
        "HeatMeterGateway",
        "12:00:00",
        "Jan 01 2026",
        "v5.3.1"
    };
    return &desc;
}