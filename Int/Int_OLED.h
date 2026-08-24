#ifndef INT_OLED_H
#define INT_OLED_H

#include <stdbool.h>
#include <stdint.h>

/* 当前产品只使用 8x16 ASCII 文本，不再暴露未验证的图形/中文/滚动接口。 */
bool Inf_OLED_Init(void);
void Inf_OLED_Clear(void);
bool Inf_OLED_Refresh(void);
void Inf_OLED_ShowText16(uint8_t x, uint8_t y, const char *text);

#endif /* INT_OLED_H */
