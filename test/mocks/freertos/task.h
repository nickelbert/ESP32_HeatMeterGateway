#pragma once

#include "FreeRTOS.h"

inline void vTaskDelay(TickType_t ticks)
{
    (void)ticks;
}

inline BaseType_t xTaskCreate(void (*taskFunction)(void *), const char *name, uint32_t stackDepth, void *params, int priority, TaskHandle_t *handle)
{
    (void)taskFunction;
    (void)name;
    (void)stackDepth;
    (void)params;
    (void)priority;
    if (handle != nullptr)
    {
        *handle = (TaskHandle_t)1;
    }
    return pdTRUE;
}

inline void vTaskDelete(TaskHandle_t handle)
{
    (void)handle;
}