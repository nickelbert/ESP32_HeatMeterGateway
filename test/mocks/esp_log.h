#pragma once

#include <cstdio>
#include "esp_err.h"

#define ESP_LOGI(tag, format, ...) printf("[INFO] [%s] " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) printf("[WARN] [%s] " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGE(tag, format, ...) printf("[ERROR] [%s] " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, format, ...) printf("[DEBUG] [%s] " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGV(tag, format, ...) printf("[VERBOSE] [%s] " format "\n", tag, ##__VA_ARGS__)

#define ESP_ERROR_CHECK(x) (void)(x)