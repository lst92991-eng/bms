#ifndef BMS_BQ_CONFIG_PORT_H
#define BMS_BQ_CONFIG_PORT_H

#include <stdbool.h>

#include "Int_BQ76952.h"

bool Bms_BqConfigPort_ApplyConfig(void);
Int_BQ76952_StatusTypeDef Bms_BqConfigPort_KeepMainFetsOff(void);
void Bms_BqConfigPort_ClearStartupAlarms(void);
bool Bms_BqConfigPort_SetMainFets(bool charge_enable, bool discharge_enable);
bool Bms_BqConfigPort_SetPreDischargeFet(bool charge_enable);
bool Bms_BqConfigPort_TestPreDischargeOnly(void);
bool Bms_BqConfigPort_AllMainFetsOff(void);

#endif /* BMS_BQ_CONFIG_PORT_H */
