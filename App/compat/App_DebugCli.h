#ifndef APP_DEBUG_CLI_H
#define APP_DEBUG_CLI_H

#include <stdbool.h>
#include <stdint.h>

void App_DebugCli_Init(void);
void App_DebugCli_Task(uint16_t interval_ms);
bool App_DebugCli_IsVofaStreaming(void);
bool App_DebugCli_IsBqMonitoring(void);

#endif /* APP_DEBUG_CLI_H */
