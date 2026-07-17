#include "App_Main.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>
#include <stdint.h>

#include "App_BatMan.h"
#include "App_CanBms.h"
#include "App_DebugCli.h"
#include "App_OLED.h"
#include "App_Power.h"
#include "App_SC8815.h"
#include "Int_Led.h"
#include "main.h"

enum
{
    APP_MAIN_BATMAN_TASK_PERIOD_MS = 1000u,
    APP_MAIN_SC8815_TASK_PERIOD_MS = 1000u,
    APP_MAIN_CAN_BMS_TASK_PERIOD_MS = 20u,
    APP_MAIN_DEBUG_CLI_TASK_PERIOD_MS = 20u
};

static uint32_t App_Main_TicksToMs(TickType_t ticks)
{
    uint64_t value_ms = ((uint64_t)ticks * 1000u) / configTICK_RATE_HZ;

    return (value_ms > UINT32_MAX) ? UINT32_MAX : (uint32_t)value_ms;
}

/**
 * @brief BQ76952 电池监控任务。
 *
 * 周期读取电芯、电流、温度、告警状态，并立即推进功率策略。
 * BQ 数据和功率决策放在同一任务里，避免 power_task 读到半更新快照。
 */
static void batman_task(void *arg)
{
    TickType_t last_wake;
    TickType_t last_run;

    (void)arg;
    last_wake = xTaskGetTickCount();
    last_run = last_wake;

    while (1)
    {
        TickType_t now;
        uint32_t elapsed_ms;

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(APP_MAIN_BATMAN_TASK_PERIOD_MS));
        now = xTaskGetTickCount();
        elapsed_ms = App_Main_TicksToMs(now - last_run);
        last_run = now;

        App_BatMan_Task(elapsed_ms);
        App_Power_Task(elapsed_ms);
    }
}

/**
 * @brief SC8815 充电芯片监控任务。
 *
 * 只做状态读取和 charge_request 状态机推进；默认不主动请求充电，功率释放由
 * BatMan 任务中的 App_Power 策略显式决定。
 */
static void sc8815_task(void *arg)
{
    (void)arg;

    while (1)
    {
        App_SC8815_Task(APP_MAIN_SC8815_TASK_PERIOD_MS);
        vTaskDelay(APP_MAIN_SC8815_TASK_PERIOD_MS);
    }
}

/**
 * @brief CAN FD 查询和主动上报任务。
 *
 * 优先级必须低于 batman_task：BatMan/Power 在优先级 3 完成整帧发布后，CAN 才复制遥测快照，
 * 避免一帧报文混入两个采样周期的数据。
 */
static void can_bms_task(void *arg)
{
    TickType_t last_wake;
    TickType_t last_run;

    (void)arg;
    last_wake = xTaskGetTickCount();
    last_run = last_wake;

    while (1)
    {
        TickType_t now = xTaskGetTickCount();
        uint32_t elapsed_ms = App_Main_TicksToMs(now - last_run);

        last_run = now;
        App_CanBms_Task(elapsed_ms);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(APP_MAIN_CAN_BMS_TASK_PERIOD_MS));
    }
}

/**
 * @brief 串口 bring-up 命令任务。
 *
 * 只轮询 USART1 RX 并分发少量调试命令，避免把临时测试入口塞进功率或芯片驱动状态机。
 */
static void debug_cli_task(void *arg)
{
    (void)arg;

    while (1)
    {
        App_DebugCli_Task(APP_MAIN_DEBUG_CLI_TASK_PERIOD_MS);
        vTaskDelay(APP_MAIN_DEBUG_CLI_TASK_PERIOD_MS);
    }
}

/**
 * @brief 启动前的 APP/INT 层初始化。
 *
 * 统一在调度器启动前完成硬件接口初始化，可以避免多个任务同时抢 I2C/SPI/CAN
 * bring-up。功率相关模块只在 BQ/APP 无故障时释放主 FET，SC8815 默认不请求充电。
 */
static void App_Main_Init(void)
{
    printf("APP初始化: LED\r\n");
    Int_Led_Init();

    printf("APP初始化: OLED\r\n");
    App_OLED_Init();
    printf("APP初始化: SC8815\r\n");
    App_SC8815_Init();
    printf("APP初始化: 电池管理\r\n");
    App_BatMan_Init();
    printf("APP初始化: 电源管理\r\n");
    App_Power_Init();
    printf("APP初始化: CANFD协议\r\n");
    if (!App_CanBms_Init())
    {
        printf("CANFD初始化失败，任务将周期重试\r\n");
    }
    printf("APP init CLI\r\n");
    App_DebugCli_Init();
    printf("APP初始化: 完成\r\n");
}

/**
 * @brief 创建 FreeRTOS 任务并启动调度器。
 *
 * 任务表按旧项目教学风格展开写，不做额外封装，便于直接看到每个业务模块的
 * 周期和优先级。NVM/OLED 由 BatMan 任务串行调用；CAN 需要较短轮询周期，使用独立低优先级任务。
 */
void App_Main(void)
{
    App_Main_Init();

    printf("RTOS: 创建任务\r\n");
    if (xTaskCreate(batman_task, "batman_task", 768, NULL, 3, NULL) != pdPASS)
    {
        printf("RTOS: batman_task 创建失败，主功率通路保持关闭\r\n");
    }
    if (xTaskCreate(sc8815_task, "sc8815_task", 512, NULL, 2, NULL) != pdPASS)
    {
        printf("RTOS: sc8815_task 创建失败\r\n");
    }
    if (xTaskCreate(can_bms_task, "can_bms_task", 512, NULL, 2, NULL) != pdPASS)
    {
        printf("RTOS: can_bms_task 创建失败\r\n");
    }
    if (xTaskCreate(debug_cli_task, "debug_cli_task", 512, NULL, 1, NULL) != pdPASS)
    {
        printf("RTOS: debug_cli_task 创建失败\r\n");
    }

    printf("RTOS: 启动调度器\r\n");
    vTaskStartScheduler();

    while (1)
    {
    }
}
