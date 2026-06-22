#ifndef __APP_DISPLAY_H_
#define __APP_DISPLAY_H_

#include "Int_OLED.h"
#include "FreeRTOS.h"
#include "task.h"

/**
 * @brief 初始化
 */
void App_Display_Init(void);

/**
 * @brief 显示
 */
void App_Display_Show(void);

#endif // __APP_DISPLAY_H_
