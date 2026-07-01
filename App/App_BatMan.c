#include "App_BatMan.h"
#include "App_BatMan_Internal.h"

#include <stdio.h>

#include "Int_BQ76952.h"
#include "Int_BQ76952_BSP.h"
#include "main.h"

/**
 * @file App_BatMan.c
 * @brief BQ76952 电池监控 APP 层主流程门面。
 *
 * 本文件只保留初始化和周期任务的执行顺序。BQ Data Memory 配置、采样、
 * SOC/SOH 适配、均衡更新、OLED/printf 辅助分别放在 App_BatMan_xxx.c。
 */

/*
 * Bring-up 配置项。
 * 当前实板通信按 non-CRC 模式验证通过；若重新启用 CRC，需要同时确认
 * BQ 侧协议模式、读写帧格式和串口日志结果。
 */
#define APP_BATMAN_CRC_BOOT_ENABLE              (0u)
#define APP_BATMAN_BQ_RESET_SETTLE_MS           (200u)

/*
 * 对外遥测快照。
 * 保留旧项目的全局变量风格，便于 CAN、OLED、Linux 日志直接读取最新值。
 * 当前只在主循环任务中更新，不需要 volatile。若后续接入 BQ_INT/SC_INT，
 * ISR 只应置位 volatile pending flag，不应直接改这些快照字段。
 */
uint16_t cell_mv[APP_BATMAN_CELL_COUNT];
uint32_t stack_mv;
uint32_t pack_mv;
int32_t current_ma;
float current_a;
uint16_t cell_min_mv;
uint16_t cell_max_mv;
uint16_t cell_avg_mv;
uint16_t cell_delta_mv;
int16_t temp_ic_c;
int16_t temp_ts1_c;
int16_t temp_ts3_c;
int16_t temp_cell_c;
int16_t temp_fet_c;
uint16_t alarm_status;
uint16_t alarm_raw;
uint16_t battery_status;
uint16_t manufacturing_status;
uint8_t fet_status;
uint8_t fet_control_request;
uint8_t safety_alert_a;
uint8_t safety_alert_b;
uint8_t safety_alert_c;
uint8_t safety_status_a;
uint8_t safety_status_b;
uint8_t safety_status_c;
uint8_t pf_status_a;
uint8_t pf_status_b;
uint8_t pf_status_c;
uint8_t pf_status_d;
bool fault_active;
float soc_percent;
float display_soc_percent;
uint8_t soc_confidence_percent;
float soc_residual_percent;
float soc_kalman_gain;
float soc_p;
float soc_active_capacity_mah;
uint32_t charge_throughput_mah;
uint32_t discharge_throughput_mah;
uint32_t cycle_count;
uint8_t soh_percent;
uint8_t soh_confidence_percent;
uint16_t balance_mask;

/*
 * APP_BatMan 内部跨文件共享状态。
 * 这些标志只给 App_BatMan_xxx.c 使用，不放进对外头文件。
 */
bool s_comm_fault = false;
bool s_cells_sample_valid = false;
bool s_current_sample_valid = false;
bool s_temp_cell_sample_valid = false;

static uint16_t s_power_config = 0u;

static void App_BatMan_BusyDelayMs(uint32_t ms)
{
    volatile uint32_t i;
    uint32_t loop;

    for (loop = 0u; loop < ms; loop++)
    {
        for (i = 0u; i < 6400u; i++)
        {
        }
    }
}

/**
 * @brief 按 BQ76952 little-endian 格式读取 16-bit 值。
 */
uint16_t App_BatMan_ReadU16Le(const uint8_t data[2])
{
    return (uint16_t)(((uint16_t)data[1] << 8u) | data[0]);
}

/**
 * @brief 按 BQ76952 little-endian 格式写入 16-bit 值。
 */
void App_BatMan_WriteU16Le(uint16_t value, uint8_t data[2])
{
    data[0] = (uint8_t)(value & 0xFFu);
    data[1] = (uint8_t)(value >> 8u);
}

/**
 * @brief 复位 APP 层公开快照和内部估算状态。
 *
 * 即使 BQ 初始化中途失败，OLED、串口或后续 CAN 输出也能看到确定的
 * 初始值，而不是残留上一次运行的状态。
 */
static void App_BatMan_ResetState(void)
{
    uint8_t i;

    for (i = 0u; i < APP_BATMAN_CELL_COUNT; i++)
    {
        cell_mv[i] = 0u;
    }

    stack_mv = 0u;
    pack_mv = 0u;
    current_ma = 0;
    current_a = 0.0f;
    cell_min_mv = 0u;
    cell_max_mv = 0u;
    cell_avg_mv = 0u;
    cell_delta_mv = 0u;
    temp_ic_c = 0;
    temp_ts1_c = 0;
    temp_ts3_c = 0;
    temp_cell_c = 0;
    temp_fet_c = 0;
    alarm_status = 0u;
    alarm_raw = 0u;
    battery_status = 0u;
    manufacturing_status = 0u;
    fet_status = 0u;
    fet_control_request = 0u;
    safety_alert_a = 0u;
    safety_alert_b = 0u;
    safety_alert_c = 0u;
    safety_status_a = 0u;
    safety_status_b = 0u;
    safety_status_c = 0u;
    pf_status_a = 0u;
    pf_status_b = 0u;
    pf_status_c = 0u;
    pf_status_d = 0u;
    fault_active = false;
    soc_percent = APP_BATMAN_DEFAULT_SOC_PERCENT;
    display_soc_percent = APP_BATMAN_DEFAULT_SOC_PERCENT;
    soc_confidence_percent = 0u;
    soc_residual_percent = 0.0f;
    soc_kalman_gain = 0.0f;
    soc_p = 0.0f;
    soc_active_capacity_mah = (float)APP_BATMAN_CAPACITY_MAH;
    charge_throughput_mah = 0u;
    discharge_throughput_mah = 0u;
    cycle_count = 0u;
    soh_percent = 100u;
    soh_confidence_percent = 0u;
    balance_mask = BQ76952_CELL_MASK_NONE;

    s_comm_fault = false;
    App_BatMan_ResetSampleState();
    App_BatMan_ResetDebugState();
    s_power_config = 0u;
    App_BatMan_ResetEstimatorState();
}

/**
 * @brief 初始化 BQ76952 APP 层。
 *
 * 流程顺序不能随意调整：
 * 1. 先复位 APP 快照和 OLED 状态；
 * 2. 初始化板级 BQ 通信并设置 CRC 模式；
 * 3. reset BQ 并读取 Device Number 验证 I2C/协议；
 * 4. 进入 ConfigUpdate 写 Data Memory；
 * 5. 退出 ConfigUpdate 后清启动告警，并明确保持主充放电 MOS 关断；
 * 6. 采样一次，给上层提供首帧有效快照。
 */
void App_BatMan_Init(void)
{
    uint8_t data[2];
    uint16_t device_number;
    Int_BQ76952_StatusTypeDef ret;

    printf("电池管理初始化: 开始\r\n");

    /* OLED 先显示 FAIL，直到 Device Number 读取成功。 */
    App_BatMan_ResetState();
    App_BatMan_InitAlgorithms();
    App_BatMan_ShowIicStatus(false);
    printf("电池管理初始化: 状态复位完成\r\n");

    /*
     * CRC 模式必须在第一条 BQ 命令前确定；主从 CRC 设置不一致时，
     * 后续读写会表现为协议失败。
     */
    Int_BQ76952_InitBoard();
    Int_BQ76952_SetCrcEnabled(APP_BATMAN_CRC_BOOT_ENABLE != 0u);
    printf("BQ板级初始化完成 CRC:%u\r\n", APP_BATMAN_CRC_BOOT_ENABLE != 0u ? 1u : 0u);

    /*
     * reset 失败时不继续写配置，避免芯片处于未知状态时留下半配置。
     */
    printf("BQ复位: 开始\r\n");
    ret = Int_BQ76952_Reset();
    if (ret != INT_BQ76952_OK)
    {
        App_BatMan_PrintBqResetFail(ret);
        return;
    }
    printf("BQ复位: 完成\r\n");
    printf("BQ复位等待: 开始 tick:%lu primask:%lu\r\n",
           HAL_GetTick(),
           __get_PRIMASK());
    App_BatMan_BusyDelayMs(APP_BATMAN_BQ_RESET_SETTLE_MS);
    printf("BQ复位等待: 结束 tick:%lu primask:%lu\r\n",
           HAL_GetTick(),
           __get_PRIMASK());

    /*
     * Device Number 是通信链路的第一道硬确认：地址、subcommand 帧和
     * 读回长度都必须正确。
     */
    printf("BQ设备号读取: 开始\r\n");
    ret = Int_BQ76952_ReadSubcommand(BQ76952_SUBCMD_DEVICE_NUMBER, data, 2u);
    if (ret != INT_BQ76952_OK)
    {
        App_BatMan_PrintBqDeviceFail(ret);
        return;
    }

    device_number = App_BatMan_ReadU16Le(data);
    App_BatMan_PrintBqOkDev(device_number);
    App_BatMan_ShowIicStatus(true);

    /*
     * Data Memory 写入必须包在 ConfigUpdate 内。ConfigUpdate 未退出前，
     * 不做正常采样，也不执行 FET_ENABLE。
     */
    printf("BQ配置模式: 进入开始\r\n");
    if (Int_BQ76952_EnterConfigUpdate() != INT_BQ76952_OK)
    {
        App_BatMan_PrintBqCfgEnterFail();
        App_BatMan_ShowIicStatus(false);
        return;
    }
    printf("BQ配置模式: 进入完成\r\n");

    printf("BQ配置写入: 开始\r\n");
    if (!App_BatMan_ConfigBq())
    {
        App_BatMan_PrintBqCfgWriteFail();
        App_BatMan_ShowIicStatus(false);
        (void)Int_BQ76952_ExitConfigUpdate();
        return;
    }
    printf("BQ配置写入: 完成\r\n");
    /*
     * Power Config 显示在 OLED 上，用作现场快速确认 Data Memory 读链路。
     */
    printf("BQ电源配置读取: 开始\r\n");
    if (Int_BQ76952_ReadDataMemory(BQ76952_DM_POWER_CONFIG, data, 2u) == INT_BQ76952_OK)
    {
        s_power_config = App_BatMan_ReadU16Le(data);
        App_BatMan_ShowPowerConfig(true, s_power_config);
        App_BatMan_PrintBqPowerConfig(s_power_config);
    }

    printf("BQ配置模式: 退出开始\r\n");
    if (Int_BQ76952_ExitConfigUpdate() != INT_BQ76952_OK)
    {
        App_BatMan_PrintBqCfgExitFail();
        App_BatMan_ShowIicStatus(false);
        return;
    }
    printf("BQ配置模式: 退出完成\r\n");

    /*
     * Bring-up 阶段先禁止 BQ 进入 Sleep，避免 CHG FET 被低功耗策略压住。
     * 后续若要做低功耗版本，再把 Sleep 策略放回电源管理状态机统一控制。
     */
    printf("BQ Sleep禁用: 开始\r\n");
    ret = Int_BQ76952_SendSubcommand(BQ76952_SUBCMD_SLEEP_DISABLE);
    if (ret != INT_BQ76952_OK)
    {
        printf("BQ Sleep禁用失败 ret:%d hal:0x%08lx\r\n",
               (int)ret,
               (unsigned long)Int_BQ76952_GetLastHalError());
        App_BatMan_ShowIicStatus(false);
        return;
    }
    printf("BQ Sleep禁用: 完成\r\n");

    /*
     * ConfigUpdate 退出后，只清启动噪声告警，并保持 CHG/DSG/PCHG/PDSG 关断。
     * 后续如需接通主功率路径，必须增加单独的业务入口和保护条件。
     */
    printf("BQ启动告警: 清除\r\n");
    App_BatMan_ClearStartupAlarms();
    printf("BQ主FET默认关断: 开始\r\n");
    if (App_BatMan_KeepMainFetsOff() != INT_BQ76952_OK)
    {
        App_BatMan_PrintBqFetOffFail();
        App_BatMan_ShowIicStatus(false);
        return;
    }
    printf("BQ主FET默认关断: 完成\r\n");

    /*
     * 初始化成功后立即采样一次，避免 UART/OLED/CAN 首帧仍是全零快照。
     */
    printf("电池管理首帧采样: 开始\r\n");
    App_BatMan_Sample();
    App_BatMan_UpdateHealth(0u);
    App_BatMan_UpdateSoc(0u);
    App_BatMan_UpdateBalance(APP_BATMAN_BALANCE_PERIOD_MS);
    App_BatMan_UpdateRuntimeOledStatus();
    App_BatMan_PrintInitOk();
}

/**
 * @brief BQ76952 周期任务。
 *
 * 该任务只从主循环调用；若未来由 RTOS 多任务读取这些全局快照，需要
 * 引入 snapshot 或临界区，而不是简单依赖 volatile。
 */
void App_BatMan_Task(uint32_t interval_ms)
{
    /*
     * 通信故障按采样周期计算。瞬时 I2C 异常应出现在当次日志中，
     * 但不永久污染后续正常采样。
     */
    s_comm_fault = false;

    App_BatMan_Sample();
    App_BatMan_UpdateHealth(interval_ms);
    App_BatMan_UpdateSoc(interval_ms);
    App_BatMan_UpdateBalance(interval_ms);
    App_BatMan_UpdateRuntimeOledStatus();
    App_BatMan_UpdateDebugOutput(interval_ms);
}
