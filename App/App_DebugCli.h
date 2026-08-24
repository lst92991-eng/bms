#ifndef APP_DEBUG_CLI_H
#define APP_DEBUG_CLI_H

#include <stdbool.h>
#include <stdint.h>

/* Release 默认不编译工程 CLI；工程构建还必须显式提供物理使能。 */
#ifndef APP_DEBUG_CLI_ENGINEERING_BUILD
#ifdef BMS_ENGINEERING_BUILD
#define APP_DEBUG_CLI_ENGINEERING_BUILD BMS_ENGINEERING_BUILD
#else
#define APP_DEBUG_CLI_ENGINEERING_BUILD 0
#endif
#endif

#ifndef APP_DEBUG_CLI_PHYSICAL_ENABLE_ACTIVE
#define APP_DEBUG_CLI_PHYSICAL_ENABLE_ACTIVE() (false)
#endif

#define APP_DEBUG_CLI_ENGINEERING_ENABLED (APP_DEBUG_CLI_ENGINEERING_BUILD != 0)

void App_DebugCli_Init(void);
void App_DebugCli_Task(uint16_t interval_ms);
void App_DebugCli_OnUartError(void);

/* true 表示 CLI 正在连续占用串口，生产模块应暂停周期诊断打印。 */
bool App_DebugCli_IsStreaming(void);

#endif /* APP_DEBUG_CLI_H */
