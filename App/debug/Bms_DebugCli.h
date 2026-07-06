#ifndef BMS_DEBUG_CLI_H
#define BMS_DEBUG_CLI_H

#include <stdbool.h>
#include <stdint.h>

void Bms_DebugCli_Init(void);
void Bms_DebugCli_Task(uint16_t interval_ms);
bool Bms_DebugCli_IsVofaStreaming(void);
bool Bms_DebugCli_IsBqMonitoring(void);

#endif /* BMS_DEBUG_CLI_H */
