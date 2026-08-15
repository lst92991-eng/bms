#ifndef APP_POWER_H
#define APP_POWER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    APP_POWER_STATE_OFF = 0,
    APP_POWER_STATE_MONITOR,
    APP_POWER_STATE_RUN,
    APP_POWER_STATE_LOW,
    APP_POWER_STATE_BQ_WAKE,
    APP_POWER_STATE_BQ_SHUTDOWN,
    APP_POWER_STATE_FAULT
} App_Power_StateTypeDef;

void App_Power_Init(void);
void App_Power_Task(uint32_t interval_ms);

App_Power_StateTypeDef App_Power_GetState(void);
bool App_Power_IsDischargeAllowed(void);
void App_Power_PrintSnapshot(void);
void App_Power_PrintStopReason(void);
bool App_Power_ClearDischargeFault(void);
/* 跨任务调用只提交请求；FET、EEPROM 和 BQ 操作由 Power 任务串行执行。 */
bool App_Power_RequestBqShutdown(void);

#endif /* APP_POWER_H */
