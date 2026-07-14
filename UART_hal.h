// uart_hal.h
#pragma once
#include <stdint.h>
#include "F133_registers.h"

#define UART_LSR_DR    (1U << 0)
#define UART_LSR_THRE  (1U << 5)

typedef enum
{
    UART_WORDLENGTH_8B = 8,
} UART_WordLengthTypeDef;

typedef enum
{
    UART_STOPBITS_1 = 1,
} UART_StopBitsTypeDef;

typedef enum
{
    UART_PARITY_NONE = 0,
} UART_ParityTypeDef;

typedef struct
{
    uint32_t BaudRate;
    UART_WordLengthTypeDef WordLength;
    UART_StopBitsTypeDef StopBits;
    UART_ParityTypeDef Parity;
} UART_InitTypeDef;

typedef struct
{
    UART_TypeDef *Instance;
    UART_InitTypeDef Init;
} UART_HandleTypeDef;

static inline void HAL_UART_Init(UART_HandleTypeDef *huart)
{
    UART_TypeDef *uart = huart->Instance;

    // Assumes UART input clock = 24 MHz
    uint32_t divisor = 24000000UL / (16UL * huart->Init.BaudRate);

    uart->UART_LCR = 0x80;                 // DLAB = 1
    uart->DATA = divisor & 0xFF;           // DLL
    uart->DLH_IER = (divisor >> 8) & 0xFF; // DLH

    uart->UART_LCR = 0x03;                 // 8N1, DLAB = 0
    uart->IIR_FCR = 0x07;                  // FIFO enable + clear RX/TX
}

static inline void HAL_UART_TransmitChar(UART_HandleTypeDef *huart, char c)
{
    UART_TypeDef *uart = huart->Instance;

    while (!(uart->UART_LSR & UART_LSR_THRE))
        ;

    uart->DATA = (uint32_t)c;
}

static inline char HAL_UART_ReceiveChar(UART_HandleTypeDef *huart)
{
    UART_TypeDef *uart = huart->Instance;

    while (!(uart->UART_LSR & UART_LSR_DR))
        ;

    return (char)(uart->DATA & 0xFF);
}

static inline void HAL_UART_TransmitString(UART_HandleTypeDef *huart, const char *s)
{
    while (*s)
    {
        if (*s == '\n')
            HAL_UART_TransmitChar(huart, '\r');

        HAL_UART_TransmitChar(huart, *s++);
    }
}