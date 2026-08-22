#pragma once

#include <cstdint>
#include <cstddef>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

typedef int uart_port_t;
#define UART_NUM_0 0
#define UART_NUM_1 1
#define UART_NUM_2 2
#define UART_PIN_NO_CHANGE -1

typedef enum
{
    UART_DATA_7_BITS = 0,
    UART_DATA_8_BITS = 1
} uart_word_length_t;

typedef enum
{
    UART_PARITY_DISABLE = 0,
    UART_PARITY_EVEN = 2,
    UART_PARITY_ODD = 3
} uart_parity_t;

typedef enum
{
    UART_STOP_BITS_1 = 1,
    UART_STOP_BITS_2 = 2
} uart_stop_bits_t;

typedef enum
{
    UART_HW_FLOWCTRL_DISABLE = 0
} uart_hw_flowcontrol_t;

typedef enum
{
    UART_SCLK_DEFAULT = 0
} uart_sclk_t;

typedef struct
{
    int baud_rate;
    uart_word_length_t data_bits;
    uart_parity_t parity;
    uart_stop_bits_t stop_bits;
    uart_hw_flowcontrol_t flow_ctrl;
    uint8_t rx_flow_ctrl_thresh;
    uart_sclk_t source_clk;
} uart_config_t;

inline esp_err_t uart_param_config(uart_port_t uart_num, const uart_config_t *uart_config)
{
    (void)uart_num;
    (void)uart_config;
    return ESP_OK;
}

inline esp_err_t uart_driver_install(uart_port_t uart_num, int rx_buffer_size, int tx_buffer_size, int queue_size, void *uart_queue, int intr_alloc_flags)
{
    (void)uart_num;
    (void)rx_buffer_size;
    (void)tx_buffer_size;
    (void)queue_size;
    (void)uart_queue;
    (void)intr_alloc_flags;
    return ESP_OK;
}

inline esp_err_t uart_set_pin(uart_port_t uart_num, int tx_io_num, int rx_io_num, int rts_io_num, int cts_io_num)
{
    (void)uart_num;
    (void)tx_io_num;
    (void)rx_io_num;
    (void)rts_io_num;
    (void)cts_io_num;
    return ESP_OK;
}

inline int uart_write_bytes(uart_port_t uart_num, const void *src, size_t size)
{
    (void)uart_num;
    (void)src;
    return (int)size;
}

inline int uart_read_bytes(uart_port_t uart_num, void *buf, uint32_t length, TickType_t ticks_to_wait)
{
    (void)uart_num;
    (void)buf;
    (void)length;
    (void)ticks_to_wait;
    return 0;
}

inline esp_err_t uart_wait_tx_done(uart_port_t uart_num, TickType_t ticks_to_wait)
{
    (void)uart_num;
    (void)ticks_to_wait;
    return ESP_OK;
}

inline esp_err_t uart_flush_input(uart_port_t uart_num)
{
    (void)uart_num;
    return ESP_OK;
}