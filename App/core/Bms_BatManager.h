#ifndef BMS_BAT_MANAGER_H
#define BMS_BAT_MANAGER_H

#include <stdint.h>

void Bms_BatManager_Init(void);
void Bms_BatManager_Task(uint32_t interval_ms);
void Bms_BatManager_SyncModelSnapshot(void);

#endif /* BMS_BAT_MANAGER_H */
