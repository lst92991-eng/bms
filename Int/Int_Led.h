#ifndef INT_LED_H
#define INT_LED_H

#include <stdbool.h>

#include "main.h"

/* 板载 LED 为低电平点亮：SYS_3V3 -> 电阻 -> LED -> MCU 引脚。 */
#define INT_LED_ON_LEVEL   GPIO_PIN_RESET
#define INT_LED_OFF_LEVEL  GPIO_PIN_SET

typedef enum
{
    INT_LED_RED = 0,
    INT_LED_GREEN
} Int_Led_IdTypeDef;

void Int_Led_Init(void);
void Int_Led_Set(Int_Led_IdTypeDef led, bool on);
void Int_Led_Toggle(Int_Led_IdTypeDef led);
void Int_Led_AllOff(void);
void Int_Led_AllOn(void);
bool Int_Led_IsOn(Int_Led_IdTypeDef led);

#endif
