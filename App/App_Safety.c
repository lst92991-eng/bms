#include "App_Safety.h"

#include <stddef.h>
#include <string.h>

#include "Int_SC8815.h"
#include "Int_Watchdog.h"

enum
{
    APP_SAFETY_PERIOD_MS = 10u,
    APP_SAFETY_STARTUP_GRACE_MS = 3000u,
    APP_SAFETY_WATCHDOG_TIMEOUT_MS = 6000u,
    APP_SAFETY_STACK_CHECK_PERIOD_MS = 100u,
    APP_SAFETY_BQ_WAKE_AUTHORIZATION_MS = 500u
};

typedef struct
{
    TaskHandle_t handle;
    TickType_t deadline_ticks;
    UBaseType_t minimum_stack_words;
    volatile TickType_t last_heartbeat_tick;
    volatile uint32_t heartbeat_count;
    volatile uint32_t overrun_count;
    volatile bool seen;
    bool registered;
    UBaseType_t stack_high_water;
} App_SafetyTaskHealthTypeDef;

static App_SafetyTaskHealthTypeDef s_task_health[APP_SAFETY_TASK_COUNT];
static TaskHandle_t s_supervisor_handle = NULL;
static UBaseType_t s_supervisor_minimum_stack_words = 0u;
static UBaseType_t s_supervisor_stack_high_water = 0u;
static TickType_t s_start_tick = 0u;
static TickType_t s_last_stack_check_tick = 0u;
static volatile uint32_t s_deadline_miss_mask = 0u;
static volatile uint32_t s_stack_fault_mask = 0u;
static volatile uint32_t s_authorization_epoch = 1u;
static volatile bool s_power_release_authorized = false;
static volatile uint32_t s_power_inhibit_mask = 0u;
static volatile uint32_t s_bq_protection_context = 0u;
static volatile bool s_bq_early_safe_failed = false;
static volatile bool s_software_healthy = false;
static volatile bool s_bq_wake_authorized = false;
static volatile bool s_bq_wake_issued = false;
static volatile uint32_t s_bq_wake_epoch = 0u;
static volatile TickType_t s_bq_wake_issue_tick = 0u;
static volatile App_SafetyWakeReasonTypeDef s_bq_wake_reason = APP_SAFETY_WAKE_REASON_NONE;

static uint32_t App_Safety_Lock(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    return primask;
}

static void App_Safety_Unlock(uint32_t primask)
{
    if (primask == 0u)
    {
        __enable_irq();
    }
}

static void App_Safety_UpdatePowerInhibit(uint32_t set_mask, uint32_t clear_mask)
{
    /* 新增任何禁止位或唤醒授权收回时，必须先物理断开。 */
    if ((set_mask != 0u) || s_bq_wake_authorized)
    {
        Int_SC8815_ForceStandby();
    }
    uint32_t primask = App_Safety_Lock();
    uint32_t old_mask = s_power_inhibit_mask;

    s_power_release_authorized = false;
    s_bq_wake_authorized = false;
    s_power_inhibit_mask |= set_mask;
    s_power_inhibit_mask &= ~clear_mask;
    if (((old_mask & APP_SAFETY_INHIBIT_BQ_INIT) != 0u) &&
        ((s_power_inhibit_mask & APP_SAFETY_INHIBIT_BQ_INIT) == 0u))
    {
        s_bq_wake_issued = false;
        s_bq_wake_reason = APP_SAFETY_WAKE_REASON_NONE;
    }
    s_authorization_epoch++;
    if (s_authorization_epoch == 0u)
    {
        s_authorization_epoch = 1u;
    }
    App_Safety_Unlock(primask);
}

static void App_Safety_ExpireBqWakeAuthorization(void)
{
    uint32_t primask;

    /* TTL/健康失效的第一条硬件动作必须是 PSTOP，不能只清软件标志。 */
    Int_SC8815_ForceStandby();
    primask = App_Safety_Lock();
    if (s_bq_wake_authorized)
    {
        s_bq_wake_authorized = false;
        s_authorization_epoch++;
        if (s_authorization_epoch == 0u)
        {
            s_authorization_epoch = 1u;
        }
    }
    App_Safety_Unlock(primask);
}

static void App_Safety_Trip(Int_FaultCodeTypeDef code, uint32_t context)
{
    App_Safety_UpdatePowerInhibit(APP_SAFETY_INHIBIT_LATCHED_FAULT, 0u);
    Int_Fault_Trip(code, context);
}

static uint32_t App_Safety_TicksToMs(TickType_t ticks)
{
    uint64_t value_ms = ((uint64_t)ticks * 1000u) / configTICK_RATE_HZ;

    return (value_ms > UINT32_MAX) ? UINT32_MAX : (uint32_t)value_ms;
}

static bool App_Safety_AllTasksRegistered(void)
{
    uint32_t id;

    for (id = 0u; id < APP_SAFETY_TASK_COUNT; id++)
    {
        if (!s_task_health[id].registered)
        {
            return false;
        }
    }
    return true;
}

static bool App_Safety_AllTasksSeen(void)
{
    uint32_t id;

    for (id = 0u; id < APP_SAFETY_TASK_COUNT; id++)
    {
        if (!s_task_health[id].seen)
        {
            return false;
        }
    }
    return true;
}

static bool App_Safety_CheckDeadlines(TickType_t now)
{
    uint32_t id;

    for (id = 0u; id < APP_SAFETY_TASK_COUNT; id++)
    {
        App_SafetyTaskHealthTypeDef *health = &s_task_health[id];

        if (!health->registered || !health->seen)
        {
            return false;
        }
        if ((TickType_t)(now - health->last_heartbeat_tick) > health->deadline_ticks)
        {
            uint32_t context =
                (id << 24u) |
                (App_Safety_TicksToMs(now - health->last_heartbeat_tick) & 0x00FFFFFFu);

            s_deadline_miss_mask |= (1u << id);
            App_Safety_Trip(INT_FAULT_TASK_DEADLINE, context);
            return false;
        }
    }
    return true;
}

static bool App_Safety_CheckStacks(void)
{
    uint32_t id;

    for (id = 0u; id < APP_SAFETY_TASK_COUNT; id++)
    {
        App_SafetyTaskHealthTypeDef *health = &s_task_health[id];

        health->stack_high_water = uxTaskGetStackHighWaterMark(health->handle);
        if (health->stack_high_water <= health->minimum_stack_words)
        {
            uint32_t context = (id << 24u) | ((uint32_t)health->stack_high_water & 0x00FFFFFFu);

            s_stack_fault_mask |= (1u << id);
            App_Safety_Trip(INT_FAULT_STACK_MARGIN, context);
            return false;
        }
    }

    s_supervisor_stack_high_water = uxTaskGetStackHighWaterMark(s_supervisor_handle);
    if (s_supervisor_stack_high_water <= s_supervisor_minimum_stack_words)
    {
        s_stack_fault_mask |= (1u << APP_SAFETY_TASK_COUNT);
        App_Safety_Trip(INT_FAULT_STACK_MARGIN,
                        (APP_SAFETY_TASK_COUNT << 24u) |
                            ((uint32_t)s_supervisor_stack_high_water & 0x00FFFFFFu));
        return false;
    }
    return true;
}

void App_Safety_Init(void)
{
    uint32_t primask = App_Safety_Lock();

    memset(s_task_health, 0, sizeof(s_task_health));
    s_supervisor_handle = NULL;
    s_supervisor_minimum_stack_words = 0u;
    s_supervisor_stack_high_water = 0u;
    s_deadline_miss_mask = 0u;
    s_stack_fault_mask = 0u;
    s_authorization_epoch = 1u;
    s_power_release_authorized = false;
    s_power_inhibit_mask = 0u;
    s_bq_protection_context = 0u;
    s_bq_early_safe_failed = false;
    s_software_healthy = false;
    s_bq_wake_authorized = false;
    s_bq_wake_issued = false;
    s_bq_wake_epoch = 0u;
    s_bq_wake_issue_tick = 0u;
    s_bq_wake_reason = APP_SAFETY_WAKE_REASON_NONE;
    App_Safety_Unlock(primask);

    Int_Fault_Init();
}

void App_Safety_RegisterTask(App_SafetyTaskIdTypeDef id,
                             TaskHandle_t handle,
                             uint32_t deadline_ms,
                             UBaseType_t minimum_stack_words)
{
    App_SafetyTaskHealthTypeDef *health;

    if ((id >= APP_SAFETY_TASK_COUNT) || (handle == NULL) || (deadline_ms == 0u))
    {
        Int_Fault_Panic(INT_FAULT_TASK_CREATE, (uint32_t)id);
    }

    health = &s_task_health[id];
    health->handle = handle;
    health->deadline_ticks = pdMS_TO_TICKS(deadline_ms);
    health->minimum_stack_words = minimum_stack_words;
    health->last_heartbeat_tick = 0u;
    health->heartbeat_count = 0u;
    health->overrun_count = 0u;
    health->seen = false;
    health->registered = true;
    health->stack_high_water = 0u;
}

void App_Safety_RegisterSupervisor(TaskHandle_t handle, UBaseType_t minimum_stack_words)
{
    if (handle == NULL)
    {
        Int_Fault_Panic(INT_FAULT_TASK_CREATE, APP_SAFETY_TASK_COUNT);
    }
    s_supervisor_handle = handle;
    s_supervisor_minimum_stack_words = minimum_stack_words;
}

void App_Safety_Heartbeat(App_SafetyTaskIdTypeDef id)
{
    uint32_t primask;

    if (id >= APP_SAFETY_TASK_COUNT)
    {
        return;
    }

    primask = App_Safety_Lock();
    s_task_health[id].last_heartbeat_tick = xTaskGetTickCount();
    s_task_health[id].heartbeat_count++;
    s_task_health[id].seen = true;
    App_Safety_Unlock(primask);
}

void App_Safety_RecordOverrun(App_SafetyTaskIdTypeDef id)
{
    uint32_t primask;

    if (id >= APP_SAFETY_TASK_COUNT)
    {
        return;
    }
    primask = App_Safety_Lock();
    s_task_health[id].overrun_count++;
    App_Safety_Unlock(primask);
}

void App_Safety_OnBqAlertFromISR(void)
{
    /* ALERT 先撤权并等待完整状态帧判因；active-low 状态源不能直接形成重启环。 */
    App_Safety_UpdatePowerInhibit(APP_SAFETY_INHIBIT_BQ_ALERT, 0u);
}

void App_Safety_OnScInterruptFromISR(void)
{
    /* SC INT 需由任务判因，但旧 start 令牌必须在 ISR 内立即失效。 */
    App_Safety_UpdatePowerInhibit(APP_SAFETY_INHIBIT_SC_EVENT, 0u);
}

void App_Safety_ResolveBqAlert(bool critical, uint32_t context)
{
    if (critical)
    {
        uint32_t primask = App_Safety_Lock();

        if ((s_power_inhibit_mask & APP_SAFETY_INHIBIT_BQ_PROTECTION_LATCHED) == 0u)
        {
            s_bq_protection_context = context;
        }
        App_Safety_Unlock(primask);
        /* 电池保护只锁功率并保留采样诊断；软件完整性正常时继续喂狗。 */
        App_Safety_UpdatePowerInhibit(
            APP_SAFETY_INHIBIT_BQ_ALERT | APP_SAFETY_INHIBIT_BQ_PROTECTION_LATCHED, 0u);
    }
    else
    {
        App_Safety_UpdatePowerInhibit(0u, APP_SAFETY_INHIBIT_BQ_ALERT);
    }
}

void App_Safety_ReportBqEarlySafeResult(bool outputs_confirmed_off)
{
    uint32_t primask = App_Safety_Lock();

    s_bq_early_safe_failed = !outputs_confirmed_off;
    App_Safety_Unlock(primask);
    /* 完整配置与首帧确认前，无论 early 结果如何都保持 BQ_INIT 门禁。 */
    App_Safety_SetPowerInhibit(APP_SAFETY_INHIBIT_BQ_INIT);
}

void App_Safety_ReportBqReady(bool ready)
{
    /* 最早关断证据缺失属于本次启动不可恢复事件，后续通信恢复不能自动抹掉。 */
    if (ready && !s_bq_early_safe_failed)
    {
        App_Safety_ClearPowerInhibit(APP_SAFETY_INHIBIT_BQ_INIT);
    }
    else
    {
        App_Safety_SetPowerInhibit(APP_SAFETY_INHIBIT_BQ_INIT);
    }
}

void App_Safety_ReportScReady(bool ready)
{
    if (ready)
    {
        App_Safety_ClearPowerInhibit(APP_SAFETY_INHIBIT_SC_INIT);
    }
    else
    {
        App_Safety_SetPowerInhibit(APP_SAFETY_INHIBIT_SC_INIT);
    }
}

void App_Safety_SetPowerInhibit(uint32_t inhibit_mask)
{
    if (inhibit_mask != 0u)
    {
        App_Safety_UpdatePowerInhibit(inhibit_mask, 0u);
    }
}

void App_Safety_ClearPowerInhibit(uint32_t inhibit_mask)
{
    /* 软件故障及 BQ 关键保护均不能由通用运行期 API 解除。 */
    inhibit_mask &= ~(APP_SAFETY_INHIBIT_LATCHED_FAULT | APP_SAFETY_INHIBIT_BQ_PROTECTION_LATCHED);
    if (inhibit_mask != 0u)
    {
        App_Safety_UpdatePowerInhibit(0u, inhibit_mask);
    }
}

bool App_Safety_GetPowerReleaseAuthorization(uint32_t *epoch)
{
    uint32_t primask;
    bool authorized;

    if (epoch == NULL)
    {
        return false;
    }
    primask = App_Safety_Lock();
    authorized =
        s_power_release_authorized && (s_power_inhibit_mask == 0u) && !Int_Fault_IsLatched();
    *epoch = s_authorization_epoch;
    App_Safety_Unlock(primask);
    return authorized;
}

bool App_Safety_GetBqWakeAuthorization(const App_SafetyWakeEvidenceTypeDef *evidence,
                                       uint32_t *epoch)
{
    uint32_t primask;
    bool evidence_valid;
    bool authorized;

    if ((evidence == NULL) || (epoch == NULL))
    {
        return false;
    }
    evidence_valid = (evidence->reason == APP_SAFETY_WAKE_REASON_BQ_HOST_SHUTDOWN) &&
                     evidence->host_requested && evidence->shutdown_command_succeeded &&
                     evidence->expected_offline_seen && evidence->input_valid && evidence->sc_ready;

    primask = App_Safety_Lock();
    authorized = evidence_valid && s_software_healthy && !s_bq_wake_issued &&
                 !Int_Fault_IsLatched() && (s_power_inhibit_mask == APP_SAFETY_INHIBIT_BQ_INIT);
    if (authorized)
    {
        s_authorization_epoch++;
        if (s_authorization_epoch == 0u)
        {
            s_authorization_epoch = 1u;
        }
        s_power_release_authorized = false;
        s_bq_wake_authorized = true;
        s_bq_wake_issued = true;
        s_bq_wake_epoch = s_authorization_epoch;
        s_bq_wake_issue_tick = xTaskGetTickCount();
        s_bq_wake_reason = evidence->reason;
        *epoch = s_bq_wake_epoch;
    }
    else
    {
        *epoch = 0u;
    }
    App_Safety_Unlock(primask);
    return authorized;
}

bool App_Safety_IsPowerReleaseAuthorized(uint32_t epoch)
{
    uint32_t primask = App_Safety_Lock();
    TickType_t now = xTaskGetTickCount();
    bool normal_authorized = s_power_release_authorized && (s_power_inhibit_mask == 0u) &&
                             !Int_Fault_IsLatched() && (epoch == s_authorization_epoch);
    bool wake_authorized = s_bq_wake_authorized && s_software_healthy && !Int_Fault_IsLatched() &&
                           (s_power_inhibit_mask == APP_SAFETY_INHIBIT_BQ_INIT) &&
                           (epoch == s_bq_wake_epoch) &&
                           ((TickType_t)(now - s_bq_wake_issue_tick) <
                            pdMS_TO_TICKS(APP_SAFETY_BQ_WAKE_AUTHORIZATION_MS));
    bool wake_expired =
        s_bq_wake_authorized && ((TickType_t)(now - s_bq_wake_issue_tick) >=
                                 pdMS_TO_TICKS(APP_SAFETY_BQ_WAKE_AUTHORIZATION_MS));
    bool authorized = normal_authorized || wake_authorized;

    App_Safety_Unlock(primask);
    if (wake_expired)
    {
        App_Safety_ExpireBqWakeAuthorization();
    }
    return authorized;
}

void App_Safety_Task(void *argument)
{
    TickType_t last_wake;
    TickType_t period_ticks = pdMS_TO_TICKS(APP_SAFETY_PERIOD_MS);

    (void)argument;
    s_start_tick = xTaskGetTickCount();
    s_last_stack_check_tick = s_start_tick;
    last_wake = s_start_tick;

    if (!Int_Watchdog_Start(APP_SAFETY_WATCHDOG_TIMEOUT_MS))
    {
        Int_Fault_Panic(INT_FAULT_WATCHDOG_INIT, APP_SAFETY_WATCHDOG_TIMEOUT_MS);
    }

    while (1)
    {
        TickType_t now = xTaskGetTickCount();
        bool grace_active =
            ((TickType_t)(now - s_start_tick) < pdMS_TO_TICKS(APP_SAFETY_STARTUP_GRACE_MS));
        bool healthy = !Int_Fault_IsLatched();

        if (!App_Safety_AllTasksRegistered())
        {
            healthy = false;
            if (!grace_active)
            {
                App_Safety_Trip(INT_FAULT_TASK_CREATE, 0xFFFFFFFFu);
            }
        }
        else if (!App_Safety_AllTasksSeen())
        {
            healthy = false;
            if (!grace_active)
            {
                uint32_t id;

                for (id = 0u; id < APP_SAFETY_TASK_COUNT; id++)
                {
                    if (!s_task_health[id].seen)
                    {
                        s_deadline_miss_mask |= (1u << id);
                        App_Safety_Trip(INT_FAULT_TASK_DEADLINE, id << 24u);
                        break;
                    }
                }
            }
        }
        else
        {
            healthy = healthy && App_Safety_CheckDeadlines(now);
            if (healthy && ((TickType_t)(now - s_last_stack_check_tick) >=
                            pdMS_TO_TICKS(APP_SAFETY_STACK_CHECK_PERIOD_MS)))
            {
                s_last_stack_check_tick = now;
                healthy = App_Safety_CheckStacks();
            }
        }

        if (s_bq_wake_authorized && ((TickType_t)(now - s_bq_wake_issue_tick) >=
                                     pdMS_TO_TICKS(APP_SAFETY_BQ_WAKE_AUTHORIZATION_MS)))
        {
            App_Safety_ExpireBqWakeAuthorization();
        }

        /* 喂狗权只属于监督任务，且必须在全部关键任务完成心跳后。 */
        if (healthy && App_Safety_AllTasksSeen() && !Int_Fault_IsLatched())
        {
            s_software_healthy = true;
            s_power_release_authorized = (s_power_inhibit_mask == 0u);
            Int_Watchdog_Refresh();
        }
        else
        {
            if (s_bq_wake_authorized)
            {
                App_Safety_ExpireBqWakeAuthorization();
            }
            s_software_healthy = false;
            s_power_release_authorized = false;
            s_bq_wake_authorized = false;
        }

        now = xTaskGetTickCount();
        if ((TickType_t)(now - last_wake) >= period_ticks)
        {
            last_wake = now;
        }
        vTaskDelayUntil(&last_wake, period_ticks);
    }
}

void App_Safety_GetSnapshot(App_SafetySnapshotTypeDef *snapshot)
{
    uint32_t primask;
    uint32_t id;
    TickType_t now;

    if (snapshot == NULL)
    {
        return;
    }

    memset(snapshot, 0, sizeof(*snapshot));
    Int_Fault_GetSnapshot(&snapshot->fault);
    now = xTaskGetTickCount();

    primask = App_Safety_Lock();
    snapshot->deadline_miss_mask = s_deadline_miss_mask;
    snapshot->stack_fault_mask = s_stack_fault_mask;
    snapshot->supervisor_stack_high_water = s_supervisor_stack_high_water;
    snapshot->power_inhibit_mask = s_power_inhibit_mask;
    snapshot->authorization_epoch = s_authorization_epoch;
    snapshot->bq_protection_context = s_bq_protection_context;
    snapshot->bq_protection_latched =
        (s_power_inhibit_mask & APP_SAFETY_INHIBIT_BQ_PROTECTION_LATCHED) != 0u;
    snapshot->bq_early_safe_failed = s_bq_early_safe_failed;
    snapshot->wake_reason = s_bq_wake_reason;
    snapshot->wake_authorization_active = s_bq_wake_authorized;
    snapshot->watchdog_started = Int_Watchdog_IsStarted();
    snapshot->startup_grace_active =
        ((TickType_t)(now - s_start_tick) < pdMS_TO_TICKS(APP_SAFETY_STARTUP_GRACE_MS));
    for (id = 0u; id < APP_SAFETY_TASK_COUNT; id++)
    {
        snapshot->overrun_count[id] = s_task_health[id].overrun_count;
        snapshot->heartbeat_count[id] = s_task_health[id].heartbeat_count;
        snapshot->stack_high_water[id] = s_task_health[id].stack_high_water;
        if (s_task_health[id].seen)
        {
            snapshot->seen_mask |= (1u << id);
            snapshot->heartbeat_age_ms[id] =
                App_Safety_TicksToMs(now - s_task_health[id].last_heartbeat_tick);
            if (snapshot->heartbeat_age_ms[id] <=
                App_Safety_TicksToMs(s_task_health[id].deadline_ticks))
            {
                snapshot->healthy_mask |= (1u << id);
            }
        }
    }
    App_Safety_Unlock(primask);
}
