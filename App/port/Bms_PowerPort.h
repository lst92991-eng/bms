#ifndef BMS_POWER_PORT_H
#define BMS_POWER_PORT_H

#include <stdbool.h>
#include <stdint.h>

bool Bms_PowerPort_ApplyMainOutput(bool charge_enable, bool discharge_enable);
bool Bms_PowerPort_ApplyPreDischarge(bool charge_enable);
bool Bms_PowerPort_AllMainFetsOff(void);
void Bms_PowerPort_SetChargeRequest(bool enable);
bool Bms_PowerPort_IsInputPresent(uint32_t valid_vbus_mv);
bool Bms_PowerPort_HasScFault(void);
uint32_t Bms_PowerPort_GetVbusMv(void);

#endif /* BMS_POWER_PORT_H */
