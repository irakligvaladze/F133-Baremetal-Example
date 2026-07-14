#pragma once
#include <stdint.h>
#include <stddef.h>
#include "UART_hal.h"

extern UART_HandleTypeDef huart0;

static inline void uart_putchar(char c)
{
    if (c == '\n')
        HAL_UART_TransmitChar(&huart0, '\r');

    HAL_UART_TransmitChar(&huart0, c);
}

static inline char uart_getchar(void)
{
    return HAL_UART_ReceiveChar(&huart0);
}

static void uart_puts(const char *s)
{
    while (*s)
        uart_putchar(*s++);
}

static void uart_print_uint(uint64_t value, uint32_t base)
{
    char buf[32];
    uint32_t i = 0;

    if (value == 0)
    {
        uart_putchar('0');
        return;
    }

    while (value)
    {
        uint32_t digit = value % base;
        buf[i++] = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
        value /= base;
    }

    while (i)
        uart_putchar(buf[--i]);
}

static void uart_print_int(int64_t value)
{
    if (value < 0)
    {
        uart_putchar('-');
        value = -value;
    }

    uart_print_uint((uint64_t)value, 10);
}

static void uart_printf(const char *fmt, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, fmt);

    while (*fmt)
    {
        if (*fmt != '%')
        {
            uart_putchar(*fmt++);
            continue;
        }

        fmt++;

        switch (*fmt)
        {
            case 'c':
                uart_putchar((char)__builtin_va_arg(args, int));
                break;

            case 's':
            {
                const char *s = __builtin_va_arg(args, const char *);
                uart_puts(s ? s : "(null)");
                break;
            }

            case 'd':
                uart_print_int(__builtin_va_arg(args, int));
                break;

            case 'u':
                uart_print_uint(__builtin_va_arg(args, unsigned int), 10);
                break;

            case 'x':
                uart_print_uint(__builtin_va_arg(args, unsigned int), 16);
                break;

            case 'p':
                uart_puts("0x");
                uart_print_uint((uintptr_t)__builtin_va_arg(args, void *), 16);
                break;

            case '%':
                uart_putchar('%');
                break;

            default:
                uart_putchar('%');
                uart_putchar(*fmt);
                break;
        }

        fmt++;
    }

    __builtin_va_end(args);
}

static int uart_readline(char *buf, uint32_t max_len)
{
    uint32_t i = 0;

    if (max_len == 0)
        return 0;

    while (i < max_len - 1)
    {
        char c = uart_getchar();

        if (c == '\r' || c == '\n')
        {
            uart_putchar('\n');
            break;
        }

        if (c == 8 || c == 127) // backspace/delete
        {
            if (i > 0)
            {
                i--;
                uart_puts("\b \b");
            }
            continue;
        }

        buf[i++] = c;
        uart_putchar(c); // echo
    }

    buf[i] = '\0';
    return i;
}