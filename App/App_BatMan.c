#include "App_BatMan.h"
#include "App_BatMan_Internal.h"

#include "Int_BQ76952.h"
#include "Int_BQ76952_BSP.h"
#include "Int_Log.h"
#include "App_OLED.h"
#include "App_SC8815.h"
#include "App_Safety.h"
#include "main.h"

/**
 * @file App_BatMan.c
 * @brief BQ76952 电池监控 APP 层主流程门面。
 *
 * 本文件只保留初始化和周期任务的执行顺序。BQ Data Memory 配置、采样、
 * SOC/SOH 适配、均衡更新、OLED/非阻塞日志辅助分别放在 App_BatMan_xxx.c。
 */

/*
 * Bring-up 配置项。
 * 当前实板通信按 non-CRC 模式验证通过；若重新启用 CRC，需要同时确认
 * BQ 侧协议模式、读写帧格式和串口日志结果。
 */
enum
{
    APP_BATMAN_CRC_BOOT_ENABLE = 0u,
    APP_BATMAN_BQ_RESET_SETTLE_MS = 200u,
    APP_BATMAN_ALERT_CONFIGURED_MASK = BQ76952_ALARM_SAFETY_PIN_MASK,
    APP_BATMAN_ALERT_CRITICAL_ALARM_MASK =
        BQ76952_ALARM_SSBC_MASK | BQ76952_ALARM_SSA_MASK | BQ76952_ALARM_PF_MASK,
    APP_BATMAN_ALERT_UNRESOLVED_RAW_MASK = BQ76952_ALARM_SSBC_MASK | BQ76952_ALARM_SSA_MASK |
                                           BQ76952_ALARM_PF_MASK | BQ76952_ALARM_MSK_SFALERT_MASK |
                                           BQ76952_ALARM_MSK_PFALERT_MASK |
                                           BQ76952_ALARM_SHUTV_MASK | BQ76952_ALARM_FUSE_MASK
};

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
uint16_t cell_min_rc_mv;
uint16_t cell_avg_rc_mv;
int16_t cell_rc_ohmic_mv;
int16_t cell_rc_polar_mv;
int16_t cell_rc_total_mv;
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
uint32_t soh_learned_capacity_mah;
uint32_t soh_learning_discharge_mah;
uint16_t soh_capacity_learning_count;
uint8_t soh_percent;
uint8_t health_score_percent;
uint8_t soh_confidence_percent;
bool soh_capacity_valid;
bool soh_learning_active;
uint16_t balance_mask;

/*
 * APP_BatMan 内部跨文件共享状态。
 * 这些标志只给 App_BatMan_xxx.c 使用，不放进对外头文件。
 */
bool s_comm_fault = false;
bool s_cells_sample_valid = false;
bool s_current_sample_valid = false;
bool s_temp_cell_sample_valid = false;
uint32_t s_fault_flags = APP_BATMAN_FAULT_NONE;
bool s_soc_full_anchor_used = false;
bool s_soc_empty_anchor_used = false;
bool s_soh_capacity_updated = false;

static bool s_bq_alert_recheck_required = false;
static bool s_bq_alert_critical_reported = false;
static bool s_bq_ready_reported = false;
static bool s_bq_ready_state = false;

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
 * @brief 按 BQ76952 little-endian 格式写入 32-bit 值。
 */
void App_BatMan_WriteU32Le(uint32_t value, uint8_t data[4])
{
    data[0] = (uint8_t)(value & 0xFFu);
    data[1] = (uint8_t)((value >> 8u) & 0xFFu);
    data[2] = (uint8_t)((value >> 16u) & 0xFFu);
    data[3] = (uint8_t)((value >> 24u) & 0xFFu);
}

static bool App_BatMan_IsAlertPinAsserted(void)
{
    return (HAL_GPIO_ReadPin(BQ_INT_GPIO_Port, BQ_INT_Pin) == GPIO_PIN_RESET);
}

static uint32_t App_BatMan_MakeAlertContext(uint32_t fault_flags)
{
    return ((uint32_t)alarm_status << 16u) | (fault_flags & 0xFFFFu);
}

static bool App_BatMan_HasUnresolvedAlertSource(void)
{
    /*
     * XCHG/XDSG 不作为恢复阻断源：host FET_CONTROL 全关本身可能维持这些观察位。
     * 是否安全只由同帧 Safety/PF、温度、配置和其他独立实时源决定。
     */
    return ((safety_alert_a != 0u) || (safety_alert_b != 0u) || (safety_alert_c != 0u) ||
            ((alarm_raw & APP_BATMAN_ALERT_UNRESOLVED_RAW_MASK) != 0u) ||
            ((battery_status & BQ76952_BATTERY_STATUS_SDM_MASK) != 0u));
}

static bool App_BatMan_ClearResolvedAlarmLatch(void)
{
    uint8_t data[2];
    uint16_t clear_mask = alarm_status & APP_BATMAN_ALERT_CONFIGURED_MASK;
    Int_BQ76952_StatusTypeDef ret;

    if (clear_mask == 0u)
    {
        return true;
    }

    /* 只在 Safety/PF/实时限制均明确解除后 W1C 本帧看到的非关键锁存位。 */
    App_BatMan_WriteU16Le(clear_mask, data);
    ret = Int_BQ76952_WriteDirect(BQ76952_CMD_ALARM_STATUS, data, 2u);
    if (ret != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        s_fault_flags |= APP_BATMAN_FAULT_COMMUNICATION;
        fault_active = true;
        return false;
    }
    return true;
}

static void App_BatMan_ObserveAlert(bool *pending, uint32_t *sequence)
{
    *pending = Int_BQ76952_IsAlertPending();
    *sequence = Int_BQ76952_GetAlertSequence();

    if (*pending || App_BatMan_IsAlertPinAsserted())
    {
        if (!s_bq_alert_recheck_required)
        {
            App_Safety_SetPowerInhibit(APP_SAFETY_INHIBIT_BQ_ALERT);
        }
        s_bq_alert_recheck_required = true;
    }
}

static void App_BatMan_ProcessAlertAfterSample(bool sample_valid,
                                               bool pending_before_sample,
                                               uint32_t sequence_before_sample)
{
    uint32_t flags;
    uint32_t context;
    bool critical;
    bool unresolved_source;

    if (Int_BQ76952_IsAlertPending() || App_BatMan_IsAlertPinAsserted())
    {
        if (!s_bq_alert_recheck_required)
        {
            App_Safety_SetPowerInhibit(APP_SAFETY_INHIBIT_BQ_ALERT);
        }
        s_bq_alert_recheck_required = true;
    }
    if (!sample_valid)
    {
        /* 半帧/通信失败时保留 pending 和功率 inhibit，等待下一完整状态帧。 */
        return;
    }

    flags = App_BatMan_GetFaultFlags();
    context = App_BatMan_MakeAlertContext(flags);
    unresolved_source = App_BatMan_HasUnresolvedAlertSource();
    critical = (((flags & (APP_BATMAN_FAULT_SAFETY | APP_BATMAN_FAULT_PERMANENT_FAILURE)) != 0u) ||
                ((alarm_status & APP_BATMAN_ALERT_CRITICAL_ALARM_MASK) != 0u) ||
                ((alarm_raw & APP_BATMAN_ALERT_CRITICAL_ALARM_MASK) != 0u));
    if (critical)
    {
        if (!s_bq_alert_critical_reported)
        {
            App_Safety_ResolveBqAlert(true, context);
            s_bq_alert_critical_reported = true;
        }
        /* Safety 锁存和 BQ 主 FET 关断必须在同一 owner 周期闭环。 */
        (void)App_BatMan_KeepMainFetsOff();
        (void)Int_BQ76952_AcknowledgeAlert(sequence_before_sample);
        return;
    }

    /* 配置、温度、FET、范围或任何未解除实时源都必须继续禁止功率。 */
    if (unresolved_source && !s_bq_alert_recheck_required)
    {
        App_Safety_SetPowerInhibit(APP_SAFETY_INHIBIT_BQ_ALERT);
        s_bq_alert_recheck_required = true;
    }
    if ((flags != APP_BATMAN_FAULT_NONE) || unresolved_source)
    {
        return;
    }

    if ((alarm_status & APP_BATMAN_ALERT_CONFIGURED_MASK) != 0u)
    {
        /* 本周期仅清锁存；下一完整帧且 ALERT 已回高后才撤销 inhibit。 */
        (void)App_BatMan_ClearResolvedAlarmLatch();
        return;
    }

    if (!s_bq_alert_recheck_required || App_BatMan_IsAlertPinAsserted())
    {
        return;
    }

    /*
     * 关中断后再次核对序号和 active-low 电平，避免新 ALERT 恰好落在“判定安全”
     * 与清 inhibit 之间而被旧完整帧错误确认。
     */
    {
        uint32_t primask = __get_PRIMASK();

        __disable_irq();
        if ((Int_BQ76952_GetAlertSequence() == sequence_before_sample) &&
            !App_BatMan_IsAlertPinAsserted() &&
            (pending_before_sample || s_bq_alert_recheck_required))
        {
            App_Safety_ResolveBqAlert(false, context);
            (void)Int_BQ76952_AcknowledgeAlert(sequence_before_sample);
            s_bq_alert_recheck_required = false;
        }
        if (primask == 0u)
        {
            __enable_irq();
        }
    }
}

static void App_BatMan_ReportBqReadiness(void)
{
    bool ready = App_BatMan_IsOnline() && App_BatMan_IsConfigValid();

    if (!s_bq_ready_reported || (ready != s_bq_ready_state))
    {
        App_Safety_ReportBqReady(ready);
        s_bq_ready_state = ready;
        s_bq_ready_reported = true;
    }
}

static bool App_BatMan_FrameLostConfigProof(void)
{
    const uint16_t invalid_battery_status =
        BQ76952_BATTERY_STATUS_POR_MASK | BQ76952_BATTERY_STATUS_CFGUPDATE_MASK;

    /*
     * 采样层只原子发布原始快照；安全门面在这里解释复位指纹。这样一旦确认
     * POR/CFGUPDATE 或 FET_EN 丢失，第一项有副作用的操作就是硬件 PSTOP。
     */
    return ((battery_status & invalid_battery_status) != 0u) ||
           ((manufacturing_status & BQ76952_MFG_STATUS_FET_EN_MASK) == 0u);
}

static void App_BatMan_EnforceRuntimeProof(bool sample_valid)
{
    bool frame_lost_config_proof = sample_valid && App_BatMan_FrameLostConfigProof();
    bool proof_invalid = !sample_valid || frame_lost_config_proof || !App_BatMan_IsConfigValid();

    if (!proof_invalid)
    {
        return;
    }

    /* 证明丢失后的第一项有副作用操作是 PSTOP；然后撤权，最后有界尝试 BQ ALL-OFF。 */
    App_SC8815_EmergencyStop();
    if (!sample_valid || frame_lost_config_proof)
    {
        /* 撤销启动期配置缓存，不能把下一次 transport ACK 当成配置证明。 */
        App_BatMan_MarkConfigRecoveryRequired();
    }
    App_Safety_ReportBqReady(false);
    s_bq_ready_state = false;
    s_bq_ready_reported = true;
    if (App_BatMan_KeepMainFetsOff() != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        s_fault_flags |= APP_BATMAN_FAULT_COMMUNICATION | APP_BATMAN_FAULT_FET_CONTROL_INVALID;
        fault_active = true;
    }
}

void App_BatMan_NotifyAlertFromISR(void)
{
    Int_BQ76952_NotifyAlertFromISR();
}

bool App_BatMan_EarlySafeOutputs(void)
{
    /* 可重复调用；驱动锁为静态对象，ALL_FETS_OFF 只会强化安全状态。 */
    Int_BQ76952_InitBoard();
    Int_BQ76952_SetCrcEnabled(APP_BATMAN_CRC_BOOT_ENABLE != 0u);
    return App_BatMan_PreResetAllFetsOff();
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
    cell_min_rc_mv = 0u;
    cell_avg_rc_mv = 0u;
    cell_rc_ohmic_mv = 0;
    cell_rc_polar_mv = 0;
    cell_rc_total_mv = 0;
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
    soh_learned_capacity_mah = 0u;
    soh_learning_discharge_mah = 0u;
    soh_capacity_learning_count = 0u;
    soh_percent = 0u;
    health_score_percent = 100u;
    soh_confidence_percent = 0u;
    soh_capacity_valid = false;
    soh_learning_active = false;
    s_soc_full_anchor_used = false;
    s_soc_empty_anchor_used = false;
    s_soh_capacity_updated = false;
    balance_mask = BQ76952_CELL_MASK_NONE;

    s_comm_fault = false;
    s_fault_flags = APP_BATMAN_FAULT_NONE;
    App_BatMan_ResetConfigState();
    App_BatMan_ResetSampleState();
    App_BatMan_ResetDebugState();
    s_bq_alert_recheck_required = false;
    s_bq_alert_critical_reported = false;
    s_bq_ready_reported = false;
    s_bq_ready_state = false;
    App_BatMan_ResetEstimatorState();
}

/**
 * @brief 初始化 BQ76952 APP 层。
 *
 * 流程顺序不能随意调整：
 * 1. 无日志地请求并确认 ALL_FETS_OFF；
 * 2. RESET、基于 HAL 时基等待并再次确认全关；
 * 3. 核对 Device Number 后进入 ConfigUpdate 写 Data Memory；
 * 4. 退出后逐项回读 manifest、清启动告警并闭环确认 FET_CONTROL 全关；
 * 5. 在安全关断后恢复 EEPROM 状态并发布第一帧完整快照。
 */
bool App_BatMan_Init(void)
{
    uint8_t data[2];
    uint16_t device_number;
    Int_BQ76952_StatusTypeDef ret;
    bool pre_reset_fets_off;
    bool post_reset_fets_off;
    bool alert_pending;
    uint32_t alert_sequence;

    /*
     * MCU 看门狗复位时 BQ 可能仍维持上次 FET 状态。第一条外设动作必须是
     * ALL_FETS_OFF + FET_STATUS 确认；在此之前禁止格式化日志、EEPROM 和显示访问。
     */
    App_BatMan_ResetState();
    pre_reset_fets_off = App_BatMan_EarlySafeOutputs();

    if (!pre_reset_fets_off)
    {
        /* 未确认全关时禁止 RESET 清掉 ALL_FETS_OFF blocker，保持安全驻留。 */
        App_BatMan_LatchConfigInvalid();
        App_OLED_ShowIicStatus(false);
        return false;
    }

    /* 关断尝试后立即 RESET，不在未知 FET 状态下执行任何可变时长维护工作。 */
    ret = Int_BQ76952_Reset();
    if (ret != INT_BQ76952_OK)
    {
        App_BatMan_LatchConfigInvalid();
        App_OLED_ShowIicStatus(false);
        return false;
    }

    /* HAL 时基给出可审计的 200 ms；复位窗口仍须 HIL 测 FET 波形。 */
    HAL_Delay(APP_BATMAN_BQ_RESET_SETTLE_MS);
    post_reset_fets_off = App_BatMan_PreResetAllFetsOff();

    Int_Log_Printf("电池管理初始化: 开始 CRC:%u\r\n", APP_BATMAN_CRC_BOOT_ENABLE != 0u ? 1u : 0u);
    if (!pre_reset_fets_off || !post_reset_fets_off)
    {
        Int_Log_Printf("BQ复位前后主FET关断未确认，配置门禁已锁存\r\n");
        App_BatMan_LatchConfigInvalid();
        App_OLED_ShowIicStatus(false);
        return false;
    }
    App_OLED_ShowIicStatus(false);

    /*
     * Device Number 同时验证地址、subcommand 帧、长度和器件身份；仅“读成功”
     * 不足以证明当前总线节点就是 BQ76952。
     */
    ret = Int_BQ76952_ReadSubcommand(BQ76952_SUBCMD_DEVICE_NUMBER, data, 2u);
    if (ret != INT_BQ76952_OK)
    {
        Int_Log_Printf("BQ设备号读取失败 ret:%d hal:0x%08lx\r\n",
                       (int)ret,
                       (unsigned long)Int_BQ76952_GetLastHalError());
        s_comm_fault = true;
        App_BatMan_LatchConfigInvalid();
        App_OLED_ShowIicStatus(false);
        (void)App_BatMan_PreResetAllFetsOff();
        return false;
    }

    device_number = App_BatMan_ReadU16Le(data);
    if (device_number != BQ76952_DEVICE_NUMBER_EXPECTED)
    {
        Int_Log_Printf("BQ设备号不匹配 exp:0x%04x act:0x%04x\r\n",
                       (unsigned int)BQ76952_DEVICE_NUMBER_EXPECTED,
                       (unsigned int)device_number);
        App_BatMan_LatchConfigInvalid();
        App_OLED_ShowIicStatus(false);
        (void)App_BatMan_PreResetAllFetsOff();
        return false;
    }
    Int_Log_Printf("BQ通信正常 设备号:0x%04x CRC:%u\r\n",
                   (unsigned int)device_number,
                   Int_BQ76952_IsCrcEnabled() ? 1u : 0u);
    App_OLED_ShowIicStatus(true);

    /*
     * Data Memory 写入必须包在 ConfigUpdate 内。ConfigUpdate 未退出前，
     * 不做正常采样，也不执行 FET_ENABLE。
     */
    Int_Log_Printf("BQ配置模式: 进入开始\r\n");
    if (Int_BQ76952_EnterConfigUpdate() != INT_BQ76952_OK)
    {
        Int_Log_Printf("BQ配置模式进入失败\r\n");
        App_OLED_ShowIicStatus(false);
        App_BatMan_LatchConfigInvalid();
        (void)App_BatMan_PreResetAllFetsOff();
        return false;
    }
    Int_Log_Printf("BQ配置模式: 进入完成\r\n");

    Int_Log_Printf("BQ配置写入: 开始\r\n");
    if (!App_BatMan_ConfigBq())
    {
        Int_Log_Printf("BQ配置写入失败\r\n");
        App_OLED_ShowIicStatus(false);
        (void)Int_BQ76952_ExitConfigUpdate();
        (void)App_BatMan_KeepMainFetsOff();
        App_BatMan_LatchConfigInvalid();
        return false;
    }
    Int_Log_Printf("BQ配置写入: 完成\r\n");

    Int_Log_Printf("BQ配置模式: 退出开始\r\n");
    if (Int_BQ76952_ExitConfigUpdate() != INT_BQ76952_OK)
    {
        Int_Log_Printf("BQ配置模式退出失败\r\n");
        App_OLED_ShowIicStatus(false);
        App_BatMan_LatchConfigInvalid();
        (void)App_BatMan_PreResetAllFetsOff();
        return false;
    }
    Int_Log_Printf("BQ配置模式: 退出完成\r\n");

    Int_Log_Printf("BQ配置全量回读: 开始\r\n");
    if (!App_BatMan_VerifyBqConfig())
    {
        Int_Log_Printf("BQ配置全量回读失败，主FET保持全关\r\n");
        App_OLED_ShowIicStatus(false);
        (void)App_BatMan_KeepMainFetsOff();
        App_BatMan_LatchConfigInvalid();
        return false;
    }
    Int_Log_Printf("BQ配置全量回读: 一致\r\n");

    /*
     * Bring-up 阶段先禁止 BQ 进入 Sleep，避免 CHG FET 被低功耗策略压住。
     * 后续若要做低功耗版本，再把 Sleep 策略放回电源管理状态机统一控制。
     */
    Int_Log_Printf("BQ Sleep禁用: 开始\r\n");
    ret = Int_BQ76952_SendSubcommand(BQ76952_SUBCMD_SLEEP_DISABLE);
    if (ret != INT_BQ76952_OK)
    {
        Int_Log_Printf("BQ Sleep禁用失败 ret:%d hal:0x%08lx\r\n",
                       (int)ret,
                       (unsigned long)Int_BQ76952_GetLastHalError());
        App_OLED_ShowIicStatus(false);
        App_BatMan_LatchConfigInvalid();
        (void)App_BatMan_KeepMainFetsOff();
        return false;
    }
    Int_Log_Printf("BQ Sleep禁用: 完成\r\n");

    /*
     * ConfigUpdate 退出后，只清启动噪声告警，并先保持 CHG/DSG/PCHG/PDSG 关断。
     * 主功率路径随后由 App_Power 按保护、充电器和放电条件统一释放。
     */
    Int_Log_Printf("BQ启动告警: 清除\r\n");
    ret = App_BatMan_ClearStartupAlarms();
    if (ret != INT_BQ76952_OK)
    {
        Int_Log_Printf("BQ启动告警清除失败 ret:%d\r\n", (int)ret);
        s_comm_fault = true;
        App_BatMan_LatchConfigInvalid();
        (void)App_BatMan_KeepMainFetsOff();
        return false;
    }
    Int_Log_Printf("BQ主FET安全控制启用: 开始\r\n");
    if (!App_BatMan_EnableFetControlSafely())
    {
        Int_Log_Printf("BQ主FET安全控制启用失败\r\n");
        App_OLED_ShowIicStatus(false);
        App_BatMan_LatchConfigInvalid();
        return false;
    }
    Int_Log_Printf("BQ主FET安全控制启用: 全关确认\r\n");

    /*
     * 初始化成功后立即采样一次，避免 UART/OLED/CAN 首帧仍是全零快照。
     */
    /* 可阻塞的 EEPROM 恢复放在主 FET 已闭环确认全关之后。 */
    App_BatMan_InitAlgorithms();
    if (App_BatMan_NvmInit())
    {
        Int_Log_Printf("SOH持久化: EEPROM在线\r\n");
    }
    else
    {
        Int_Log_Printf("SOH持久化: EEPROM暂不可用，将由维护任务重试\r\n");
    }

    Int_Log_Printf("电池管理首帧采样: 开始\r\n");
    App_BatMan_ObserveAlert(&alert_pending, &alert_sequence);
    if (!App_BatMan_Sample())
    {
        App_BatMan_EnforceRuntimeProof(false);
        App_BatMan_ProcessAlertAfterSample(false, alert_pending, alert_sequence);
        Int_Log_Printf("电池管理首帧采样失败，主FET保持全关\r\n");
        return false;
    }
    App_BatMan_EnforceRuntimeProof(true);
    App_BatMan_ProcessAlertAfterSample(true, alert_pending, alert_sequence);
    if (!App_BatMan_IsOnline() || !App_BatMan_IsConfigValid())
    {
        Int_Log_Printf("电池管理首帧配置/告警证明无效，主FET保持全关\r\n");
        return false;
    }
    App_BatMan_UpdateRcModel(0u);
    App_BatMan_UpdateSoc(0u);
    App_BatMan_UpdateHealth(0u);
    App_BatMan_NvmTask(0u);
    App_BatMan_UpdateBalance(APP_BATMAN_BALANCE_PERIOD_MS);
    App_BatMan_UpdateRuntimeOledStatus();
    Int_Log_Printf("电池管理初始化成功\r\n");
    return true;
}

/**
 * @brief BQ76952 周期任务。
 *
 * 该任务只从主循环调用；若未来由 RTOS 多任务读取这些全局快照，需要
 * 引入 snapshot 或临界区，而不是简单依赖 volatile。
 */
void App_BatMan_Task(uint32_t interval_ms)
{
    bool alert_pending;
    bool sample_valid;
    uint32_t alert_sequence;

    App_BatMan_ObserveAlert(&alert_pending, &alert_sequence);
    sample_valid = App_BatMan_Sample();
    App_BatMan_EnforceRuntimeProof(sample_valid);
    App_BatMan_ProcessAlertAfterSample(sample_valid, alert_pending, alert_sequence);
    App_BatMan_ReportBqReadiness();
    App_BatMan_UpdateRcModel(interval_ms);
    App_BatMan_UpdateSoc(interval_ms);
    App_BatMan_UpdateHealth(interval_ms);
    App_BatMan_UpdateBalance(interval_ms);
    App_BatMan_UpdateRuntimeOledStatus();
}

void App_BatMan_MaintenanceTask(uint32_t interval_ms)
{
    App_BatMan_NvmTask(interval_ms);
    App_BatMan_UpdateDebugOutput(interval_ms);
}

bool App_BatMan_IsOnline(void)
{
    return (!s_comm_fault && s_cells_sample_valid);
}
