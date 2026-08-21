#pragma once

#include <cstdint>

typedef void *TaskHandle_t;
typedef uint32_t TickType_t;
typedef int BaseType_t;

#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define pdTRUE 1
#define pdFALSE 0
#define portMAX_DELAY 0xFFFFFFFFUL