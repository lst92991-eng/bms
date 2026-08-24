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

typedef enum
{
    APP_POWER_STOP_NONE = 0,
    APP_POWER_STOP_BOOT,
    APP_POWER_STOP_HOST_SHUTDOWN,
    APP_POWER_STOP_BQ_OFFLINE,
    APP_POWER_STOP_CONFIG_INVALID,
    APP_POWER_STOP_CELL_SAMPLE_INVALID,
    APP_POWER_STOP_CELL_TEMP_INVALID,
    APP_POWER_STOP_CELL_TEMP_RANGE,
    APP_POWER_STOP_BQ_FAULT,
    APP_POWER_STOP_SCD_LATCHED,
    APP_POWER_STOP_LOW_CELL,
    APP_POWER_STOP_DISCHARGE_OVERCURRENT,
    APP_POWER_STOP_CHARGE_COMPLETE,
    APP_POWER_STOP_CHARGE_VOLTAGE_LIMIT,
    APP_POWER_STOP_TOP_BALANCE,
    APP_POWER_STOP_CHARGE_INHIBITED,
    APP_POWER_STOP_NO_INPUT,
    APP_POWER_STOP_SC_FAULT,
    APP_POWER_STOP_SAFETY_NOT_AUTHORIZED,
    APP_POWER_STOP_COMMAND_FAILED,
    APP_POWER_STOP_SHUTDOWN_FAILED,
    APP_POWER_STOP_WAKE_TIMEOUT,
    APP_POWER_STOP_WAKE_DISABLED
} App_Power_StopReasonTypeDef;

typedef struct
{
    bool host_requested;
    bool shutdown_command_succeeded;
    bool expected_offline_seen;
    bool valid_input_seen;
    bool wake_authorization_issued;
    bool wake_pulse_started;
    bool recovery_config_succeeded;
    bool post_recovery_frame_seen;
} App_Power_ShutdownProvenanceTypeDef;

void App_Power_Init(void);
void App_Power_Task(uint32_t interval_ms);

App_Power_StateTypeDef App_Power_GetState(void);
bool App_Power_IsDischargeAllowed(void);
void App_Power_PrintSnapshot(void);
void App_Power_PrintStopReason(void);
bool App_Power_ClearDischargeFault(void);
/* 跨任务调用只提交请求；FET、EEPROM 和 BQ 操作由 Power 任务串行执行。 */
bool App_Power_RequestBqShutdown(void);
bool App_Power_RequestChargeInhibit(bool inhibit);
App_Power_StopReasonTypeDef App_Power_GetStopReason(void);
void App_Power_GetShutdownProvenance(App_Power_ShutdownProvenanceTypeDef *provenance);

#endif /* APP_POWER_H */
