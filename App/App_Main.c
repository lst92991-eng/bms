#include "App_Main.h"

#include "FreeRTOS.h"
#include "task.h"

#include "App_BatMan.h"
#include "App_Buzzer.h"
#include "App_OLED.h"
#include "App_SC8815.h"
#include "Int_Button.h"
#include "Int_Buzzer.h"
#include "Int_CanFd.h"
#include "Int_EEPROM.h"
#include "Int_Led.h"
#include "main.h"

#define APP_MAIN_BATMAN_TASK_PERIOD_MS     1000u
#define APP_MAIN_SC8815_TASK_PERIOD_MS     1000u
#define APP_MAIN_BOARD_IO_TASK_PERIOD_MS   10u
#define APP_MAIN_POWER_TASK_PERIOD_MS      100u
#define APP_MAIN_CAN_TASK_PERIOD_MS        100u
#define APP_MAIN_NVM_TASK_PERIOD_MS        1000u
#define APP_MAIN_DISPLAY_TASK_PERIOD_MS    1000u

/**
 * @brief 板载按键、蜂鸣器等轻量 IO 任务。
 *
 * 这些接口内部只做短时间状态机推进，周期放短一点，保证按键消抖和蜂鸣器
 * 音符切换不被 BQ/SC 的 I2C 访问节奏拖慢。
 */
void board_io_task(void *arg)
{
    (void)arg;

    while (1)
    {
        Int_Button_Task(HAL_GetTick());
        Int_Buzzer_Task(HAL_GetTick());
        vTaskDelay(APP_MAIN_BOARD_IO_TASK_PERIOD_MS);
    }
}

/**
 * @brief P0 功率策略任务预留。
 *
 * 后续在这里统一决定 BQ 主充放电 FET、SC8815 充电请求、故障关断和恢复条件。
 * 当前阶段保持空任务，避免在 P0 规则未确认前引入隐含功率动作。
 */
void power_task(void *arg)
{
    (void)arg;

    while (1)
    {
        vTaskDelay(APP_MAIN_POWER_TASK_PERIOD_MS);
    }
}

/**
 * @brief CANFD APP 层任务预留。
 *
 * 后续用于电池状态上报，以及接收 Linux/上位机控制命令。当前 INT 层已完成
 * 初始化和收发接口，这里只保留任务入口。
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
 * @brief BQ76952 电池监控任务。
 *
 * 周期读取电芯、电流、温度、告警状态，并推进 SOC/SOH、MCU 均衡和调试输出。
 */
void batman_task(void *arg)
{
    (void)arg;

    while (1)
    {
        App_BatMan_Task(APP_MAIN_BATMAN_TASK_PERIOD_MS);
        vTaskDelay(APP_MAIN_BATMAN_TASK_PERIOD_MS);
    }
}

/**
 * @brief SC8815 充电芯片监控任务。
 *
 * 只做状态读取和 charge_request 状态机推进；默认不主动请求充电，功率释放由
 * 后续 P0 任务显式决定。
 */
void sc8815_task(void *arg)
{
    (void)arg;

    while (1)
    {
        App_SC8815_Task(APP_MAIN_SC8815_TASK_PERIOD_MS);
        vTaskDelay(APP_MAIN_SC8815_TASK_PERIOD_MS);
    }
}

/**
 * @brief EEPROM/NVM 任务预留。
 *
 * 后续用于掉电保持 SOC、SOH、循环次数、累计吞吐量等慢速数据。
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
 * @brief OLED 正式页面任务预留。
 *
 * 当前 OLED 仍由 BatMan 更新 bring-up 页；后续再把状态页刷新集中到此任务。
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
 * @brief 启动前的 APP/INT 层初始化。
 *
 * 统一在调度器启动前完成硬件接口初始化，可以避免多个任务同时抢 I2C/SPI/CAN
 * bring-up。功率相关模块仍保持安全默认态：BQ 主 FET 关断，SC8815 不请求充电。
 */
static void App_Main_Init(void)
{
    Int_Led_Init();
    Int_Buzzer_Init();
    App_Buzzer_Init();
    Int_Button_Init();
    (void)Int_CanFd_Init();
    (void)Int_EEPROM_Init();

    App_OLED_Init();
    App_SC8815_Init();
    App_BatMan_Init();
}

/**
 * @brief 创建 FreeRTOS 任务并启动调度器。
 *
 * 任务表按旧项目教学风格展开写，不做额外封装，便于直接看到每个业务模块的
 * 周期和优先级。P0 未完成的 APP 先保留空任务入口。
 */
void App_Main(void)
{
    App_Main_Init();

    xTaskCreate(board_io_task, "board_io_task", 256, NULL, 3, NULL);
    xTaskCreate(power_task, "power_task", 512, NULL, 3, NULL);
    xTaskCreate(can_task, "can_task", 512, NULL, 2, NULL);
    xTaskCreate(batman_task, "batman_task", 512, NULL, 2, NULL);
    xTaskCreate(sc8815_task, "sc8815_task", 512, NULL, 2, NULL);
    xTaskCreate(nvm_task, "nvm_task", 384, NULL, 1, NULL);
    xTaskCreate(display_task, "display_task", 384, NULL, 1, NULL);

    vTaskStartScheduler();

    while (1)
    {
    }
}
