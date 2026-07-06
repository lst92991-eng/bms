#ifndef BMS_ESTIMATE_H
#define BMS_ESTIMATE_H

#include <stdint.h>

void Bms_Estimate_Reset(void);
void Bms_Estimate_Init(void);
void Bms_Estimate_UpdateFirstFrame(void);
void Bms_Estimate_Task(uint32_t interval_ms);

#endif /* BMS_ESTIMATE_H */
