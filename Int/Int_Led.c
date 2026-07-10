#include "Int_Led.h"

#include "main.h"

void Int_Led_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* 板载 LED 低电平点亮；先写关闭电平，避免切换输出模式时闪烁。 */
    HAL_GPIO_WritePin(GPIOB, LED_RED_Pin | LED_GREEN_Pin, GPIO_PIN_SET);

    gpio.Pin = LED_RED_Pin | LED_GREEN_Pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);
}
