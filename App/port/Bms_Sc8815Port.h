#ifndef BMS_SC8815_PORT_H
#define BMS_SC8815_PORT_H

#include <stdbool.h>
#include <stdint.h>

void Bms_Sc8815Port_Init(void);
void Bms_Sc8815Port_Task(uint16_t interval_ms);
void Bms_Sc8815Port_RequestCharge(bool enable);
bool Bms_Sc8815Port_IsCommOk(void);
bool Bms_Sc8815Port_IsAcOk(void);
bool Bms_Sc8815Port_HasFault(void);
bool Bms_Sc8815Port_IsCharging(void);
uint32_t Bms_Sc8815Port_GetVbusMv(void);
uint32_t Bms_Sc8815Port_GetVbatMv(void);
uint32_t Bms_Sc8815Port_GetInputLimitMa(void);
void Bms_Sc8815Port_PrintCliSnapshot(void);

#endif /* BMS_SC8815_PORT_H */
