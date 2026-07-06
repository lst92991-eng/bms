#ifndef BMS_OLED_DEBUG_VIEW_H
#define BMS_OLED_DEBUG_VIEW_H

#include <stdbool.h>
#include <stdint.h>

void Bms_OledDebugView_Init(void);
void Bms_OledDebugView_ShowIicStatus(bool ok);
void Bms_OledDebugView_ShowBqIicPowerConfig(bool ok, uint16_t power_config);

#endif /* BMS_OLED_DEBUG_VIEW_H */
