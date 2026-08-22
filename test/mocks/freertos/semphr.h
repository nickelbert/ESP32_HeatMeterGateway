#pragma once

#include "FreeRTOS.h"

typedef void *SemaphoreHandle_t;

inline SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    return (SemaphoreHandle_t)1;
}

inline void vSemaphoreDelete(SemaphoreHandle_t mutex)
{
    (void)mutex;
}

inline BaseType_t xSemaphoreTake(SemaphoreHandle_t mutex, TickType_t blockTime)
{
    (void)mutex;
    (void)blockTime;
    return pdTRUE;
}

inline BaseType_t xSemaphoreGive(SemaphoreHandle_t mutex)
{
    (void)mutex;
    return pdTRUE;
}