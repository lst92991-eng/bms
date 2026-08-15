#ifndef APP_OLED_H
#define APP_OLED_H

#include <stdbool.h>
#include <stdint.h>

void App_OLED_Init(void);
void App_OLED_ShowIicStatus(bool ok);
void App_OLED_ShowBqIicPowerConfig(bool ok, uint16_t power_config);
void App_OLED_ShowBatteryStatus(bool ok,
                                float soc_percent,
                                bool soh_valid,
                                uint8_t soh_percent);

#endif /* APP_OLED_H */
