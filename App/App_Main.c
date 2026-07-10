#include "App_Main.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>

#include "App_BatMan.h"
#include "App_DebugCli.h"
#include "App_OLED.h"
#include "App_Power.h"
#include "App_SC8815.h"
#include "Int_CanFd.h"
#include "Int_EEPROM.h"
#include "Int_Led.h"
#include "main.h"

enum
{
    APP_MAIN_BATMAN_TASK_PERIOD_MS = 1000u,
    APP_MAIN_SC8815_TASK_PERIOD_MS = 1000u,
    APP_MAIN_DEBUG_CLI_TASK_PERIOD_MS = 20u
};

/**
 * @brief BQ76952 电池监控任务。
 *
 * 周期读取电芯、电流、温度、告警状态，并立即推进功率策略。
 * BQ 数据和功率决策放在同一任务里，避免 power_task 读到半更新快照。
 */
static void batman_task(void *arg)
{
    (void)arg;

    while (1)
    {
        App_BatMan_Task(APP_MAIN_BATMAN_TASK_PERIOD_MS);
        App_Power_Task(APP_MAIN_BATMAN_TASK_PERIOD_MS);
        vTaskDelay(APP_MAIN_BATMAN_TASK_PERIOD_MS);
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
    printf("APP初始化: CANFD\r\n");
    (void)Int_CanFd_Init();
    printf("APP初始化: EEPROM\r\n");
    (void)Int_EEPROM_Init();

    printf("APP初始化: OLED\r\n");
    App_OLED_Init();
    printf("APP初始化: SC8815\r\n");
    App_SC8815_Init();
    printf("APP初始化: 电池管理\r\n");
    App_BatMan_Init();
    printf("APP初始化: 电源管理\r\n");
    App_Power_Init();
    printf("APP init CLI\r\n");
    App_DebugCli_Init();
    printf("APP初始化: 完成\r\n");
}

/**
 * @brief 创建 FreeRTOS 任务并启动调度器。
 *
 * 任务表按旧项目教学风格展开写，不做额外封装，便于直接看到每个业务模块的
 * 周期和优先级。CAN/NVM/OLED 当前只有初始化和底层接口，不创建空转任务。
 */
void App_Main(void)
{
    App_Main_Init();

    printf("RTOS: 创建任务\r\n");
    xTaskCreate(batman_task, "batman_task", 768, NULL, 3, NULL);
    xTaskCreate(sc8815_task, "sc8815_task", 512, NULL, 2, NULL);
    xTaskCreate(debug_cli_task, "debug_cli_task", 512, NULL, 1, NULL);

    printf("RTOS: 启动调度器\r\n");
    vTaskStartScheduler();

    while (1)
    {
    }
}
