#ifndef APP_SAFETY_H
#define APP_SAFETY_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#include "Int_Fault.h"

typedef enum
{
    APP_SAFETY_TASK_BATMAN = 0,
    APP_SAFETY_TASK_POWER,
    APP_SAFETY_TASK_SC8815,
    APP_SAFETY_TASK_CAN,
    APP_SAFETY_TASK_COUNT
} App_SafetyTaskIdTypeDef;

typedef enum
{
    APP_SAFETY_WAKE_REASON_NONE = 0,
    APP_SAFETY_WAKE_REASON_BQ_HOST_SHUTDOWN
} App_SafetyWakeReasonTypeDef;

typedef struct
{
    App_SafetyWakeReasonTypeDef reason;
    bool host_requested;
    bool shutdown_command_succeeded;
    bool expected_offline_seen;
    bool input_valid;
    bool sc_ready;
} App_SafetyWakeEvidenceTypeDef;

#define APP_SAFETY_INHIBIT_BQ_INIT (1u << 0)
#define APP_SAFETY_INHIBIT_SC_INIT (1u << 1)
#define APP_SAFETY_INHIBIT_BQ_ALERT (1u << 3)
#define APP_SAFETY_INHIBIT_SC_EVENT (1u << 4)
#define APP_SAFETY_INHIBIT_BQ_PROTECTION_LATCHED (1u << 5)
#define APP_SAFETY_INHIBIT_LATCHED_FAULT (1u << 31)

typedef struct
{
    Int_FaultSnapshotTypeDef fault;
    uint32_t healthy_mask;
    uint32_t seen_mask;
    uint32_t deadline_miss_mask;
    uint32_t stack_fault_mask;
    uint32_t overrun_count[APP_SAFETY_TASK_COUNT];
    uint32_t heartbeat_count[APP_SAFETY_TASK_COUNT];
    uint32_t heartbeat_age_ms[APP_SAFETY_TASK_COUNT];
    UBaseType_t stack_high_water[APP_SAFETY_TASK_COUNT];
    UBaseType_t supervisor_stack_high_water;
    uint32_t power_inhibit_mask;
    uint32_t authorization_epoch;
    uint32_t bq_protection_context;
    bool bq_protection_latched;
    bool bq_early_safe_failed;
    App_SafetyWakeReasonTypeDef wake_reason;
    bool wake_authorization_active;
    bool watchdog_started;
    bool startup_grace_active;
} App_SafetySnapshotTypeDef;

void App_Safety_Init(void);
void App_Safety_RegisterTask(App_SafetyTaskIdTypeDef id,
                             TaskHandle_t handle,
                             uint32_t deadline_ms,
                             UBaseType_t minimum_stack_words);
void App_Safety_RegisterSupervisor(TaskHandle_t handle, UBaseType_t minimum_stack_words);
void App_Safety_Heartbeat(App_SafetyTaskIdTypeDef id);
void App_Safety_RecordOverrun(App_SafetyTaskIdTypeDef id);
void App_Safety_OnBqAlertFromISR(void);
void App_Safety_OnScInterruptFromISR(void);
void App_Safety_ResolveBqAlert(bool critical, uint32_t context);
void App_Safety_ReportBqEarlySafeResult(bool outputs_confirmed_off);
void App_Safety_ReportBqReady(bool ready);
void App_Safety_ReportScReady(bool ready);
void App_Safety_SetPowerInhibit(uint32_t inhibit_mask);
void App_Safety_ClearPowerInhibit(uint32_t inhibit_mask);

/**
 * @brief 获取/校验功率释放代次令牌。
 *
 * BQ 主 FET 放行和 SC8815 start 必须携带同一 epoch；任何 trip 都会使旧令牌失效。
 */
bool App_Safety_GetPowerReleaseAuthorization(uint32_t *epoch);
bool App_Safety_GetBqWakeAuthorization(const App_SafetyWakeEvidenceTypeDef *evidence,
                                       uint32_t *epoch);
bool App_Safety_IsPowerReleaseAuthorized(uint32_t epoch);

void App_Safety_Task(void *argument);
void App_Safety_GetSnapshot(App_SafetySnapshotTypeDef *snapshot);

#endif /* APP_SAFETY_H */
