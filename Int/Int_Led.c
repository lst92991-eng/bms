#include "Int_Led.h"

static void Int_Led_Write(Int_Led_IdTypeDef led, bool on)
{
    GPIO_PinState level = on ? INT_LED_ON_LEVEL : INT_LED_OFF_LEVEL;

    if (led == INT_LED_RED)
    {
        HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, level);
    }
    else if (led == INT_LED_GREEN)
    {
        HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, level);
    }
}

void Int_Led_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* 先写关闭电平，再切输出模式，避免上电初始化时 LED 闪一下。 */
    HAL_GPIO_WritePin(GPIOB, LED_RED_Pin | LED_GREEN_Pin, INT_LED_OFF_LEVEL);

    gpio.Pin = LED_RED_Pin | LED_GREEN_Pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);
}

void Int_Led_Set(Int_Led_IdTypeDef led, bool on)
{
    Int_Led_Write(led, on);
}

void Int_Led_Toggle(Int_Led_IdTypeDef led)
{
    Int_Led_Set(led, !Int_Led_IsOn(led));
}

void Int_Led_AllOff(void)
{
    Int_Led_Set(INT_LED_RED, false);
    Int_Led_Set(INT_LED_GREEN, false);
}

void Int_Led_AllOn(void)
{
    Int_Led_Set(INT_LED_RED, true);
    Int_Led_Set(INT_LED_GREEN, true);
}

bool Int_Led_IsOn(Int_Led_IdTypeDef led)
{
    if (led == INT_LED_RED)
    {
        return HAL_GPIO_ReadPin(LED_RED_GPIO_Port, LED_RED_Pin) == INT_LED_ON_LEVEL;
    }

    if (led == INT_LED_GREEN)
    {
        return HAL_GPIO_ReadPin(LED_GREEN_GPIO_Port, LED_GREEN_Pin) == INT_LED_ON_LEVEL;
    }

    return false;
}
