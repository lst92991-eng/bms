#ifndef APP_OLED_H
#define APP_OLED_H

#include <stdbool.h>
#include <stdint.h>

void App_OLED_Init(void);
void App_OLED_ShowIicStatus(bool ok);
void App_OLED_ShowBqIicPowerConfig(bool ok, uint16_t power_config);

#endif /* APP_OLED_H */
