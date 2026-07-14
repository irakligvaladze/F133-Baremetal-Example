// TIME_hal.h
#pragma once
#include <stdint.h>
#include "F133_registers.h"


static inline void HAL_delayMs(uint32_t ms)
{   

    for(volatile uint32_t j = 0; j < ms; j++){
        // 1ms delay
        for (volatile uint32_t i = 0; i < 3200; i++) {
            __asm__ volatile ("nop");
        }
    } 
}

// it should be 3.2 but whatever.. not exactly 1us
static inline void HAL_delayUs(uint32_t ms)
{   

    for(volatile uint32_t j = 0; j < ms; j++){
        // 1ms delay
        for (volatile uint32_t i = 0; i < 3; i++) {
            __asm__ volatile ("nop");
        }
    } 
}