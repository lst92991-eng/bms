#ifndef APP_DEBUG_CLI_H
#define APP_DEBUG_CLI_H

#include <stdbool.h>
#include <stdint.h>

void App_DebugCli_Init(void);
void App_DebugCli_Task(uint16_t interval_ms);

/* true 表示 CLI 正在连续占用串口，生产模块应暂停周期诊断打印。 */
bool App_DebugCli_IsStreaming(void);

#endif /* APP_DEBUG_CLI_H */
