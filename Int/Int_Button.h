#ifndef INT_BUTTON_H
#define INT_BUTTON_H

#include <stdbool.h>
#include <stdint.h>

#include "main.h"

/* PD3 外部已经有 5.1k 下拉和 100nF 到地，MCU 内部保持浮空输入。 */
#define INT_BUTTON_ACTIVE_LEVEL  GPIO_PIN_SET
#define INT_BUTTON_DEBOUNCE_MS   20u

typedef enum
{
    INT_BUTTON_RELEASED = 0,
    INT_BUTTON_PRESSED
} Int_Button_StateTypeDef;

void Int_Button_Init(void);
void Int_Button_Task(uint32_t now_ms);
Int_Button_StateTypeDef Int_Button_GetState(void);
bool Int_Button_IsPressed(void);
bool Int_Button_IsPressedRaw(void);
bool Int_Button_WasPressed(void);
bool Int_Button_WasReleased(void);
uint32_t Int_Button_GetHoldMs(uint32_t now_ms);

#endif
