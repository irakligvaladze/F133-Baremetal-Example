#pragma once
#include <stdint.h>
#include "F133_registers.h"


typedef enum
{
    GPIO_MODE_INPUT  = 0x0,
    GPIO_MODE_OUTPUT = 0x1,
} gpio_mode_t;

typedef enum
{
    GPIO_RESET  = 0x0,
    GPIO_SET = 0x1,
} gpio_value_t;


static inline void HAL_GPIO_Init(GPIO_TypeDef *port, uint32_t pin, gpio_mode_t mode)
{
    uint32_t cfg_index = pin / 8;
    uint32_t shift     = (pin % 8) * 4;

    port->CFG[cfg_index] &= ~(0xFU << shift);
    port->CFG[cfg_index] |=  ((uint32_t)mode << shift);
}

static inline void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint32_t pin, gpio_value_t value)
{
    if (value)
        port->DATA |=  (1U << pin);
    else
        port->DATA &= ~(1U << pin);
}

static inline void HAL_GPIO_SetPin(GPIO_TypeDef *port, uint32_t pin)
{
    port->DATA |= (1U << pin);
}

static inline void HAL_GPIO_ResetPin(GPIO_TypeDef *port, uint32_t pin)
{
    port->DATA &= ~(1U << pin);
}

static inline void HAL_GPIO_TogglePin(GPIO_TypeDef *port, uint32_t pin)
{
    port->DATA ^= (1U << pin);
}

static inline uint32_t HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint32_t pin)
{
    return (port->DATA >> pin) & 1U;
}
