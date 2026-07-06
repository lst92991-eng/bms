#ifndef BMS_POWER_SERVICE_H
#define BMS_POWER_SERVICE_H

#include "App_Power.h"

void Bms_PowerService_Init(void);
void Bms_PowerService_Task(uint32_t interval_ms);
App_Power_StateTypeDef Bms_PowerService_GetState(void);
bool Bms_PowerService_IsChargeAllowed(void);
bool Bms_PowerService_IsDischargeAllowed(void);
void Bms_PowerService_PrintSnapshot(void);
void Bms_PowerService_PrintStopReason(void);
bool Bms_PowerService_ClearDischargeFault(void);

#endif /* BMS_POWER_SERVICE_H */
