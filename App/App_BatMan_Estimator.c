#include "App_BatMan_Internal.h"

#include "Com_SOC.h"
#include "Com_SOH.h"
#include "Int_BQ76952.h"
#include "Int_BQ76952_BSP.h"

/*
 * 显示 SOC 滤波系数。
 * 充电响应略快，放电响应略慢，静置时只轻微引入 OCV 校正，避免显示跳变。
 */
enum
{
    APP_BATMAN_SOC_REST_NEED_MS = 600000u,
    APP_BATMAN_SOC_CURRENT_DEADBAND_MA = 30,
    APP_BATMAN_SOC_REST_CURRENT_MA = 100,
    APP_BATMAN_SOC_REST_STABLE_MV = 3u,
    APP_BATMAN_SOC_FULL_CURRENT_MA = 250,
    APP_BATMAN_SOC_EMPTY_CURRENT_MA = 250,
    APP_BATMAN_SOC_FULL_ANCHOR_HOLD_MS = 5000u,
    APP_BATMAN_SOC_EMPTY_ANCHOR_HOLD_MS = 5000u,

    APP_BATMAN_RC_R0_MOHM = 35,
    APP_BATMAN_RC_RP_MOHM = 20,
    APP_BATMAN_RC_TAU_MS = 4000u,
    APP_BATMAN_RC_CURRENT_DEADBAND_MA = 200,
    APP_BATMAN_RC_COMP_MAX_MV = 550,

    APP_BATMAN_BALANCE_START_MIN_MV = 3900u,
    APP_BATMAN_BALANCE_STOP_MIN_MV = 3850u,
    APP_BATMAN_BALANCE_START_DELTA_MV = 40u,
    APP_BATMAN_BALANCE_STOP_DELTA_MV = 20u,
    APP_BATMAN_BALANCE_TEMP_MIN_C = 0,
    APP_BATMAN_BALANCE_TEMP_MAX_C = 45,
    APP_BATMAN_BALANCE_IC_TEMP_MAX_C = 70,

    /* 早期 SOH 趋势提醒，不是最终老化模型。 */
    APP_BATMAN_HEALTH_DELTA_WARN_MV = 80u,
    APP_BATMAN_HEALTH_TEMP_WARN_C = 55
};

#define APP_BATMAN_SOC_KALMAN_Q                 (0.002f)
#define APP_BATMAN_SOC_KALMAN_R                 (9.0f)
#define APP_BATMAN_SOC_OCV_STEP_LIMIT_PERCENT   (3.0f)
#define APP_BATMAN_SOC_DISPLAY_RISE_PER_S       (1.5f)
#define APP_BATMAN_SOC_DISPLAY_FALL_PER_S       (2.5f)

static uint32_t s_balance_ms = 0u;
static uint16_t s_last_balance_mask = BQ76952_CELL_MASK_NONE;
static int32_t s_rc_polar_mv = 0;

static const uint16_t s_balance_cell_mask[APP_BATMAN_CELL_COUNT] =
{
    BQ76952_CELL_MASK_6S_HW_CELL1,
    BQ76952_CELL_MASK_6S_HW_CELL2,
    BQ76952_CELL_MASK_6S_HW_CELL3,
    BQ76952_CELL_MASK_6S_HW_CELL4,
    BQ76952_CELL_MASK_6S_HW_CELL5,
    BQ76952_CELL_MASK_6S_HW_CELL6
};

void App_BatMan_ResetEstimatorState(void)
{
    s_balance_ms = 0u;
    s_last_balance_mask = BQ76952_CELL_MASK_NONE;
    s_rc_polar_mv = 0;
}

/**
 * @brief 初始化 SOC/SOH 纯算法模块。
 *
 * 参数仍放在 APP 层集中表达，便于现场按电芯和热敏实测结果调参。
 * COM 层只保存算法状态，不知道 BQ、SC 或 OLED 的存在。
 */
void App_BatMan_InitAlgorithms(void)
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
    soc_config.kalman_q = APP_BATMAN_SOC_KALMAN_Q;
    soc_config.kalman_r = APP_BATMAN_SOC_KALMAN_R;
    soc_config.ocv_update_limit_percent = APP_BATMAN_SOC_OCV_STEP_LIMIT_PERCENT;
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
    soh_config.learn_min_percent = 50u;
    soh_config.learn_max_percent = 110u;
    Com_SOH_Init(&soh_config);
}

static int32_t App_BatMan_LimitRcCompMv(int32_t value_mv)
{
    if (value_mv < 0)
    {
        return 0;
    }
    if (value_mv > APP_BATMAN_RC_COMP_MAX_MV)
    {
        return APP_BATMAN_RC_COMP_MAX_MV;
    }

    return value_mv;
}

static uint16_t App_BatMan_AddCompToCellMv(uint16_t cell_raw_mv, int32_t comp_mv)
{
    uint32_t value_mv;

    if (cell_raw_mv == 0u)
    {
        return 0u;
    }

    value_mv = (uint32_t)cell_raw_mv + (uint32_t)App_BatMan_LimitRcCompMv(comp_mv);
    if (value_mv > APP_BATMAN_CELL_VALID_MAX_MV)
    {
        value_mv = APP_BATMAN_CELL_VALID_MAX_MV;
    }

    return (uint16_t)value_mv;
}

void App_BatMan_UpdateRcModel(uint32_t interval_ms)
{
    int32_t discharge_current_ma = 0;
    int32_t ohmic_mv = 0;
    int32_t polar_target_mv = 0;
    int32_t total_comp_mv;

    if (!s_cells_sample_valid || !s_current_sample_valid)
    {
        cell_min_rc_mv = cell_min_mv;
        cell_avg_rc_mv = cell_avg_mv;
        cell_rc_ohmic_mv = 0;
        cell_rc_polar_mv = (int16_t)App_BatMan_LimitRcCompMv(s_rc_polar_mv);
        cell_rc_total_mv = cell_rc_polar_mv;
        return;
    }

    if (current_ma < -APP_BATMAN_RC_CURRENT_DEADBAND_MA)
    {
        discharge_current_ma = -current_ma;
    }

    /*
     * Thevenin 一阶 RC 模型：实测端电压 = OCV - I*R0 - Vp。
     * 这里反推 OCV 估计值，只用于 APP 软欠压策略和日志，不替代 BQ 硬保护。
     */
    ohmic_mv = (discharge_current_ma * APP_BATMAN_RC_R0_MOHM) / 1000;
    polar_target_mv = (discharge_current_ma * APP_BATMAN_RC_RP_MOHM) / 1000;

    if (interval_ms == 0u)
    {
        s_rc_polar_mv = polar_target_mv;
    }
    else
    {
        s_rc_polar_mv += ((polar_target_mv - s_rc_polar_mv) * (int32_t)interval_ms) /
                         (int32_t)(APP_BATMAN_RC_TAU_MS + interval_ms);
    }

    total_comp_mv = App_BatMan_LimitRcCompMv(ohmic_mv + s_rc_polar_mv);
    cell_min_rc_mv = App_BatMan_AddCompToCellMv(cell_min_mv, total_comp_mv);
    cell_avg_rc_mv = App_BatMan_AddCompToCellMv(cell_avg_mv, total_comp_mv);
    cell_rc_ohmic_mv = (int16_t)App_BatMan_LimitRcCompMv(ohmic_mv);
    cell_rc_polar_mv = (int16_t)App_BatMan_LimitRcCompMv(s_rc_polar_mv);
    cell_rc_total_mv = (int16_t)total_comp_mv;
}

/**
 * @brief 更新轻量 SOC 估算。
 *
 * 设计意图：
 * - 用 CC2 电流做库仑积分；
 * - 低电流时用 OCV 表轻微拉回；
 * - 显示值做滤波，避免 OLED/Linux 看到跳变。
 */
void App_BatMan_UpdateSoc(uint32_t interval_ms)
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
    sample.soh_valid = soh_capacity_valid;
    sample.soh_percent = soh_percent;
    Com_SOC_Update(&sample);

    Com_SOC_GetResult(&result);
    soc_percent = result.soc_percent;
    display_soc_percent = result.display_percent;
    soc_confidence_percent = result.confidence_percent;
    soc_residual_percent = result.residual_percent;
    soc_kalman_gain = result.kalman_gain_soc;
    soc_p = result.p_soc;
    soc_active_capacity_mah = result.active_capacity_mah;
    s_soc_full_anchor_used = result.full_anchor_used;
    s_soc_empty_anchor_used = result.empty_anchor_used;
}

static uint16_t App_BatMan_MakeBalanceMask(void)
{
    uint8_t i;
    uint8_t max_index = 0u;
    uint8_t last_index = APP_BATMAN_CELL_COUNT;

    if (s_comm_fault || fault_active || !s_cells_sample_valid)
    {
        return BQ76952_CELL_MASK_NONE;
    }
    if ((temp_cell_c < APP_BATMAN_BALANCE_TEMP_MIN_C) ||
        (temp_cell_c > APP_BATMAN_BALANCE_TEMP_MAX_C) ||
        (temp_ic_c >= APP_BATMAN_BALANCE_IC_TEMP_MAX_C))
    {
        return BQ76952_CELL_MASK_NONE;
    }
    if ((cell_min_mv < APP_BATMAN_BALANCE_STOP_MIN_MV) ||
        (cell_delta_mv <= APP_BATMAN_BALANCE_STOP_DELTA_MV))
    {
        return BQ76952_CELL_MASK_NONE;
    }

    for (i = 0u; i < APP_BATMAN_CELL_COUNT; i++)
    {
        if ((s_last_balance_mask & s_balance_cell_mask[i]) != 0u)
        {
            last_index = i;
        }
    }

    for (i = 1u; i < APP_BATMAN_CELL_COUNT; i++)
    {
        if (cell_mv[i] > cell_mv[max_index])
        {
            max_index = i;
        }
    }

    if ((cell_min_mv < APP_BATMAN_BALANCE_START_MIN_MV) ||
        (cell_delta_mv < APP_BATMAN_BALANCE_START_DELTA_MV))
    {
        if ((last_index < APP_BATMAN_CELL_COUNT) &&
            ((uint16_t)(cell_mv[last_index] - cell_min_mv) > APP_BATMAN_BALANCE_STOP_DELTA_MV))
        {
            return s_last_balance_mask;
        }
        return BQ76952_CELL_MASK_NONE;
    }

    if ((uint16_t)(cell_mv[max_index] - cell_min_mv) < APP_BATMAN_BALANCE_START_DELTA_MV)
    {
        if ((last_index < APP_BATMAN_CELL_COUNT) &&
            ((uint16_t)(cell_mv[last_index] - cell_min_mv) > APP_BATMAN_BALANCE_STOP_DELTA_MV))
        {
            return s_last_balance_mask;
        }
        return BQ76952_CELL_MASK_NONE;
    }

    return s_balance_cell_mask[max_index];
}

void App_BatMan_UpdateBalance(uint32_t interval_ms)
{
    uint16_t new_mask;

    s_balance_ms += interval_ms;
    if ((s_balance_ms < APP_BATMAN_BALANCE_PERIOD_MS) &&
        !fault_active &&
        !s_comm_fault)
    {
        return;
    }
    s_balance_ms = 0u;

    new_mask = App_BatMan_MakeBalanceMask();
    if (Int_BQ76952_SetBalanceMask(new_mask) != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        new_mask = BQ76952_CELL_MASK_NONE;
    }

    balance_mask = new_mask;
    s_last_balance_mask = new_mask;
}

/**
 * @brief 更新容量 SOH 学习和健康趋势统计。
 *
 * 容量 SOH 由一次连续的满电到空电净放电量学习；健康趋势分数单独记录等效循环、
 * 最大压差、最高温度和 safety fault，不能与容量 SOH 混为同一指标。
 */
void App_BatMan_UpdateHealth(uint32_t interval_ms)
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
    sample.full_anchor_used = s_soc_full_anchor_used;
    sample.empty_anchor_used = s_soc_empty_anchor_used;
    Com_SOH_Update(&sample);

    Com_SOH_GetResult(&result);
    charge_throughput_mah = result.charge_throughput_mah;
    discharge_throughput_mah = result.discharge_throughput_mah;
    cycle_count = result.cycle_count;
    soh_learned_capacity_mah = result.learned_capacity_mah;
    soh_learning_discharge_mah = result.learning_net_discharge_mah;
    soh_capacity_learning_count = result.capacity_learning_count;
    soh_percent = result.soh_percent;
    health_score_percent = result.health_score_percent;
    soh_confidence_percent = result.confidence_percent;
    soh_capacity_valid = result.capacity_valid;
    soh_learning_active = result.learning_active;
    s_soh_capacity_updated = result.capacity_updated;
}
