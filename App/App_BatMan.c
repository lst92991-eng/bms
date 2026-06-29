#include "App_BatMan.h"

#include <stdio.h>

#include "App_OLED.h"
#include "Com_BQ76952.h"
#include "Com_SOC.h"
#include "Com_SOH.h"
#include "Int_BQ76952.h"
#include "Int_BQ76952_BSP.h"
#include "main.h"

/**
 * @file App_BatMan.c
 * @brief BQ76952 电池监控 APP 层。
 *
 * 本文件只做“初始化 + 监控 + 估算”，不再作为功率策略执行器。
 * BQ76952 在 `FET_ENABLE` 后负责 CHG/DSG MOS 的保护与开关判定；
 * MCU 只写入项目确认过的 Data Memory 基线配置，随后周期采样电压、
 * 电流、温度、告警、FET 状态，并给上层日志/OLED 提供 SOC/SOH 估算。
 *
 * 维护边界：
 * - 不在 APP 层直接写 FET 控制命令强制开关 MOS。
 * - 不在 APP 层计算并写入 host cell-balance mask。
 * - `fault_active` 只用于监控和日志，不替代 BQ 的硬件保护动作。
 */

/*
 * Bring-up 配置项。
 * 当前实板通信按 non-CRC 模式验证通过；若重新启用 CRC，需要同时确认
 * BQ 侧协议模式、读写帧格式和串口日志结果。
 */
#define APP_BATMAN_CRC_BOOT_ENABLE              (0u)
#define APP_BATMAN_BQ_RESET_SETTLE_MS           (200u)

/*
 * 显示 SOC 滤波系数。
 * 充电响应略快，放电响应略慢，静置时只轻微引入 OCV 校正，避免显示跳变。
 */
#define APP_BATMAN_SOC_REST_NEED_MS             (600000u)
#define APP_BATMAN_SOC_CURRENT_DEADBAND_MA      (30)
#define APP_BATMAN_SOC_REST_CURRENT_MA          (100)
#define APP_BATMAN_SOC_REST_STABLE_MV           (3u)
#define APP_BATMAN_SOC_R0_OHM                   (0.012f)
#define APP_BATMAN_SOC_R1_OHM                   (0.006f)
#define APP_BATMAN_SOC_C1_F                     (8000.0f)
#define APP_BATMAN_SOC_R2_OHM                   (0.003f)
#define APP_BATMAN_SOC_C2_F                     (60000.0f)
#define APP_BATMAN_SOC_PROCESS_Q_SOC            (0.00000001f)
#define APP_BATMAN_SOC_PROCESS_Q_V1             (0.0000001f)
#define APP_BATMAN_SOC_PROCESS_Q_V2             (0.0000001f)
#define APP_BATMAN_SOC_MEASURE_R_MV             (25.0f)
#define APP_BATMAN_SOC_MEASURE_R_DYN_MV         (80.0f)
#define APP_BATMAN_SOC_RESIDUAL_REJECT_MV       (300.0f)
#define APP_BATMAN_SOC_DYN_CURR_DELTA_MA        (300)
#define APP_BATMAN_SOC_DYN_CURR_STABLE_MS       (5000u)
#define APP_BATMAN_SOC_FULL_CURRENT_MA          (250)
#define APP_BATMAN_SOC_EMPTY_CURRENT_MA         (250)
#define APP_BATMAN_SOC_FULL_ANCHOR_HOLD_MS      (5000u)
#define APP_BATMAN_SOC_EMPTY_ANCHOR_HOLD_MS     (5000u)
#define APP_BATMAN_SOC_DISPLAY_RISE_PER_S       (1.5f)
#define APP_BATMAN_SOC_DISPLAY_FALL_PER_S       (2.5f)
#define APP_BATMAN_SOC_DYNAMIC_EKF_ENABLE       (0u)

/*
 * 早期 SOH 提醒因子。
 * 这不是最终老化模型，只给 Linux 记录健康趋势和异常提醒使用。
 */
#define APP_BATMAN_HEALTH_DELTA_WARN_MV         (80u)
#define APP_BATMAN_HEALTH_TEMP_WARN_C           (55)

/*
 * 项目 Data Memory 基线值。
 * 这里只保留已经和硬件/bring-up 目标绑定的最小集合：6S 映射、FET 托管、
 * 告警掩码、保护路由和均衡模式。最终阈值必须来自实验标定，不在 APP
 * 层凭经验补齐。
 */
#define APP_BATMAN_DM_DA_CONFIGURATION_DEFAULT          (0x05u)
#define APP_BATMAN_DM_PROTECTION_CONFIGURATION_DEFAULT  (0x0002u)
#define APP_BATMAN_DM_ENABLED_PROTECTIONS_A_DEFAULT     (0x88u)
#define APP_BATMAN_DM_ENABLED_PROTECTIONS_B_DEFAULT     (0x00u)
#define APP_BATMAN_DM_ENABLED_PROTECTIONS_C_DEFAULT     (0x00u)
#define APP_BATMAN_DM_CHG_FET_PROTECTIONS_A_DEFAULT     (0x98u)
#define APP_BATMAN_DM_CHG_FET_PROTECTIONS_B_DEFAULT     (0xD5u)
#define APP_BATMAN_DM_CHG_FET_PROTECTIONS_C_DEFAULT     (0x56u)
#define APP_BATMAN_DM_DSG_FET_PROTECTIONS_A_DEFAULT     (0xE4u)
#define APP_BATMAN_DM_DSG_FET_PROTECTIONS_B_DEFAULT     (0xE6u)
#define APP_BATMAN_DM_DSG_FET_PROTECTIONS_C_DEFAULT     (0xE2u)
#define APP_BATMAN_DM_DEFAULT_ALARM_MASK_DEFAULT        (0xF800u)
#define APP_BATMAN_DM_FET_OPTIONS_DEFAULT               (BQ76952_FET_OPTIONS_FET_CTRL_EN_MASK | \
                                                         BQ76952_FET_OPTIONS_SFET_MASK)
#define APP_BATMAN_DM_CHG_PUMP_CONTROL_DEFAULT          (0x01u)
#define APP_BATMAN_DM_BALANCING_CONFIGURATION_DEFAULT   (BQ76952_BALANCING_CB_CHG_MASK | \
                                                         BQ76952_BALANCING_CB_NOSLEEP_MASK)

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
uint8_t fet_status;
uint8_t safety_status_a;
uint8_t safety_status_b;
uint8_t safety_status_c;
bool fault_active;
float soc_percent;
float display_soc_percent;
uint8_t soc_confidence_percent;
float soc_residual_mv;
float soc_kalman_gain;
float soc_p;
float soc_active_capacity_mah;
uint32_t charge_throughput_mah;
uint32_t discharge_throughput_mah;
uint32_t cycle_count;
uint8_t soh_percent;
uint8_t soh_confidence_percent;

/*
 * 文件内运行状态。
 * 这些字段不对外暴露，避免上层绕过 APP 接口修改 SOC/SOH 或通信状态。
 */
static bool s_comm_fault = false;
static bool s_cells_sample_valid = false;
static bool s_current_sample_valid = false;
static bool s_temp_ts1_sample_valid = false;
static bool s_temp_ts3_sample_valid = false;
static bool s_temp_cell_sample_valid = false;
static uint32_t s_debug_ms = 0u;
static uint16_t s_power_config = 0u;

/**
 * @brief 按 BQ76952 little-endian 格式读取 16-bit 值。
 */
static uint16_t App_BatMan_ReadU16Le(const uint8_t data[2])
{
    return (uint16_t)(((uint16_t)data[1] << 8u) | data[0]);
}

/**
 * @brief 按 BQ76952 little-endian 格式写入 16-bit 值。
 */
static void App_BatMan_WriteU16Le(uint16_t value, uint8_t data[2])
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
    fet_status = 0u;
    safety_status_a = 0u;
    safety_status_b = 0u;
    safety_status_c = 0u;
    fault_active = false;
    soc_percent = APP_BATMAN_DEFAULT_SOC_PERCENT;
    display_soc_percent = APP_BATMAN_DEFAULT_SOC_PERCENT;
    soc_confidence_percent = 0u;
    soc_residual_mv = 0.0f;
    soc_kalman_gain = 0.0f;
    soc_p = 0.0f;
    soc_active_capacity_mah = (float)APP_BATMAN_CAPACITY_MAH;
    charge_throughput_mah = 0u;
    discharge_throughput_mah = 0u;
    cycle_count = 0u;
    soh_percent = 100u;
    soh_confidence_percent = 0u;

    s_comm_fault = false;
    s_cells_sample_valid = false;
    s_current_sample_valid = false;
    s_temp_ts1_sample_valid = false;
    s_temp_ts3_sample_valid = false;
    s_temp_cell_sample_valid = false;
    s_debug_ms = 0u;
    s_power_config = 0u;
}

/**
 * @brief 写入 1 字节 Data Memory 配置。
 *
 * APP 层只表达业务配置项，I2C、Data Memory 校验和及时序细节由 INT 层负责。
 */
static bool App_BatMan_WriteConfigU8(uint16_t address, uint8_t value)
{
    if (Int_BQ76952_WriteDataMemory(address, &value, 1u) == INT_BQ76952_OK)
    {
        return true;
    }

    s_comm_fault = true;
    printf("bq dm write8 fail addr:0x%04x\r\n", (unsigned int)address);
    return false;
}

/**
 * @brief 写入 2 字节 Data Memory 配置。
 */
static bool App_BatMan_WriteConfigU16(uint16_t address, uint16_t value)
{
    uint8_t data[2];

    App_BatMan_WriteU16Le(value, data);
    if (Int_BQ76952_WriteDataMemory(address, data, 2u) == INT_BQ76952_OK)
    {
        return true;
    }

    s_comm_fault = true;
    printf("bq dm write16 fail addr:0x%04x\r\n", (unsigned int)address);
    return false;
}

/**
 * @brief 写入 BQ76952 bring-up 阶段的项目基线配置。
 *
 * 与旧 APP 的关键区别：这里配置 BQ 的 FET/保护/均衡能力，但不再由
 * APP 选择 MOS 状态或指定某一串均衡。函数保持短而直，是为了让每个
 * Data Memory 写入在审阅时都能被快速定位。
 */
static bool App_BatMan_ConfigBq(void)
{
    /*
     * 真实 6S 采样并不是连续接在 BQ Cell1..Cell6：
     * 物理 cell0..5 对应 BQ Cell1/2/6/9/12/16。
     */
    if (!App_BatMan_WriteConfigU8(BQ76952_DM_DA_CONFIGURATION,
                                  APP_BATMAN_DM_DA_CONFIGURATION_DEFAULT) ||
        !App_BatMan_WriteConfigU16(BQ76952_DM_VCELL_MODE,
                                   BQ76952_CELL_MASK_6S_HW_CONFIRMED))
    {
        return false;
    }

    /*
     * 保护配置与告警掩码决定 BQ 上报哪些事件，以及哪些事件进入
     * BQ 自己的保护/FET 判定路径。
     */
    if (!App_BatMan_WriteConfigU16(BQ76952_DM_PROTECTION_CONFIGURATION,
                                   APP_BATMAN_DM_PROTECTION_CONFIGURATION_DEFAULT) ||
        !App_BatMan_WriteConfigU16(BQ76952_DM_DEFAULT_ALARM_MASK,
                                   APP_BATMAN_DM_DEFAULT_ALARM_MASK_DEFAULT))
    {
        return false;
    }

    /*
     * FET_CTRL_EN 是 FET 托管的核心开关。APP 后续只发送一次
     * FET_ENABLE，不再强制 CHG/DSG MOS 状态。
     */
    if (!App_BatMan_WriteConfigU8(BQ76952_DM_FET_OPTIONS,
                                  APP_BATMAN_DM_FET_OPTIONS_DEFAULT) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_CHG_PUMP_CONTROL,
                                  APP_BATMAN_DM_CHG_PUMP_CONTROL_DEFAULT))
    {
        return false;
    }

    /*
     * 均衡不再由 host mask 驱动。APP 不写 CB_ACTIVE_CELLS，只保留
     * BQ 自主管理所需的项目模式配置。
     */
    if (!App_BatMan_WriteConfigU8(BQ76952_DM_BALANCING_CONFIGURATION,
                                  APP_BATMAN_DM_BALANCING_CONFIGURATION_DEFAULT))
    {
        return false;
    }

    /*
     * FET protection routing 决定哪些安全事件会关断 CHG/DSG。
     * 这些值和 FET_OPTIONS 放在同一段，便于上板前整体审查 FET 托管。
     */
    if (!App_BatMan_WriteConfigU8(BQ76952_DM_ENABLED_PROTECTIONS_A,
                                  APP_BATMAN_DM_ENABLED_PROTECTIONS_A_DEFAULT) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_ENABLED_PROTECTIONS_B,
                                  APP_BATMAN_DM_ENABLED_PROTECTIONS_B_DEFAULT) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_ENABLED_PROTECTIONS_C,
                                  APP_BATMAN_DM_ENABLED_PROTECTIONS_C_DEFAULT) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_CHG_FET_PROTECTIONS_A,
                                  APP_BATMAN_DM_CHG_FET_PROTECTIONS_A_DEFAULT) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_CHG_FET_PROTECTIONS_B,
                                  APP_BATMAN_DM_CHG_FET_PROTECTIONS_B_DEFAULT) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_CHG_FET_PROTECTIONS_C,
                                  APP_BATMAN_DM_CHG_FET_PROTECTIONS_C_DEFAULT) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_DSG_FET_PROTECTIONS_A,
                                  APP_BATMAN_DM_DSG_FET_PROTECTIONS_A_DEFAULT) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_DSG_FET_PROTECTIONS_B,
                                  APP_BATMAN_DM_DSG_FET_PROTECTIONS_B_DEFAULT) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_DSG_FET_PROTECTIONS_C,
                                  APP_BATMAN_DM_DSG_FET_PROTECTIONS_C_DEFAULT))
    {
        return false;
    }

    return true;
}

/**
 * @brief 确保 BQ 的 FET 控制状态机已允许运行。
 *
 * `FET_ENABLE` 是允许 BQ FET 逻辑工作的 manufacturing subcommand，
 * 不是强制 MOS 导通命令。执行后 BQ 仍会根据 safety/FET status
 * 自己决定 CHG/DSG 是否打开。
 */
static Int_BQ76952_StatusTypeDef App_BatMan_EnableBqFetControl(void)
{
    uint8_t data[2];
    uint16_t mfg_status;
    Int_BQ76952_StatusTypeDef ret;

    ret = Int_BQ76952_ReadSubcommand(BQ76952_SUBCMD_MANUFACTURING_STATUS, data, 2u);
    if (ret != INT_BQ76952_OK)
    {
        return ret;
    }

    mfg_status = App_BatMan_ReadU16Le(data);
    if ((mfg_status & BQ76952_MFG_STATUS_FET_EN_MASK) != 0u)
    {
        return INT_BQ76952_OK;
    }

    return Int_BQ76952_SendSubcommand(BQ76952_SUBCMD_FET_ENABLE);
}

/**
 * @brief 清除启动阶段预期出现的告警位。
 *
 * INIT/SCAN/WAKE 类告警在启动中很常见，只清这些位可以降低串口噪声，
 * 同时避免掩盖真正的 safety status。
 */
static void App_BatMan_ClearStartupAlarms(void)
{
    uint8_t data[2];
    const uint16_t mask = BQ76952_ALARM_INITSTART_MASK |
                          BQ76952_ALARM_INITCOMP_MASK |
                          BQ76952_ALARM_FULLSCAN_MASK |
                          BQ76952_ALARM_ADSCAN_MASK |
                          BQ76952_ALARM_WAKE_MASK;

    App_BatMan_WriteU16Le(mask, data);
    (void)Int_BQ76952_WriteDirect(BQ76952_CMD_ALARM_STATUS, data, 2u);
}

/**
 * @brief 读取 BQ direct command 的 1 字节数据。
 *
 * 单次失败只标记当前采样周期通信异常，下一次任务周期会重新尝试。
 */
static bool App_BatMan_ReadDirectU8(uint8_t command, uint8_t *value)
{
    if (Int_BQ76952_ReadDirect(command, value, 1u) == INT_BQ76952_OK)
    {
        return true;
    }

    s_comm_fault = true;
    return false;
}

/**
 * @brief 读取 BQ direct command 的 2 字节 little-endian 数据。
 */
static bool App_BatMan_ReadDirectU16(uint8_t command, uint16_t *value)
{
    uint8_t data[2];

    if (Int_BQ76952_ReadDirect(command, data, 2u) == INT_BQ76952_OK)
    {
        *value = App_BatMan_ReadU16Le(data);
        return true;
    }

    s_comm_fault = true;
    return false;
}

/**
 * @brief 判断温度值是否适合作为 APP 层温度输入。
 *
 * TS 通道未接或未正确配置时可能读到极端值。这里过滤掉明显异常值，
 * 后续用 IC 温度兜底，避免 SOC/SOH 或日志出现 -273C 一类假数据。
 */
static bool App_BatMan_IsTempValid(int16_t temp_c)
{
    return (temp_c >= -40) && (temp_c <= 100);
}

/**
 * @brief 初始化 SOC/SOH 纯算法模块。
 *
 * 参数仍放在 APP 层集中表达，便于现场按电芯和热敏实测结果调参。
 * COM 层只保存算法状态，不知道 BQ、SC 或 OLED 的存在。
 */
static void App_BatMan_InitAlgorithms(void)
{
    Com_SOC_ConfigTypeDef soc_config = {0};
    Com_SOH_ConfigTypeDef soh_config = {0};

    soc_config.capacity_mah = APP_BATMAN_CAPACITY_MAH;
    soc_config.default_soc_percent = APP_BATMAN_DEFAULT_SOC_PERCENT;
    soc_config.current_sign = COM_SOC_CURRENT_POS_CHARGE;
    soc_config.current_deadband_ma = APP_BATMAN_SOC_CURRENT_DEADBAND_MA;
    soc_config.rest_need_ms = APP_BATMAN_SOC_REST_NEED_MS;
    soc_config.rest_current_ma = APP_BATMAN_SOC_REST_CURRENT_MA;
    soc_config.rest_voltage_stable_mv = APP_BATMAN_SOC_REST_STABLE_MV;
    soc_config.r0_ohm = APP_BATMAN_SOC_R0_OHM;
    soc_config.r1_ohm = APP_BATMAN_SOC_R1_OHM;
    soc_config.c1_f = APP_BATMAN_SOC_C1_F;
    soc_config.r2_ohm = APP_BATMAN_SOC_R2_OHM;
    soc_config.c2_f = APP_BATMAN_SOC_C2_F;
    soc_config.process_q_soc = APP_BATMAN_SOC_PROCESS_Q_SOC;
    soc_config.process_q_v = APP_BATMAN_SOC_PROCESS_Q_V1;
    soc_config.process_q_v1 = APP_BATMAN_SOC_PROCESS_Q_V1;
    soc_config.process_q_v2 = APP_BATMAN_SOC_PROCESS_Q_V2;
    soc_config.measure_r_mv = APP_BATMAN_SOC_MEASURE_R_MV;
    soc_config.dynamic_ekf_enable = (APP_BATMAN_SOC_DYNAMIC_EKF_ENABLE != 0u);
    soc_config.measure_r_dynamic_mv = APP_BATMAN_SOC_MEASURE_R_DYN_MV;
    soc_config.residual_reject_mv = APP_BATMAN_SOC_RESIDUAL_REJECT_MV;
    soc_config.dynamic_current_delta_limit_ma = APP_BATMAN_SOC_DYN_CURR_DELTA_MA;
    soc_config.dynamic_current_stable_ms = APP_BATMAN_SOC_DYN_CURR_STABLE_MS;
    soc_config.full_cell_mv = APP_BATMAN_CELL_FULL_MV;
    soc_config.empty_cell_mv = 3000u;
    soc_config.full_current_ma = APP_BATMAN_SOC_FULL_CURRENT_MA;
    soc_config.empty_current_ma = APP_BATMAN_SOC_EMPTY_CURRENT_MA;
    soc_config.full_anchor_hold_ms = APP_BATMAN_SOC_FULL_ANCHOR_HOLD_MS;
    soc_config.empty_anchor_hold_ms = APP_BATMAN_SOC_EMPTY_ANCHOR_HOLD_MS;
    soc_config.display_rise_percent_per_s = APP_BATMAN_SOC_DISPLAY_RISE_PER_S;
    soc_config.display_fall_percent_per_s = APP_BATMAN_SOC_DISPLAY_FALL_PER_S;
    Com_SOC_Init(&soc_config);

    soh_config.capacity_mah = APP_BATMAN_CAPACITY_MAH;
    soh_config.delta_warn_mv = APP_BATMAN_HEALTH_DELTA_WARN_MV;
    soh_config.temp_warn_c = APP_BATMAN_HEALTH_TEMP_WARN_C;
    soh_config.cycle_warn_count = 300u;
    Com_SOH_Init(&soh_config);
}

/**
 * @brief 读取 6 串实际电芯电压并计算 min/max/avg/delta。
 *
 * 命令表体现了本硬件的稀疏采样映射；维护时不要按连续 Cell1..Cell6
 * 直觉改写。
 */
static void App_BatMan_LoadCellsVoltage(void)
{
    static const uint8_t commands[APP_BATMAN_CELL_COUNT] =
    {
        BQ76952_CMD_CELL1_VOLTAGE,
        BQ76952_CMD_CELL2_VOLTAGE,
        BQ76952_CMD_CELL6_VOLTAGE,
        BQ76952_CMD_CELL9_VOLTAGE,
        BQ76952_CMD_CELL12_VOLTAGE,
        BQ76952_CMD_CELL16_VOLTAGE
    };
    uint8_t i;
    uint16_t mv;
    uint32_t sum_mv = 0u;
    uint8_t valid_count = 0u;

    s_cells_sample_valid = false;
    cell_min_mv = 0xFFFFu;
    cell_max_mv = 0u;

    for (i = 0u; i < APP_BATMAN_CELL_COUNT; i++)
    {
        if (!App_BatMan_ReadDirectU16(commands[i], &mv))
        {
            cell_mv[i] = 0u;
            continue;
        }

        cell_mv[i] = mv;
        sum_mv += mv;
        valid_count++;
        cell_min_mv = (mv < cell_min_mv) ? mv : cell_min_mv;
        cell_max_mv = (mv > cell_max_mv) ? mv : cell_max_mv;
    }

    if (cell_min_mv == 0xFFFFu)
    {
        cell_min_mv = 0u;
    }

    if (valid_count == APP_BATMAN_CELL_COUNT)
    {
        cell_avg_mv = (uint16_t)(sum_mv / valid_count);
        s_cells_sample_valid = true;
    }
    else
    {
        cell_avg_mv = 0u;
    }
    cell_delta_mv = (uint16_t)(cell_max_mv - cell_min_mv);
}

/**
 * @brief 读取电池总压快照。
 *
 * 当前 `pack_mv` 暂时镜像 `stack_mv`。若后续需要 PACK/LD pin 电压，
 * 应新增独立字段，避免复用 `stack_mv` 造成语义混淆。
 */
static void App_BatMan_LoadBatVoltage(void)
{
    uint16_t raw;

    if (App_BatMan_ReadDirectU16(BQ76952_CMD_STACK_VOLTAGE, &raw))
    {
        stack_mv = raw;
        pack_mv = stack_mv;
    }
}

/**
 * @brief 读取 CC2 电流并转换为 APP 约定方向。
 *
 * 当前约定：`current_ma > 0` 表示充电，`current_ma < 0` 表示放电。
 * 若实测方向相反，只改 `APP_BATMAN_CC2_RAW_POLARITY`。
 */
static void App_BatMan_LoadCurrent(void)
{
    uint16_t raw_u16;

    s_current_sample_valid = false;
    if (App_BatMan_ReadDirectU16(BQ76952_CMD_CC2_CURRENT, &raw_u16))
    {
        current_ma = (int32_t)((int16_t)raw_u16) * APP_BATMAN_CC2_RAW_POLARITY;
        current_a = (float)current_ma / 1000.0f;
        s_current_sample_valid = true;
    }
}

/**
 * @brief 读取 IC/TS 温度并生成 APP 层温度快照。
 *
 * TS1/TS3 有效时用于估算 cell temperature；无效时降级为 IC 温度，
 * 这样 SOH 和日志仍保持可读。
 */
static void App_BatMan_LoadTemperature(void)
{
    uint16_t raw;
    int16_t temp;

    s_temp_ts1_sample_valid = false;
    s_temp_ts3_sample_valid = false;
    s_temp_cell_sample_valid = false;

    if (App_BatMan_ReadDirectU16(BQ76952_CMD_INT_TEMPERATURE, &raw))
    {
        temp_ic_c = Com_BQ76952_Temp0p1KToC((int16_t)raw);
    }
    if (App_BatMan_ReadDirectU16(BQ76952_CMD_TS1_TEMPERATURE, &raw))
    {
        temp = Com_BQ76952_Temp0p1KToC((int16_t)raw);
        if (App_BatMan_IsTempValid(temp))
        {
            temp_ts1_c = temp;
            s_temp_ts1_sample_valid = true;
        }
        else
        {
            temp_ts1_c = temp_ic_c;
        }
    }
    if (App_BatMan_ReadDirectU16(BQ76952_CMD_TS3_TEMPERATURE, &raw))
    {
        temp = Com_BQ76952_Temp0p1KToC((int16_t)raw);
        if (App_BatMan_IsTempValid(temp))
        {
            temp_ts3_c = temp;
            s_temp_ts3_sample_valid = true;
        }
        else
        {
            temp_ts3_c = temp_ic_c;
        }
    }

    if (s_temp_ts1_sample_valid && s_temp_ts3_sample_valid)
    {
        temp_cell_c = (int16_t)((temp_ts1_c + temp_ts3_c) / 2);
        s_temp_cell_sample_valid = true;
    }
    else if (s_temp_ts1_sample_valid)
    {
        temp_cell_c = temp_ts1_c;
        s_temp_cell_sample_valid = true;
    }
    else if (s_temp_ts3_sample_valid)
    {
        temp_cell_c = temp_ts3_c;
        s_temp_cell_sample_valid = true;
    }
    else
    {
        temp_cell_c = temp_ic_c;
    }
    temp_fet_c = temp_ic_c;
}

/**
 * @brief 读取 BQ 告警、FET 和 safety 状态。
 *
 * 这些字段用于解释 BQ 为什么打开或关闭 MOS。APP 只记录状态，
 * 不覆盖 BQ 的保护决策。
 */
static void App_BatMan_LoadBqStatus(void)
{
    uint16_t value16;
    uint8_t value8;

    if (App_BatMan_ReadDirectU16(BQ76952_CMD_ALARM_STATUS, &value16))
    {
        alarm_status = value16;
    }
    if (App_BatMan_ReadDirectU16(BQ76952_CMD_ALARM_RAW_STATUS, &value16))
    {
        alarm_raw = value16;
    }
    if (App_BatMan_ReadDirectU16(BQ76952_CMD_BATTERY_STATUS, &value16))
    {
        battery_status = value16;
    }
    if (App_BatMan_ReadDirectU8(BQ76952_CMD_FET_STATUS, &value8))
    {
        fet_status = value8;
    }
    if (App_BatMan_ReadDirectU8(BQ76952_CMD_SAFETY_STATUS_A, &value8))
    {
        safety_status_a = value8;
    }
    if (App_BatMan_ReadDirectU8(BQ76952_CMD_SAFETY_STATUS_B, &value8))
    {
        safety_status_b = value8;
    }
    if (App_BatMan_ReadDirectU8(BQ76952_CMD_SAFETY_STATUS_C, &value8))
    {
        safety_status_c = value8;
    }
}

/**
 * @brief 更新 APP 层故障摘要。
 *
 * 这是显示/日志用的软件摘要，不是硬件保护动作。通信失败、明显异常
 * 电芯电压或 safety status 非零都会让 `fault_active` 置位。
 */
static void App_BatMan_UpdateFaultState(void)
{
    uint8_t i;
    bool cell_fault = false;
    bool safety_fault;

    for (i = 0u; i < APP_BATMAN_CELL_COUNT; i++)
    {
        if ((cell_mv[i] < APP_BATMAN_CELL_VALID_MIN_MV) ||
            (cell_mv[i] > APP_BATMAN_CELL_VALID_MAX_MV))
        {
            cell_fault = true;
        }
    }

    safety_fault = ((safety_status_a != 0u) ||
                    (safety_status_b != 0u) ||
                    (safety_status_c != 0u));
    fault_active = s_comm_fault || cell_fault || safety_fault;
}

/**
 * @brief 用平均单体电压给 SOC 初值播种。
 *
 * OCV 估算只有在接近静置时最可靠，但比固定 50% 更适合作为开机初值。
 */
static void App_BatMan_SeedSoc(void)
{
    Com_SOC_SampleTypeDef sample;
    Com_SOC_ResultTypeDef result;

    if (!s_cells_sample_valid || (cell_avg_mv == 0u))
    {
        return;
    }

    sample.interval_ms = 0u;
    sample.current_valid = false;
    sample.current_ma = 0;
    sample.cells_valid = true;
    sample.cell_min_mv = cell_min_mv;
    sample.cell_max_mv = cell_max_mv;
    sample.cell_avg_mv = cell_avg_mv;
    sample.temp_valid = s_temp_cell_sample_valid;
    sample.temp_c = temp_cell_c;
    sample.soh_valid = (soh_confidence_percent >= 50u);
    sample.soh_percent = soh_percent;
    Com_SOC_Update(&sample);

    Com_SOC_GetResult(&result);
    soc_percent = result.soc_percent;
    display_soc_percent = result.display_percent;
    soc_confidence_percent = result.confidence_percent;
    soc_residual_mv = result.residual_mv;
    soc_kalman_gain = result.kalman_gain_soc;
    soc_p = result.p_soc;
    soc_active_capacity_mah = result.active_capacity_mah;
}

/**
 * @brief 更新轻量 SOC 估算。
 *
 * 设计意图：
 * - 用 CC2 电流做库仑积分；
 * - 低电流时用 OCV 表轻微拉回；
 * - 显示值做滤波，避免 OLED/Linux 看到跳变。
 */
static void App_BatMan_UpdateSoc(uint32_t interval_ms)
{
    Com_SOC_SampleTypeDef sample;
    Com_SOC_ResultTypeDef result;

    sample.interval_ms = interval_ms;
    sample.current_valid = s_current_sample_valid;
    sample.current_ma = current_ma;
    sample.cells_valid = s_cells_sample_valid;
    sample.cell_min_mv = cell_min_mv;
    sample.cell_max_mv = cell_max_mv;
    sample.cell_avg_mv = cell_avg_mv;
    sample.temp_valid = s_temp_cell_sample_valid;
    sample.temp_c = temp_cell_c;
    sample.soh_valid = (soh_confidence_percent >= 50u);
    sample.soh_percent = soh_percent;
    Com_SOC_Update(&sample);

    Com_SOC_GetResult(&result);
    soc_percent = result.soc_percent;
    display_soc_percent = result.display_percent;
    soc_confidence_percent = result.confidence_percent;
    soc_residual_mv = result.residual_mv;
    soc_kalman_gain = result.kalman_gain_soc;
    soc_p = result.p_soc;
    soc_active_capacity_mah = result.active_capacity_mah;
}

/**
 * @brief 更新健康趋势计数。
 *
 * 这不是标定过的 SOH 算法，只记录可长期观察的压力指标：吞吐量、
 * 等效循环、最大压差、最高温度和 safety fault 次数。
 */
static void App_BatMan_UpdateHealth(uint32_t interval_ms)
{
    Com_SOH_SampleTypeDef sample;
    Com_SOH_ResultTypeDef result;

    sample.interval_ms = interval_ms;
    sample.current_valid = s_current_sample_valid;
    sample.current_ma = current_ma;
    sample.cells_valid = s_cells_sample_valid;
    sample.cell_delta_mv = cell_delta_mv;
    sample.temp_cell_valid = s_temp_cell_sample_valid;
    sample.temp_cell_c = temp_cell_c;
    sample.safety_status_a = safety_status_a;
    sample.safety_status_b = safety_status_b;
    sample.safety_status_c = safety_status_c;
    Com_SOH_Update(&sample);

    Com_SOH_GetResult(&result);
    charge_throughput_mah = result.charge_throughput_mah;
    discharge_throughput_mah = result.discharge_throughput_mah;
    cycle_count = result.cycle_count;
    soh_percent = result.soh_percent;
    soh_confidence_percent = result.confidence_percent;
}

/**
 * @brief 执行一次完整 BQ 采样。
 *
 * 固定采样顺序便于对比不同固件版本的串口日志。
 */
static void App_BatMan_Sample(void)
{
    App_BatMan_LoadCellsVoltage();
    App_BatMan_LoadBatVoltage();
    App_BatMan_LoadCurrent();
    App_BatMan_LoadTemperature();
    App_BatMan_LoadBqStatus();
    App_BatMan_UpdateFaultState();
}

/**
 * @brief 输出一行紧凑调试信息。
 *
 * 当前版本面向持续运行，不再每秒逐节刷屏，避免串口日志过载。
 */
static void App_BatMan_PrintDebug(void)
{
    printf("bat cell:%u/%u/%u d:%u stack:%lu curr:%ld temp:%d fet:%02x safe:%02x/%02x/%02x soc:%.1f sc:%u res:%.1f k:%.3f p:%.6f soh:%u cyc:%lu fault:%u\r\n",
           (unsigned int)cell_min_mv,
           (unsigned int)cell_avg_mv,
           (unsigned int)cell_max_mv,
           (unsigned int)cell_delta_mv,
           (unsigned long)stack_mv,
           (long)current_ma,
           temp_cell_c,
           fet_status,
           safety_status_a,
           safety_status_b,
           safety_status_c,
           (double)display_soc_percent,
           soc_confidence_percent,
           (double)soc_residual_mv,
           (double)soc_kalman_gain,
           (double)soc_p,
           soh_percent,
           (unsigned long)cycle_count,
           fault_active ? 1u : 0u);
}

/**
 * @brief 初始化 BQ76952 APP 层。
 *
 * 流程顺序不能随意调整：
 * 1. 先复位 APP 快照和 OLED 状态；
 * 2. 初始化板级 BQ 通信并设置 CRC 模式；
 * 3. reset BQ 并读取 Device Number 验证 I2C/协议；
 * 4. 进入 ConfigUpdate 写 Data Memory；
 * 5. 退出 ConfigUpdate 后清启动告警并执行 FET_ENABLE；
 * 6. 采样一次，给上层提供首帧有效快照。
 */
void App_BatMan_Init(void)
{
    uint8_t data[2];
    uint16_t device_number;
    Int_BQ76952_StatusTypeDef ret;

    /* OLED 先显示 FAIL，直到 Device Number 读取成功。 */
    App_BatMan_ResetState();
    App_BatMan_InitAlgorithms();
    App_OLED_ShowIicStatus(false);

    /*
     * CRC 模式必须在第一条 BQ 命令前确定；主从 CRC 设置不一致时，
     * 后续读写会表现为协议失败。
     */
    Int_BQ76952_InitBoard();
    Int_BQ76952_SetCrcEnabled(APP_BATMAN_CRC_BOOT_ENABLE != 0u);

    /*
     * reset/wake 失败时不继续写配置，避免芯片处于未知状态时留下半配置。
     */
    ret = Int_BQ76952_Reset();
    if (ret != INT_BQ76952_OK)
    {
        printf("bq reset fail ret:%d hal:0x%08lx\r\n",
               (int)ret,
               (unsigned long)Int_BQ76952_GetLastHalError());
        return;
    }
    HAL_Delay(APP_BATMAN_BQ_RESET_SETTLE_MS);

    /*
     * Device Number 是通信链路的第一道硬确认：地址、subcommand 帧和
     * 读回长度都必须正确。
     */
    ret = Int_BQ76952_ReadSubcommand(BQ76952_SUBCMD_DEVICE_NUMBER, data, 2u);
    if (ret != INT_BQ76952_OK)
    {
        printf("bq device fail ret:%d hal:0x%08lx\r\n",
               (int)ret,
               (unsigned long)Int_BQ76952_GetLastHalError());
        return;
    }

    device_number = App_BatMan_ReadU16Le(data);
    printf("bq ok dev:0x%04x crc:%u\r\n",
           (unsigned int)device_number,
           Int_BQ76952_IsCrcEnabled() ? 1u : 0u);
    App_OLED_ShowIicStatus(true);

    /*
     * Data Memory 写入必须包在 ConfigUpdate 内。ConfigUpdate 未退出前，
     * 不做正常采样，也不执行 FET_ENABLE。
     */
    if (Int_BQ76952_EnterConfigUpdate() != INT_BQ76952_OK)
    {
        printf("bq cfg enter fail\r\n");
        App_OLED_ShowIicStatus(false);
        return;
    }

    if (!App_BatMan_ConfigBq())
    {
        printf("bq cfg write fail\r\n");
        App_OLED_ShowIicStatus(false);
        (void)Int_BQ76952_ExitConfigUpdate();
        return;
    }
    /*
     * Power Config 显示在 OLED 上，用作现场快速确认 Data Memory 读链路。
     */
    if (Int_BQ76952_ReadDataMemory(BQ76952_DM_POWER_CONFIG, data, 2u) == INT_BQ76952_OK)
    {
        s_power_config = App_BatMan_ReadU16Le(data);
        App_OLED_ShowBqIicPowerConfig(true, s_power_config);
        printf("bq power_config:0x%04x\r\n", (unsigned int)s_power_config);
    }

    if (Int_BQ76952_ExitConfigUpdate() != INT_BQ76952_OK)
    {
        printf("bq cfg exit fail\r\n");
        App_OLED_ShowIicStatus(false);
        return;
    }

    /*
     * ConfigUpdate 退出后，只清启动噪声告警，然后允许 BQ FET 状态机运行。
     * APP 不直接强制 CHG/DSG 导通。
     */
    App_BatMan_ClearStartupAlarms();
    if (App_BatMan_EnableBqFetControl() != INT_BQ76952_OK)
    {
        printf("bq fet enable fail\r\n");
        App_OLED_ShowIicStatus(false);
        return;
    }

    /*
     * 初始化成功后立即采样一次，避免 UART/OLED/CAN 首帧仍是全零快照。
     */
    App_BatMan_Sample();
    App_BatMan_UpdateHealth(0u);
    App_BatMan_UpdateSoc(0u);
    App_OLED_ShowIicStatus(!s_comm_fault);
    printf("batman init ok\r\n");
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
    App_OLED_ShowIicStatus(!s_comm_fault);

    s_debug_ms += interval_ms;
    if (s_debug_ms >= APP_BATMAN_DEBUG_PERIOD_MS)
    {
        s_debug_ms = 0u;
        App_BatMan_PrintDebug();
    }
}
