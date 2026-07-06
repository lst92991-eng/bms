#include "Bms_Main.h"

#include "App_Main.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>

#include "Bms_BatManager.h"
#include "Bms_BuzzerService.h"
#include "App_DebugCli.h"
#include "Bms_DebugCli.h"
#include "Bms_OledDebugView.h"
#include "Bms_PowerService.h"
#include "Bms_Sc8815Port.h"
#include "Int_Button.h"
#include "Int_Buzzer.h"
#include "Int_CanFd.h"
#include "Int_EEPROM.h"
#include "Int_Led.h"
#include "main.h"

#define APP_MAIN_BATMAN_TASK_PERIOD_MS     1000u
#define APP_MAIN_SC8815_TASK_PERIOD_MS     1000u
#define APP_MAIN_BOARD_IO_TASK_PERIOD_MS   10u
#define APP_MAIN_CAN_TASK_PERIOD_MS        100u
#define APP_MAIN_NVM_TASK_PERIOD_MS        1000u
#define APP_MAIN_DISPLAY_TASK_PERIOD_MS    1000u
#define APP_MAIN_DEBUG_CLI_TASK_PERIOD_MS  20u
#define APP_MAIN_BUZZER_ENABLE             0u
#define APP_MAIN_OLED_ENABLE               0u

/**
 * @brief 板载按键、蜂鸣器等轻�?IO 任务�?
 *
 * 这些接口内部只做短时间状态机推进，周期放短一点，保证按键消抖和蜂鸣器
 * 音符切换不被 BQ/SC �?I2C 访问节奏拖慢�?
 */
void board_io_task(void *arg)
{
    (void)arg;

    while (1)
    {
        Int_Button_Task(HAL_GetTick());
#if APP_MAIN_BUZZER_ENABLE
        Bms_BuzzerService_Task(HAL_GetTick());
#endif
        vTaskDelay(APP_MAIN_BOARD_IO_TASK_PERIOD_MS);
    }
}

/**
 * @brief CANFD APP 层任务预留�?
 *
 * 后续用于电池状态上报，以及接收 Linux/上位机控制命令。当�?INT 层已完成
 * 初始化和收发接口，这里只保留任务入口�?
 */
void can_task(void *arg)
{
    (void)arg;

    while (1)
    {
        vTaskDelay(APP_MAIN_CAN_TASK_PERIOD_MS);
    }
}

/**
 * @brief BQ76952 电池监控任务�?
 *
 * 周期读取电芯、电流、温度、告警状态，并立即推进功率策略�?
 * BQ 数据和功率决策放在同一任务里，避免 power_task 读到半更新快照�?
 */
void batman_task(void *arg)
{
    (void)arg;

    while (1)
    {
        Bms_BatManager_Task(APP_MAIN_BATMAN_TASK_PERIOD_MS);
        Bms_PowerService_Task(APP_MAIN_BATMAN_TASK_PERIOD_MS);
        vTaskDelay(APP_MAIN_BATMAN_TASK_PERIOD_MS);
    }
}

/**
 * @brief SC8815 充电芯片监控任务�?
 *
 * 只做状态读取和 charge_request 状态机推进；默认不主动请求充电，功率释放由
 * BatMan 任务中的 App_Power 策略显式决定�?
 */
void sc8815_task(void *arg)
{
    (void)arg;

    while (1)
    {
        Bms_Sc8815Port_Task(APP_MAIN_SC8815_TASK_PERIOD_MS);
        vTaskDelay(APP_MAIN_SC8815_TASK_PERIOD_MS);
    }
}

/**
 * @brief EEPROM/NVM 任务预留�?
 *
 * 后续用于掉电保持 SOC、SOH、循环次数、累计吞吐量等慢速数据�?
 */
void nvm_task(void *arg)
{
    (void)arg;

    while (1)
    {
        vTaskDelay(APP_MAIN_NVM_TASK_PERIOD_MS);
    }
}

/**
 * @brief OLED 正式页面任务预留�?
 *
 * 当前 OLED 仍由 BatMan 更新 bring-up 页；后续再把状态页刷新集中到此任务�?
 */
void display_task(void *arg)
{
    (void)arg;

    while (1)
    {
        vTaskDelay(APP_MAIN_DISPLAY_TASK_PERIOD_MS);
    }
}

/**
 * @brief 串口 bring-up 命令任务�? *
 * 只轮�?USART1 RX 并分发少量调试命令，避免把临时测试入口塞进功率或芯片驱动状态机�? */
void debug_cli_task(void *arg)
{
    (void)arg;

    while (1)
    {
        Bms_DebugCli_Task(APP_MAIN_DEBUG_CLI_TASK_PERIOD_MS);
        vTaskDelay(APP_MAIN_DEBUG_CLI_TASK_PERIOD_MS);
    }
}

/**
 * @brief 启动前的 APP/INT 层初始化�?
 *
 * 统一在调度器启动前完成硬件接口初始化，可以避免多个任务同时抢 I2C/SPI/CAN
 * bring-up。功率相关模块仍保持安全默认态：BQ �?FET 关断，SC8815 不请求充电�?
 */
static void App_Main_Init(void)
{
    printf("APP初始�? LED\r\n");
    Int_Led_Init();
#if APP_MAIN_BUZZER_ENABLE
    printf("APP初始�? 蜂鸣器底层\r\n");
    Int_Buzzer_Init();
    printf("APP初始�? 蜂鸣器应用\r\n");
    Bms_BuzzerService_Init();
#endif
    printf("APP初始�? 按键\r\n");
    Int_Button_Init();
    printf("APP初始�? CANFD\r\n");
    (void)Int_CanFd_Init();
    printf("APP初始�? EEPROM\r\n");
    (void)Int_EEPROM_Init();

#if APP_MAIN_OLED_ENABLE
    printf("APP初始�? OLED\r\n");
    Bms_OledDebugView_Init();
#else
    printf("APP初始�? OLED 跳过，充放电测试优先保证串口闭环\r\n");
#endif
    printf("APP初始�? SC8815\r\n");
    Bms_Sc8815Port_Init();
    printf("APP初始�? 电池管理\r\n");
    Bms_BatManager_Init();
    printf("APP初始�? 电源管理\r\n");
    Bms_PowerService_Init();
    printf("APP init CLI\r\n");
    Bms_DebugCli_Init();
    printf("APP初始�? 完成\r\n");
}

/**
 * @brief 创建 FreeRTOS 任务并启动调度器�?
 *
 * 任务表按旧项目教学风格展开写，不做额外封装，便于直接看到每个业务模块的
 * 周期和优先级。协议、NVM、正式显示页先保留空任务入口�?
 */
void Bms_Main(void)
{
    App_Main_Init();

    printf("RTOS: 创建任务\r\n");
    xTaskCreate(board_io_task, "board_io_task", 256, NULL, 3, NULL);
    xTaskCreate(can_task, "can_task", 512, NULL, 2, NULL);
    xTaskCreate(batman_task, "batman_task", 768, NULL, 3, NULL);
    xTaskCreate(sc8815_task, "sc8815_task", 512, NULL, 2, NULL);
    xTaskCreate(nvm_task, "nvm_task", 384, NULL, 1, NULL);
#if APP_MAIN_OLED_ENABLE
    xTaskCreate(display_task, "display_task", 384, NULL, 1, NULL);
#endif
    xTaskCreate(debug_cli_task, "debug_cli_task", 512, NULL, 1, NULL);

    printf("RTOS: 启动调度器\r\n");
    vTaskStartScheduler();

    while (1)
    {
    }
}

void App_Main(void)
{
    Bms_Main();
}