#include "App_Main.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdbool.h>
#include <stdint.h>

#include "App_BatMan.h"
#include "App_Buzzer.h"
#include "App_CanBms.h"
#include "App_DebugCli.h"
#include "App_OLED.h"
#include "App_Power.h"
#include "App_SC8815.h"
#include "App_Safety.h"
#include "Int_Buzzer.h"
#include "Int_Fault.h"
#include "Int_I2C2Bus.h"
#include "Int_Led.h"
#include "Int_Log.h"
#include "Int_SC8815.h"
#include "main.h"

enum
{
    APP_MAIN_BATMAN_TASK_PERIOD_MS = 1000u,
    APP_MAIN_SC8815_TASK_PERIOD_MS = 20u,
    APP_MAIN_CAN_BMS_TASK_PERIOD_MS = 20u,
    APP_MAIN_DEBUG_CLI_TASK_PERIOD_MS = 20u,
    APP_MAIN_MAINTENANCE_TASK_PERIOD_MS = 250u,
    APP_MAIN_BUZZER_TASK_PERIOD_MS = 10u,
    APP_MAIN_BATMAN_DEADLINE_MS = 2500u,
    APP_MAIN_POWER_DEADLINE_MS = 2500u,
    APP_MAIN_SC8815_DEADLINE_MS = 100u,
    APP_MAIN_CAN_BMS_DEADLINE_MS = 150u,
    APP_MAIN_SAFETY_STACK_WORDS = 320u,
    APP_MAIN_BATMAN_STACK_WORDS = 1024u,
    APP_MAIN_SC8815_STACK_WORDS = 512u,
    APP_MAIN_CAN_BMS_STACK_WORDS = 512u,
    APP_MAIN_DEBUG_CLI_STACK_WORDS = 512u,
    APP_MAIN_MAINTENANCE_STACK_WORDS = 768u,
    APP_MAIN_BUZZER_STACK_WORDS = 256u,
    APP_MAIN_IDLE_STACK_WORDS = configMINIMAL_STACK_SIZE,
    APP_MAIN_SAFETY_STACK_MARGIN_WORDS = 64u,
    APP_MAIN_BATMAN_STACK_MARGIN_WORDS = 96u,
    APP_MAIN_SC8815_STACK_MARGIN_WORDS = 64u,
    APP_MAIN_CAN_BMS_STACK_MARGIN_WORDS = 64u
};

static StaticTask_t s_safety_tcb;
static StackType_t s_safety_stack[APP_MAIN_SAFETY_STACK_WORDS];
static TaskHandle_t s_safety_handle = NULL;
static StaticTask_t s_batman_tcb;
static StackType_t s_batman_stack[APP_MAIN_BATMAN_STACK_WORDS];
static TaskHandle_t s_batman_handle = NULL;
static StaticTask_t s_sc8815_tcb;
static StackType_t s_sc8815_stack[APP_MAIN_SC8815_STACK_WORDS];
static TaskHandle_t s_sc8815_handle = NULL;
static StaticTask_t s_can_bms_tcb;
static StackType_t s_can_bms_stack[APP_MAIN_CAN_BMS_STACK_WORDS];
static TaskHandle_t s_can_bms_handle = NULL;

#if BMS_ENGINEERING_BUILD
static StaticTask_t s_debug_cli_tcb;
static StackType_t s_debug_cli_stack[APP_MAIN_DEBUG_CLI_STACK_WORDS];
static TaskHandle_t s_debug_cli_handle = NULL;
#endif

static StaticTask_t s_maintenance_tcb;
static StackType_t s_maintenance_stack[APP_MAIN_MAINTENANCE_STACK_WORDS];
static TaskHandle_t s_maintenance_handle = NULL;
static StaticTask_t s_buzzer_tcb;
static StackType_t s_buzzer_stack[APP_MAIN_BUZZER_STACK_WORDS];
static TaskHandle_t s_buzzer_handle = NULL;
static StaticTask_t s_idle_tcb;
static StackType_t s_idle_stack[APP_MAIN_IDLE_STACK_WORDS];

static uint32_t App_Main_TicksToMs(TickType_t ticks)
{
    uint64_t value_ms = ((uint64_t)ticks * 1000u) / configTICK_RATE_HZ;

    return (value_ms > UINT32_MAX) ? UINT32_MAX : (uint32_t)value_ms;
}

static void App_Main_DelayCriticalTask(TickType_t *last_wake,
                                       uint32_t period_ms,
                                       App_SafetyTaskIdTypeDef primary_id,
                                       bool record_power)
{
    TickType_t now = xTaskGetTickCount();
    TickType_t period_ticks = pdMS_TO_TICKS(period_ms);

    /* 超周期时从当前时刻重建相位，禁止 vTaskDelayUntil 连续 catch-up。 */
    if ((TickType_t)(now - *last_wake) >= period_ticks)
    {
        App_Safety_RecordOverrun(primary_id);
        if (record_power)
        {
            App_Safety_RecordOverrun(APP_SAFETY_TASK_POWER);
        }
        *last_wake = now;
    }
    vTaskDelayUntil(last_wake, period_ticks);
}

static void batman_task(void *argument)
{
    TickType_t last_wake;
    TickType_t last_run;

    (void)argument;
    last_wake = xTaskGetTickCount();
    last_run = last_wake;
    while (1)
    {
        TickType_t now = xTaskGetTickCount();
        uint32_t elapsed_ms = App_Main_TicksToMs(now - last_run);

        last_run = now;
        App_BatMan_Task(elapsed_ms);
        App_Safety_Heartbeat(APP_SAFETY_TASK_BATMAN);
        App_Power_Task(elapsed_ms);
        App_Safety_Heartbeat(APP_SAFETY_TASK_POWER);
        App_Main_DelayCriticalTask(
            &last_wake, APP_MAIN_BATMAN_TASK_PERIOD_MS, APP_SAFETY_TASK_BATMAN, true);
    }
}

static void sc8815_task(void *argument)
{
    TickType_t last_wake;

    (void)argument;
    last_wake = xTaskGetTickCount();
    while (1)
    {
        App_SC8815_Task(APP_MAIN_SC8815_TASK_PERIOD_MS);
        App_Safety_Heartbeat(APP_SAFETY_TASK_SC8815);
        App_Main_DelayCriticalTask(
            &last_wake, APP_MAIN_SC8815_TASK_PERIOD_MS, APP_SAFETY_TASK_SC8815, false);
    }
}

static void can_bms_task(void *argument)
{
    TickType_t last_wake;
    TickType_t last_run;

    (void)argument;
    last_wake = xTaskGetTickCount();
    last_run = last_wake;
    while (1)
    {
        TickType_t now = xTaskGetTickCount();
        uint32_t elapsed_ms = App_Main_TicksToMs(now - last_run);

        last_run = now;
        App_CanBms_Task(elapsed_ms);
        App_Safety_Heartbeat(APP_SAFETY_TASK_CAN);
        App_Main_DelayCriticalTask(
            &last_wake, APP_MAIN_CAN_BMS_TASK_PERIOD_MS, APP_SAFETY_TASK_CAN, false);
    }
}

#if BMS_ENGINEERING_BUILD
static void debug_cli_task(void *argument)
{
    TickType_t last_wake;

    (void)argument;
    last_wake = xTaskGetTickCount();
    while (1)
    {
        App_DebugCli_Task(APP_MAIN_DEBUG_CLI_TASK_PERIOD_MS);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(APP_MAIN_DEBUG_CLI_TASK_PERIOD_MS));
    }
}
#endif

static void maintenance_task(void *argument)
{
    TickType_t last_wake;
    TickType_t last_run;

    (void)argument;
    last_wake = xTaskGetTickCount();
    last_run = last_wake;
    while (1)
    {
        TickType_t now = xTaskGetTickCount();
        uint32_t elapsed_ms = App_Main_TicksToMs(now - last_run);

        last_run = now;
        /* EEPROM 与 OLED 共用 I2C2，固定在同一低优先级任务内串行访问。 */
        App_BatMan_MaintenanceTask(elapsed_ms);
        App_OLED_Task(elapsed_ms);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(APP_MAIN_MAINTENANCE_TASK_PERIOD_MS));
    }
}

static void buzzer_task(void *argument)
{
    TickType_t last_wake;

    (void)argument;
    last_wake = xTaskGetTickCount();
    while (1)
    {
        App_Buzzer_Task(HAL_GetTick());
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(APP_MAIN_BUZZER_TASK_PERIOD_MS));
    }
}

static void App_Main_Init(bool boot_early_bq_safe)
{
    bool batman_ready;
    bool can_ready;
    bool early_bq_safe;

    /* 复位后前两条外设动作固定为 SC PSTOP 与 BQ ALL_FETS_OFF。 */
    Int_SC8815_ForceStandby();
    early_bq_safe = App_BatMan_EarlySafeOutputs();
    App_Safety_Init();
    App_Safety_ReportBqEarlySafeResult(boot_early_bq_safe && early_bq_safe);
    if (!Int_I2C2Bus_Init())
    {
        Int_Fault_Panic(INT_FAULT_CRITICAL_INIT, 0x49324332u);
    }

    /* 完整初始化会再次无日志强化全关；失败则安全驻留并由任务保留诊断。 */
    batman_ready = App_BatMan_Init();
    App_Safety_ReportBqReady(batman_ready && App_BatMan_IsOnline() && App_BatMan_IsConfigValid());

    Int_Log_Printf("APP初始化: LED\r\n");
    Int_Led_Init();
    Int_Log_Printf("APP初始化: 蜂鸣器\r\n");
    Int_Buzzer_Init();
    App_Buzzer_Init();
    Int_Log_Printf("APP初始化: OLED\r\n");
    App_OLED_Init();

    Int_Log_Printf("APP初始化: SC8815\r\n");
    App_Safety_ReportScReady(false);
    App_SC8815_Init();

    Int_Log_Printf("APP初始化: 电源管理\r\n");
    App_Power_Init();
    Int_Log_Printf("APP初始化: CANFD协议\r\n");
    can_ready = App_CanBms_Init();
    if (!can_ready)
    {
        /* CAN 失联仅降级诊断，不得代替本地保护链停止充放电。 */
        Int_Log_Printf("CANFD初始化失败: 本地保护继续运行\r\n");
    }

#if BMS_ENGINEERING_BUILD
    Int_Log_Printf("APP初始化: 工程CLI\r\n");
    App_DebugCli_Init();
#endif

    if (Int_Fault_IsLatched())
    {
        Int_Fault_Panic(Int_Fault_GetLatchedCode(), 0u);
    }
    Int_Log_Printf("APP初始化: 完成\r\n");
}

static bool App_Main_CreateTasks(void)
{
    s_safety_handle = xTaskCreateStatic(App_Safety_Task,
                                        "safety",
                                        APP_MAIN_SAFETY_STACK_WORDS,
                                        NULL,
                                        4u,
                                        s_safety_stack,
                                        &s_safety_tcb);
    s_batman_handle = xTaskCreateStatic(batman_task,
                                        "batman",
                                        APP_MAIN_BATMAN_STACK_WORDS,
                                        NULL,
                                        3u,
                                        s_batman_stack,
                                        &s_batman_tcb);
    s_sc8815_handle = xTaskCreateStatic(sc8815_task,
                                        "sc8815",
                                        APP_MAIN_SC8815_STACK_WORDS,
                                        NULL,
                                        2u,
                                        s_sc8815_stack,
                                        &s_sc8815_tcb);
    s_can_bms_handle = xTaskCreateStatic(can_bms_task,
                                         "can_bms",
                                         APP_MAIN_CAN_BMS_STACK_WORDS,
                                         NULL,
                                         2u,
                                         s_can_bms_stack,
                                         &s_can_bms_tcb);
    s_maintenance_handle = xTaskCreateStatic(maintenance_task,
                                             "maintenance",
                                             APP_MAIN_MAINTENANCE_STACK_WORDS,
                                             NULL,
                                             1u,
                                             s_maintenance_stack,
                                             &s_maintenance_tcb);
    s_buzzer_handle = xTaskCreateStatic(buzzer_task,
                                        "buzzer",
                                        APP_MAIN_BUZZER_STACK_WORDS,
                                        NULL,
                                        1u,
                                        s_buzzer_stack,
                                        &s_buzzer_tcb);
#if BMS_ENGINEERING_BUILD
    s_debug_cli_handle = xTaskCreateStatic(debug_cli_task,
                                           "debug_cli",
                                           APP_MAIN_DEBUG_CLI_STACK_WORDS,
                                           NULL,
                                           1u,
                                           s_debug_cli_stack,
                                           &s_debug_cli_tcb);
#endif

    if ((s_safety_handle == NULL) || (s_batman_handle == NULL) || (s_sc8815_handle == NULL) ||
        (s_can_bms_handle == NULL) || (s_maintenance_handle == NULL) || (s_buzzer_handle == NULL))
    {
        return false;
    }
#if BMS_ENGINEERING_BUILD
    if (s_debug_cli_handle == NULL)
    {
        return false;
    }
#endif

    App_Safety_RegisterSupervisor(s_safety_handle, APP_MAIN_SAFETY_STACK_MARGIN_WORDS);
    App_Safety_RegisterTask(APP_SAFETY_TASK_BATMAN,
                            s_batman_handle,
                            APP_MAIN_BATMAN_DEADLINE_MS,
                            APP_MAIN_BATMAN_STACK_MARGIN_WORDS);
    App_Safety_RegisterTask(APP_SAFETY_TASK_POWER,
                            s_batman_handle,
                            APP_MAIN_POWER_DEADLINE_MS,
                            APP_MAIN_BATMAN_STACK_MARGIN_WORDS);
    App_Safety_RegisterTask(APP_SAFETY_TASK_SC8815,
                            s_sc8815_handle,
                            APP_MAIN_SC8815_DEADLINE_MS,
                            APP_MAIN_SC8815_STACK_MARGIN_WORDS);
    App_Safety_RegisterTask(APP_SAFETY_TASK_CAN,
                            s_can_bms_handle,
                            APP_MAIN_CAN_BMS_DEADLINE_MS,
                            APP_MAIN_CAN_BMS_STACK_MARGIN_WORDS);
    return true;
}

void vApplicationGetIdleTaskMemory(StaticTask_t **idle_tcb,
                                   StackType_t **idle_stack,
                                   uint32_t *idle_stack_size)
{
    *idle_tcb = &s_idle_tcb;
    *idle_stack = s_idle_stack;
    *idle_stack_size = APP_MAIN_IDLE_STACK_WORDS;
}

void App_Main(bool boot_early_bq_safe)
{
    App_Main_Init(boot_early_bq_safe);
    Int_Log_Printf("RTOS: 创建静态任务\r\n");
    if (!App_Main_CreateTasks())
    {
        /* 调度器尚未启动，不允许任何已创建任务形成“部分运行”系统。 */
        Int_Fault_Panic(INT_FAULT_TASK_CREATE, 0u);
    }

    Int_Log_Printf("RTOS: 启动调度器\r\n");
    vTaskStartScheduler();
    Int_Fault_Panic(INT_FAULT_SCHEDULER_RETURN, 0u);
}
