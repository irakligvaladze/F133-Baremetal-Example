#include "UART_hal.h"
#include "GPIO_hal.h"
#include "TIME_hal.h"
#include "mini_stdio.h"

UART_HandleTypeDef huart0;

static void GPIO_AllInputs(void)
{
    for (int pin = 0; pin < 32; pin++)
    {
        HAL_GPIO_Init(GPIOB, pin, GPIO_MODE_INPUT);
        HAL_GPIO_Init(GPIOC, pin, GPIO_MODE_INPUT);
        HAL_GPIO_Init(GPIOD, pin, GPIO_MODE_INPUT);
        HAL_GPIO_Init(GPIOF, pin, GPIO_MODE_INPUT);
        HAL_GPIO_Init(GPIOG, pin, GPIO_MODE_INPUT);

        if (pin != 2 && pin != 3)
            HAL_GPIO_Init(GPIOE, pin, GPIO_MODE_INPUT);
    }
}

void HAL_GPIO_PrintPort(GPIO_TypeDef *port, const char *name)
{
    uart_printf("%s : ", name);

    for (int pin = 31; pin >= 0; pin--)
    {
        uart_printf("%d", HAL_GPIO_ReadPin(port, pin));

        if ((pin % 8) == 0)
            uart_printf(" ");
    }

    uart_printf("\n");
}

void HAL_GPIO_PrintAllPorts(void)
{
    HAL_GPIO_PrintPort(GPIOB, "GPIOB");
    HAL_GPIO_PrintPort(GPIOC, "GPIOC");
    HAL_GPIO_PrintPort(GPIOD, "GPIOD");
    HAL_GPIO_PrintPort(GPIOE, "GPIOE");
    HAL_GPIO_PrintPort(GPIOF, "GPIOF");
    HAL_GPIO_PrintPort(GPIOG, "GPIOG");
}

int main(void)
{
    char line[64];

    huart0.Instance = UART0;
    huart0.Init.BaudRate = 115200;
    huart0.Init.WordLength = UART_WORDLENGTH_8B;
    huart0.Init.StopBits = UART_STOPBITS_1;
    huart0.Init.Parity = UART_PARITY_NONE;

    HAL_UART_Init(&huart0);

    uart_printf("\nF133 configuring port\n");

    GPIO_AllInputs();

    uart_printf("\nF133 mini stdio ready\n");

    while (1)
    {
        uart_printf("\n----------------------\n");
        HAL_delayMs(200);
        HAL_GPIO_PrintAllPorts();
    }
}
