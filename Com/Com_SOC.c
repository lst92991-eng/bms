#include "Com_SOC.h"

#include "Com_BatteryParam.h"

#include <limits.h>
#include <math.h>

#define COM_SOC_STATE_COUNT               (3u)
#define COM_SOC_INDEX_SOC                 (0u)
#define COM_SOC_INDEX_V1                  (1u)
#define COM_SOC_INDEX_V2                  (2u)
#define COM_SOC_CELL_MIN_VALID_MV         (2500u)
#define COM_SOC_CELL_MAX_VALID_MV         (4350u)
#define COM_SOC_POLAR_LIMIT_V             (0.5f)
#define COM_SOC_P_MIN                     (0.00000001f)
#define COM_SOC_P_SOC_MAX                 (1.0f)
#define COM_SOC_P_V_MAX                   (1.0f)
#define COM_SOC_P_SOC_ANCHOR              (0.000025f)

typedef struct
{
    Com_SOC_ConfigTypeDef config;
    Com_SOC_ResultTypeDef result;
    float x[COM_SOC_STATE_COUNT];
    float p[COM_SOC_STATE_COUNT][COM_SOC_STATE_COUNT];
    uint32_t rest_ms;
    uint32_t voltage_stable_ms;
    uint16_t last_cell_avg_mv;
    uint16_t rest_start_cell_avg_mv;
    uint32_t full_anchor_ms;
    uint32_t empty_anchor_ms;
    int32_t last_current_ma;
    uint32_t current_stable_ms;
} Com_SOC_StateTypeDef;

static Com_SOC_StateTypeDef s_soc;

static float Com_SOC_GetCellCurrentA(int32_t current_ma);

static float Com_SOC_ClampFloat(float value, float min, float max)
{
    if (value < min)
    {
        return min;
    }
    if (value > max)
    {
        return max;
    }
    return value;
}

static bool Com_SOC_IsFiniteFloat(float value)
{
    return (isfinite(value) != 0);
}

static uint32_t Com_SOC_AbsI32(int32_t value)
{
    if (value >= 0)
    {
        return (uint32_t)value;
    }

    if (value == INT32_MIN)
    {
        return 2147483648u;
    }

    return (uint32_t)(-value);
}

static int32_t Com_SOC_MakePositiveI32(int32_t value)
{
    if (value >= 0)
    {
        return value;
    }

    if (value == INT32_MIN)
    {
        return INT32_MAX;
    }

    return -value;
}

static uint32_t Com_SOC_DeltaI32(int32_t left, int32_t right)
{
    int64_t delta = (int64_t)left - (int64_t)right;

    if (delta < 0)
    {
        delta = -delta;
    }
    if (delta > 0xFFFFFFFFLL)
    {
        return 0xFFFFFFFFu;
    }

    return (uint32_t)delta;
}

static uint32_t Com_SOC_AddMs(uint32_t old_ms, uint32_t add_ms)
{
    if ((0xFFFFFFFFu - old_ms) > add_ms)
    {
        return old_ms + add_ms;
    }

    return 0xFFFFFFFFu;
}

static bool Com_SOC_IsCellVoltageValid(uint16_t cell_mv)
{
    return (cell_mv >= COM_SOC_CELL_MIN_VALID_MV) &&
           (cell_mv <= COM_SOC_CELL_MAX_VALID_MV);
}

static bool Com_SOC_IsCellSampleValid(const Com_SOC_SampleTypeDef *sample)
{
    if ((sample == 0) || !sample->cells_valid)
    {
        return false;
    }

    if (!Com_SOC_IsCellVoltageValid(sample->cell_avg_mv))
    {
        return false;
    }

    if ((sample->cell_min_mv > sample->cell_avg_mv) ||
        (sample->cell_avg_mv > sample->cell_max_mv))
    {
        return false;
    }

    if (!Com_SOC_IsCellVoltageValid(sample->cell_min_mv) ||
        !Com_SOC_IsCellVoltageValid(sample->cell_max_mv))
    {
        return false;
    }

    return true;
}

static void Com_SOC_FillDefaultConfig(Com_SOC_ConfigTypeDef *config)
{
    config->capacity_mah = COM_BATTERY_PARAM_CAP_TYP_MAH;
    config->default_soc_percent = 50.0f;
    config->current_sign = COM_SOC_CURRENT_POS_CHARGE;

    config->current_deadband_ma = 30;
    config->rest_need_ms = 600000u;
    config->rest_current_ma = 100;
    config->rest_voltage_stable_mv = 3u;

    config->r0_ohm = 0.012f;
    config->r1_ohm = 0.006f;
    config->c1_f = 8000.0f;
    config->r2_ohm = 0.003f;
    config->c2_f = 60000.0f;

    config->process_q_soc = 0.00000001f;
    config->process_q_v = 0.0000001f;
    config->process_q_v1 = 0.0000001f;
    config->process_q_v2 = 0.0000001f;
    config->measure_r_mv = 25.0f;
    config->dynamic_ekf_enable = false;
    config->measure_r_dynamic_mv = 80.0f;
    config->residual_reject_mv = 300.0f;
    config->dynamic_current_delta_limit_ma = 300;
    config->dynamic_current_stable_ms = 5000u;

    config->full_cell_mv = COM_BATTERY_PARAM_CELL_FULL_MV;
    config->empty_cell_mv = 3000u;
    config->full_current_ma = 250;
    config->empty_current_ma = 250;
    config->full_anchor_hold_ms = 5000u;
    config->empty_anchor_hold_ms = 5000u;

    config->display_rise_percent_per_s = 1.5f;
    config->display_fall_percent_per_s = 2.5f;
}

static void Com_SOC_CheckConfig(Com_SOC_ConfigTypeDef *config)
{
    if (config->capacity_mah == 0u)
    {
        config->capacity_mah = COM_BATTERY_PARAM_CAP_TYP_MAH;
    }
    if (config->capacity_mah > 200000u)
    {
        config->capacity_mah = 200000u;
    }
    config->default_soc_percent = Com_SOC_ClampFloat(config->default_soc_percent, 0.0f, 100.0f);
    if ((config->current_sign != COM_SOC_CURRENT_POS_CHARGE) &&
        (config->current_sign != COM_SOC_CURRENT_POS_DISCHARGE))
    {
        config->current_sign = COM_SOC_CURRENT_POS_CHARGE;
    }

    if (config->rest_need_ms == 0u)
    {
        config->rest_need_ms = 600000u;
    }
    config->rest_current_ma = Com_SOC_MakePositiveI32(config->rest_current_ma);
    if (config->rest_voltage_stable_mv == 0u)
    {
        config->rest_voltage_stable_mv = 3u;
    }
    if (config->rest_voltage_stable_mv > 50u)
    {
        config->rest_voltage_stable_mv = 50u;
    }
    config->current_deadband_ma = Com_SOC_MakePositiveI32(config->current_deadband_ma);

    config->r0_ohm = Com_SOC_ClampFloat(config->r0_ohm, 0.0f, 1.0f);
    config->r1_ohm = Com_SOC_ClampFloat(config->r1_ohm, 0.0001f, 1.0f);
    config->r2_ohm = Com_SOC_ClampFloat(config->r2_ohm, 0.0001f, 1.0f);
    config->c1_f = Com_SOC_ClampFloat(config->c1_f, 1.0f, 1000000.0f);
    config->c2_f = Com_SOC_ClampFloat(config->c2_f, 1.0f, 1000000.0f);
    config->process_q_soc = Com_SOC_ClampFloat(config->process_q_soc, 0.000000000001f, 0.0001f);
    config->process_q_v = Com_SOC_ClampFloat(config->process_q_v, 0.000000000001f, 0.001f);
    config->process_q_v1 = Com_SOC_ClampFloat(config->process_q_v1, 0.000000000001f, 0.001f);
    config->process_q_v2 = Com_SOC_ClampFloat(config->process_q_v2, 0.000000000001f, 0.001f);
    config->measure_r_mv = Com_SOC_ClampFloat(config->measure_r_mv, 1.0f, 200.0f);
    config->measure_r_dynamic_mv = Com_SOC_ClampFloat(config->measure_r_dynamic_mv, 10.0f, 500.0f);
    config->residual_reject_mv = Com_SOC_ClampFloat(config->residual_reject_mv, 10.0f, 1000.0f);
    config->dynamic_current_delta_limit_ma =
        Com_SOC_MakePositiveI32(config->dynamic_current_delta_limit_ma);
    if (config->dynamic_current_stable_ms == 0u)
    {
        config->dynamic_current_stable_ms = 5000u;
    }
    config->full_current_ma = Com_SOC_MakePositiveI32(config->full_current_ma);
    config->empty_current_ma = Com_SOC_MakePositiveI32(config->empty_current_ma);
    if (config->full_anchor_hold_ms == 0u)
    {
        config->full_anchor_hold_ms = 5000u;
    }
    if (config->empty_anchor_hold_ms == 0u)
    {
        config->empty_anchor_hold_ms = 5000u;
    }
    if (config->empty_cell_mv < COM_SOC_CELL_MIN_VALID_MV)
    {
        config->empty_cell_mv = COM_SOC_CELL_MIN_VALID_MV;
    }
    if (config->full_cell_mv > COM_SOC_CELL_MAX_VALID_MV)
    {
        config->full_cell_mv = COM_SOC_CELL_MAX_VALID_MV;
    }
    if (config->empty_cell_mv >= config->full_cell_mv)
    {
        config->empty_cell_mv = 3000u;
        config->full_cell_mv = COM_BATTERY_PARAM_CELL_FULL_MV;
    }
    config->display_rise_percent_per_s = Com_SOC_ClampFloat(config->display_rise_percent_per_s, 0.1f, 100.0f);
    config->display_fall_percent_per_s = Com_SOC_ClampFloat(config->display_fall_percent_per_s, 0.1f, 100.0f);
}

static int32_t Com_SOC_NormalizeCurrentMa(int32_t current_ma)
{
    if (Com_SOC_AbsI32(current_ma) <= (uint32_t)s_soc.config.current_deadband_ma)
    {
        return 0;
    }

    if (s_soc.config.current_sign == COM_SOC_CURRENT_POS_DISCHARGE)
    {
        return -current_ma;
    }

    return current_ma;
}

static float Com_SOC_GetCapacityMah(const Com_SOC_SampleTypeDef *sample)
{
    float capacity_mah = (float)s_soc.config.capacity_mah;

    /*
     * SOH 和温度先只做温和修正。真正的容量学习放在 SOH/NVM 完成后再加强，
     * 这里保留接口，避免后面推翻 SOC 主框架。
     */
    if ((sample != 0) && sample->soh_valid && (sample->soh_percent >= 50u))
    {
        capacity_mah = capacity_mah * ((float)sample->soh_percent / 100.0f);
    }
    if ((sample != 0) && sample->temp_valid && (sample->temp_c < 0))
    {
        capacity_mah *= 0.90f;
    }

    if (capacity_mah < 1.0f)
    {
        capacity_mah = 1.0f;
    }
    s_soc.result.active_capacity_mah = capacity_mah;
    return capacity_mah;
}

static void Com_SOC_UpdatePublicResult(void)
{
    s_soc.result.soc_percent = s_soc.x[COM_SOC_INDEX_SOC] * 100.0f;
    s_soc.result.soc_raw_percent = s_soc.result.soc_percent;
    s_soc.result.remain_mah =
        (uint32_t)((s_soc.x[COM_SOC_INDEX_SOC] * s_soc.result.active_capacity_mah) + 0.5f);
    s_soc.result.polar_v1_mv = s_soc.x[COM_SOC_INDEX_V1] * 1000.0f;
    s_soc.result.polar_v2_mv = s_soc.x[COM_SOC_INDEX_V2] * 1000.0f;
    s_soc.result.p_soc = s_soc.p[COM_SOC_INDEX_SOC][COM_SOC_INDEX_SOC];
    s_soc.result.p_v1 = s_soc.p[COM_SOC_INDEX_V1][COM_SOC_INDEX_V1];
    s_soc.result.p_v2 = s_soc.p[COM_SOC_INDEX_V2][COM_SOC_INDEX_V2];
    s_soc.result.rest_ms = s_soc.rest_ms;
    s_soc.result.voltage_stable_ms = s_soc.voltage_stable_ms;
    s_soc.result.current_stable_ms = s_soc.current_stable_ms;
}

static void Com_SOC_UpdateVoltageModelDebug(const Com_SOC_SampleTypeDef *sample,
                                            int32_t current_ma)
{
    float current_a;
    uint16_t soc_0p01;

    if ((sample == 0) || !Com_SOC_IsCellSampleValid(sample))
    {
        s_soc.result.measured_cell_mv = 0.0f;
        s_soc.result.ekf_used_avg_voltage = false;
        return;
    }

    current_a = Com_SOC_GetCellCurrentA(current_ma);
    soc_0p01 = (uint16_t)(s_soc.x[COM_SOC_INDEX_SOC] * 10000.0f);

    s_soc.result.measured_cell_mv = (float)sample->cell_avg_mv;
    s_soc.result.ekf_used_avg_voltage = true;
    s_soc.result.ocv_cell_mv = (float)Com_BatteryParam_GetVoltageBySoc0p01(soc_0p01);
    s_soc.result.ir0_mv = s_soc.config.r0_ohm * current_a * 1000.0f;
    s_soc.result.model_cell_mv =
        s_soc.result.ocv_cell_mv +
        s_soc.result.ir0_mv +
        (s_soc.x[COM_SOC_INDEX_V1] * 1000.0f) +
        (s_soc.x[COM_SOC_INDEX_V2] * 1000.0f);
    s_soc.result.residual_mv =
        s_soc.result.measured_cell_mv - s_soc.result.model_cell_mv;
}

static void Com_SOC_ResetCovariance(void)
{
    uint8_t i;
    uint8_t j;

    for (i = 0u; i < COM_SOC_STATE_COUNT; i++)
    {
        for (j = 0u; j < COM_SOC_STATE_COUNT; j++)
        {
            s_soc.p[i][j] = 0.0f;
        }
    }

    s_soc.p[COM_SOC_INDEX_SOC][COM_SOC_INDEX_SOC] = 0.05f * 0.05f;
    s_soc.p[COM_SOC_INDEX_V1][COM_SOC_INDEX_V1] = 0.02f * 0.02f;
    s_soc.p[COM_SOC_INDEX_V2][COM_SOC_INDEX_V2] = 0.02f * 0.02f;
}

static void Com_SOC_ProtectCovariance(void)
{
    uint8_t i;
    uint8_t j;
    float avg;

    for (i = 0u; i < COM_SOC_STATE_COUNT; i++)
    {
        for (j = (uint8_t)(i + 1u); j < COM_SOC_STATE_COUNT; j++)
        {
            avg = 0.5f * (s_soc.p[i][j] + s_soc.p[j][i]);
            s_soc.p[i][j] = avg;
            s_soc.p[j][i] = avg;
        }
    }

    if (s_soc.p[COM_SOC_INDEX_SOC][COM_SOC_INDEX_SOC] < COM_SOC_P_MIN)
    {
        s_soc.p[COM_SOC_INDEX_SOC][COM_SOC_INDEX_SOC] = COM_SOC_P_MIN;
    }
    if (s_soc.p[COM_SOC_INDEX_V1][COM_SOC_INDEX_V1] < COM_SOC_P_MIN)
    {
        s_soc.p[COM_SOC_INDEX_V1][COM_SOC_INDEX_V1] = COM_SOC_P_MIN;
    }
    if (s_soc.p[COM_SOC_INDEX_V2][COM_SOC_INDEX_V2] < COM_SOC_P_MIN)
    {
        s_soc.p[COM_SOC_INDEX_V2][COM_SOC_INDEX_V2] = COM_SOC_P_MIN;
    }
    if (s_soc.p[COM_SOC_INDEX_SOC][COM_SOC_INDEX_SOC] > COM_SOC_P_SOC_MAX)
    {
        s_soc.p[COM_SOC_INDEX_SOC][COM_SOC_INDEX_SOC] = COM_SOC_P_SOC_MAX;
    }
    if (s_soc.p[COM_SOC_INDEX_V1][COM_SOC_INDEX_V1] > COM_SOC_P_V_MAX)
    {
        s_soc.p[COM_SOC_INDEX_V1][COM_SOC_INDEX_V1] = COM_SOC_P_V_MAX;
    }
    if (s_soc.p[COM_SOC_INDEX_V2][COM_SOC_INDEX_V2] > COM_SOC_P_V_MAX)
    {
        s_soc.p[COM_SOC_INDEX_V2][COM_SOC_INDEX_V2] = COM_SOC_P_V_MAX;
    }
}

static void Com_SOC_ProtectState(void)
{
    s_soc.x[COM_SOC_INDEX_SOC] =
        Com_SOC_ClampFloat(s_soc.x[COM_SOC_INDEX_SOC], 0.0f, 1.0f);
    s_soc.x[COM_SOC_INDEX_V1] =
        Com_SOC_ClampFloat(s_soc.x[COM_SOC_INDEX_V1],
                           -COM_SOC_POLAR_LIMIT_V,
                           COM_SOC_POLAR_LIMIT_V);
    s_soc.x[COM_SOC_INDEX_V2] =
        Com_SOC_ClampFloat(s_soc.x[COM_SOC_INDEX_V2],
                           -COM_SOC_POLAR_LIMIT_V,
                           COM_SOC_POLAR_LIMIT_V);

    if (!Com_SOC_IsFiniteFloat(s_soc.x[COM_SOC_INDEX_SOC]))
    {
        s_soc.x[COM_SOC_INDEX_SOC] = s_soc.config.default_soc_percent / 100.0f;
    }
    if (!Com_SOC_IsFiniteFloat(s_soc.x[COM_SOC_INDEX_V1]))
    {
        s_soc.x[COM_SOC_INDEX_V1] = 0.0f;
    }
    if (!Com_SOC_IsFiniteFloat(s_soc.x[COM_SOC_INDEX_V2]))
    {
        s_soc.x[COM_SOC_INDEX_V2] = 0.0f;
    }
}

static void Com_SOC_TightenSocCovariance(void)
{
    s_soc.p[COM_SOC_INDEX_SOC][COM_SOC_INDEX_SOC] = COM_SOC_P_SOC_ANCHOR;
    s_soc.p[COM_SOC_INDEX_SOC][COM_SOC_INDEX_V1] = 0.0f;
    s_soc.p[COM_SOC_INDEX_V1][COM_SOC_INDEX_SOC] = 0.0f;
    s_soc.p[COM_SOC_INDEX_SOC][COM_SOC_INDEX_V2] = 0.0f;
    s_soc.p[COM_SOC_INDEX_V2][COM_SOC_INDEX_SOC] = 0.0f;
    Com_SOC_ProtectCovariance();
}

static void Com_SOC_SeedByVoltage(uint16_t cell_avg_mv)
{
    uint16_t soc_0p01;

    if (s_soc.result.seeded || !Com_SOC_IsCellVoltageValid(cell_avg_mv))
    {
        return;
    }

    soc_0p01 = Com_BatteryParam_GetSoc0p01ByVoltage(cell_avg_mv);
    s_soc.x[COM_SOC_INDEX_SOC] =
        Com_SOC_ClampFloat((float)soc_0p01 / 10000.0f, 0.0f, 1.0f);
    s_soc.x[COM_SOC_INDEX_V1] = 0.0f;
    s_soc.x[COM_SOC_INDEX_V2] = 0.0f;
    s_soc.result.display_percent = s_soc.x[COM_SOC_INDEX_SOC] * 100.0f;
    s_soc.result.seeded = true;
    s_soc.result.seed_source = COM_SOC_SEED_OCV;
    Com_SOC_ResetCovariance();
    Com_SOC_ProtectState();
    Com_SOC_UpdatePublicResult();
}

static void Com_SOC_Predict(uint32_t interval_ms, int32_t current_ma, float capacity_mah)
{
    float dt_s;
    float current_a;
    float soc_delta;
    float a1;
    float a2;
    float b1;
    float b2;
    float f[COM_SOC_STATE_COUNT];
    float old_p[COM_SOC_STATE_COUNT][COM_SOC_STATE_COUNT];
    uint8_t row;
    uint8_t col;

    if (interval_ms == 0u)
    {
        return;
    }

    dt_s = (float)interval_ms / 1000.0f;
    current_a = (float)current_ma / 1000.0f;

    soc_delta = ((float)current_ma * dt_s) / (capacity_mah * 3600.0f);
    s_soc.x[COM_SOC_INDEX_SOC] =
        Com_SOC_ClampFloat(s_soc.x[COM_SOC_INDEX_SOC] + soc_delta, 0.0f, 1.0f);

    /*
     * 二阶 RC 精确离散化：
     * Vp(k+1) = exp(-dt/tau) * Vp(k) + R * (1-exp(-dt/tau)) * I
     */
    a1 = expf(-dt_s / (s_soc.config.r1_ohm * s_soc.config.c1_f));
    a2 = expf(-dt_s / (s_soc.config.r2_ohm * s_soc.config.c2_f));
    b1 = s_soc.config.r1_ohm * (1.0f - a1);
    b2 = s_soc.config.r2_ohm * (1.0f - a2);

    s_soc.x[COM_SOC_INDEX_V1] = (a1 * s_soc.x[COM_SOC_INDEX_V1]) + (b1 * current_a);
    s_soc.x[COM_SOC_INDEX_V2] = (a2 * s_soc.x[COM_SOC_INDEX_V2]) + (b2 * current_a);
    Com_SOC_ProtectState();

    for (row = 0u; row < COM_SOC_STATE_COUNT; row++)
    {
        for (col = 0u; col < COM_SOC_STATE_COUNT; col++)
        {
            old_p[row][col] = s_soc.p[row][col];
        }
    }

    f[COM_SOC_INDEX_SOC] = 1.0f;
    f[COM_SOC_INDEX_V1] = a1;
    f[COM_SOC_INDEX_V2] = a2;
    for (row = 0u; row < COM_SOC_STATE_COUNT; row++)
    {
        for (col = 0u; col < COM_SOC_STATE_COUNT; col++)
        {
            s_soc.p[row][col] = f[row] * old_p[row][col] * f[col];
        }
    }
    s_soc.p[COM_SOC_INDEX_SOC][COM_SOC_INDEX_SOC] += s_soc.config.process_q_soc * dt_s;
    s_soc.p[COM_SOC_INDEX_V1][COM_SOC_INDEX_V1] += s_soc.config.process_q_v1 * dt_s;
    s_soc.p[COM_SOC_INDEX_V2][COM_SOC_INDEX_V2] += s_soc.config.process_q_v2 * dt_s;
    Com_SOC_ProtectCovariance();
}

static void Com_SOC_UpdateRestState(const Com_SOC_SampleTypeDef *sample, int32_t current_ma)
{
    uint16_t diff_mv;
    bool low_current;

    low_current = (Com_SOC_AbsI32(current_ma) <= (uint32_t)s_soc.config.rest_current_ma);
    if (low_current)
    {
        if (sample != 0)
        {
            s_soc.rest_ms = Com_SOC_AddMs(s_soc.rest_ms, sample->interval_ms);
        }
    }
    else
    {
        s_soc.rest_ms = 0u;
        s_soc.voltage_stable_ms = 0u;
        s_soc.rest_start_cell_avg_mv = 0u;
        s_soc.last_cell_avg_mv = 0u;
    }

    if (low_current &&
        Com_SOC_IsCellSampleValid(sample))
    {
        if (s_soc.rest_start_cell_avg_mv == 0u)
        {
            s_soc.rest_start_cell_avg_mv = sample->cell_avg_mv;
            s_soc.voltage_stable_ms = 0u;
        }
        else
        {
            diff_mv = (sample->cell_avg_mv >= s_soc.rest_start_cell_avg_mv) ?
                      (uint16_t)(sample->cell_avg_mv - s_soc.rest_start_cell_avg_mv) :
                      (uint16_t)(s_soc.rest_start_cell_avg_mv - sample->cell_avg_mv);
            if (diff_mv <= s_soc.config.rest_voltage_stable_mv)
            {
                s_soc.voltage_stable_ms =
                    Com_SOC_AddMs(s_soc.voltage_stable_ms, sample->interval_ms);
            }
            else
            {
                s_soc.voltage_stable_ms = 0u;
                s_soc.rest_start_cell_avg_mv = sample->cell_avg_mv;
            }
        }
        s_soc.last_cell_avg_mv = sample->cell_avg_mv;
    }
    else
    {
        s_soc.voltage_stable_ms = 0u;
        s_soc.rest_start_cell_avg_mv = 0u;
        s_soc.last_cell_avg_mv = 0u;
    }

    s_soc.result.rest_ready =
        (s_soc.rest_ms >= s_soc.config.rest_need_ms) &&
        (s_soc.voltage_stable_ms >= s_soc.config.rest_need_ms);
}

static void Com_SOC_UpdateCurrentStable(uint32_t interval_ms, int32_t current_ma)
{
    uint32_t delta_ma;

    delta_ma = Com_SOC_DeltaI32(current_ma, s_soc.last_current_ma);

    if (delta_ma <= (uint32_t)s_soc.config.dynamic_current_delta_limit_ma)
    {
        s_soc.current_stable_ms = Com_SOC_AddMs(s_soc.current_stable_ms, interval_ms);
    }
    else
    {
        s_soc.current_stable_ms = 0u;
    }

    s_soc.last_current_ma = current_ma;
}

static float Com_SOC_GetCellCurrentA(int32_t current_ma)
{
    /*
     * 当前硬件按 6S1P 处理，pack current 即 cell current。
     * 若后续并联数量变化，应在这里除以并联数。
     */
    return (float)current_ma / 1000.0f;
}

static void Com_SOC_EkfVoltageUpdate(const Com_SOC_SampleTypeDef *sample, int32_t current_ma)
{
    float h[COM_SOC_STATE_COUNT];
    float ph[COM_SOC_STATE_COUNT];
    float s;
    float k[COM_SOC_STATE_COUNT];
    float r_v;
    float y;
    float slope_mv_percent;
    uint16_t soc_0p01;
    uint8_t i;
    uint8_t row;
    uint8_t col;
    float i_kh[COM_SOC_STATE_COUNT][COM_SOC_STATE_COUNT];
    float temp_p[COM_SOC_STATE_COUNT][COM_SOC_STATE_COUNT];
    float new_p[COM_SOC_STATE_COUNT][COM_SOC_STATE_COUNT];
    bool soc_clamped;

    s_soc.result.voltage_update_used = false;
    s_soc.result.dynamic_ekf_used = false;
    if (!Com_SOC_IsCellSampleValid(sample))
    {
        return;
    }
    if (!s_soc.result.rest_ready && !s_soc.config.dynamic_ekf_enable)
    {
        return;
    }
    if (!s_soc.result.rest_ready &&
        (s_soc.current_stable_ms < s_soc.config.dynamic_current_stable_ms))
    {
        return;
    }

    /*
     * EKF 的电压观测统一使用平均单体电压，和二阶 RC 模型的“等效单体”
     * 语义一致。最低/最高串只参与边界与锚定，避免 min/avg 混用。
     */
    soc_0p01 = (uint16_t)(s_soc.x[COM_SOC_INDEX_SOC] * 10000.0f);
    slope_mv_percent = Com_BatteryParam_GetOcvSlopeMvPerPercent(soc_0p01);

    Com_SOC_UpdateVoltageModelDebug(sample, current_ma);
    y = s_soc.result.residual_mv / 1000.0f;
    if ((s_soc.result.residual_mv > s_soc.config.residual_reject_mv) ||
        (s_soc.result.residual_mv < -s_soc.config.residual_reject_mv))
    {
        return;
    }

    h[COM_SOC_INDEX_SOC] = (slope_mv_percent * 100.0f) / 1000.0f;
    h[COM_SOC_INDEX_V1] = 1.0f;
    h[COM_SOC_INDEX_V2] = 1.0f;

    r_v = s_soc.result.rest_ready ?
          (s_soc.config.measure_r_mv / 1000.0f) :
          (s_soc.config.measure_r_dynamic_mv / 1000.0f);
    r_v = r_v * r_v;

    for (i = 0u; i < COM_SOC_STATE_COUNT; i++)
    {
        ph[i] = (s_soc.p[i][0] * h[0]) +
                (s_soc.p[i][1] * h[1]) +
                (s_soc.p[i][2] * h[2]);
    }
    s = r_v +
        (h[0] * ph[0]) +
        (h[1] * ph[1]) +
        (h[2] * ph[2]);
    if ((s <= 0.0f) || !Com_SOC_IsFiniteFloat(s))
    {
        return;
    }

    soc_clamped = false;
    for (i = 0u; i < COM_SOC_STATE_COUNT; i++)
    {
        k[i] = ph[i] / s;
        s_soc.x[i] += k[i] * y;
    }
    s_soc.result.kalman_gain_soc = k[COM_SOC_INDEX_SOC];
    s_soc.result.kalman_gain_v1 = k[COM_SOC_INDEX_V1];
    s_soc.result.kalman_gain_v2 = k[COM_SOC_INDEX_V2];
    if ((s_soc.x[COM_SOC_INDEX_SOC] < 0.0f) ||
        (s_soc.x[COM_SOC_INDEX_SOC] > 1.0f))
    {
        soc_clamped = true;
    }
    Com_SOC_ProtectState();

    for (row = 0u; row < COM_SOC_STATE_COUNT; row++)
    {
        for (col = 0u; col < COM_SOC_STATE_COUNT; col++)
        {
            i_kh[row][col] = (row == col) ? 1.0f : 0.0f;
            i_kh[row][col] -= k[row] * h[col];
        }
    }
    for (row = 0u; row < COM_SOC_STATE_COUNT; row++)
    {
        for (col = 0u; col < COM_SOC_STATE_COUNT; col++)
        {
            temp_p[row][col] =
                (i_kh[row][0] * s_soc.p[0][col]) +
                (i_kh[row][1] * s_soc.p[1][col]) +
                (i_kh[row][2] * s_soc.p[2][col]);
        }
    }
    for (row = 0u; row < COM_SOC_STATE_COUNT; row++)
    {
        for (col = 0u; col < COM_SOC_STATE_COUNT; col++)
        {
            new_p[row][col] =
                (temp_p[row][0] * i_kh[col][0]) +
                (temp_p[row][1] * i_kh[col][1]) +
                (temp_p[row][2] * i_kh[col][2]) +
                (k[row] * r_v * k[col]);
        }
    }
    for (row = 0u; row < COM_SOC_STATE_COUNT; row++)
    {
        for (col = 0u; col < COM_SOC_STATE_COUNT; col++)
        {
            s_soc.p[row][col] = new_p[row][col];
        }
    }
    Com_SOC_ProtectCovariance();
    if (soc_clamped)
    {
        Com_SOC_TightenSocCovariance();
    }

    s_soc.result.voltage_update_used = true;
    s_soc.result.dynamic_ekf_used = !s_soc.result.rest_ready;
}

static void Com_SOC_AnchorByVoltage(const Com_SOC_SampleTypeDef *sample, int32_t current_ma)
{
    bool full_condition;
    bool empty_condition;

    if (!Com_SOC_IsCellSampleValid(sample))
    {
        return;
    }

    full_condition = (sample->cell_max_mv >= s_soc.config.full_cell_mv) &&
                     (current_ma > 0) &&
                     (Com_SOC_AbsI32(current_ma) <= (uint32_t)s_soc.config.full_current_ma);
    empty_condition = (sample->cell_min_mv <= s_soc.config.empty_cell_mv) &&
                      (current_ma < 0) &&
                      (Com_SOC_AbsI32(current_ma) <= (uint32_t)s_soc.config.empty_current_ma);

    if (full_condition && empty_condition)
    {
        s_soc.full_anchor_ms = 0u;
        s_soc.empty_anchor_ms = 0u;
        return;
    }

    if (full_condition)
    {
        s_soc.full_anchor_ms =
            Com_SOC_AddMs(s_soc.full_anchor_ms, sample->interval_ms);
    }
    else
    {
        s_soc.full_anchor_ms = 0u;
    }

    if (empty_condition)
    {
        s_soc.empty_anchor_ms =
            Com_SOC_AddMs(s_soc.empty_anchor_ms, sample->interval_ms);
    }
    else
    {
        s_soc.empty_anchor_ms = 0u;
    }

    if (s_soc.full_anchor_ms >= s_soc.config.full_anchor_hold_ms)
    {
        s_soc.x[COM_SOC_INDEX_SOC] = 1.0f;
        s_soc.result.full_anchor_used = true;
        Com_SOC_TightenSocCovariance();
    }
    else if (s_soc.empty_anchor_ms >= s_soc.config.empty_anchor_hold_ms)
    {
        s_soc.x[COM_SOC_INDEX_SOC] = 0.0f;
        s_soc.result.empty_anchor_used = true;
        Com_SOC_TightenSocCovariance();
    }
}

static void Com_SOC_UpdateDisplay(uint32_t interval_ms)
{
    float target = s_soc.x[COM_SOC_INDEX_SOC] * 100.0f;
    float max_step;
    float diff = target - s_soc.result.display_percent;

    if (interval_ms == 0u)
    {
        s_soc.result.display_percent = target;
        return;
    }

    if (diff >= 0.0f)
    {
        max_step = s_soc.config.display_rise_percent_per_s * ((float)interval_ms / 1000.0f);
        if (diff > max_step)
        {
            diff = max_step;
        }
    }
    else
    {
        max_step = s_soc.config.display_fall_percent_per_s * ((float)interval_ms / 1000.0f);
        if ((-diff) > max_step)
        {
            diff = -max_step;
        }
    }

    s_soc.result.display_percent =
        Com_SOC_ClampFloat(s_soc.result.display_percent + diff, 0.0f, 100.0f);
}

static void Com_SOC_UpdateConfidence(const Com_SOC_SampleTypeDef *sample)
{
    uint8_t confidence = 40u;

    if (s_soc.result.seeded)
    {
        confidence = (uint8_t)(confidence + 20u);
    }
    if ((sample != 0) && sample->current_valid)
    {
        confidence = (uint8_t)(confidence + 15u);
    }
    if ((sample != 0) && sample->cells_valid)
    {
        confidence = (uint8_t)(confidence + 15u);
    }
    if ((sample != 0) && sample->temp_valid)
    {
        confidence = (uint8_t)(confidence + 5u);
    }
    if ((sample != 0) && sample->soh_valid)
    {
        confidence = (uint8_t)(confidence + 5u);
    }
    if (s_soc.result.voltage_update_used)
    {
        confidence = (uint8_t)(confidence + 10u);
    }
    if (confidence > 100u)
    {
        confidence = 100u;
    }
    s_soc.result.confidence_percent = confidence;
}

void Com_SOC_Init(const Com_SOC_ConfigTypeDef *config)
{
    Com_SOC_FillDefaultConfig(&s_soc.config);
    if (config != 0)
    {
        s_soc.config = *config;
    }
    Com_SOC_CheckConfig(&s_soc.config);

    s_soc.x[COM_SOC_INDEX_SOC] = s_soc.config.default_soc_percent / 100.0f;
    s_soc.x[COM_SOC_INDEX_V1] = 0.0f;
    s_soc.x[COM_SOC_INDEX_V2] = 0.0f;
    s_soc.rest_ms = 0u;
    s_soc.voltage_stable_ms = 0u;
    s_soc.last_cell_avg_mv = 0u;
    s_soc.rest_start_cell_avg_mv = 0u;
    s_soc.full_anchor_ms = 0u;
    s_soc.empty_anchor_ms = 0u;
    s_soc.last_current_ma = 0;
    s_soc.current_stable_ms = 0u;

    s_soc.result.seeded = false;
    s_soc.result.seed_source = COM_SOC_SEED_NONE;
    s_soc.result.rest_ready = false;
    s_soc.result.voltage_update_used = false;
    s_soc.result.full_anchor_used = false;
    s_soc.result.empty_anchor_used = false;
    s_soc.result.dynamic_ekf_used = false;
    s_soc.result.confidence_percent = 0u;
    s_soc.result.display_percent = s_soc.config.default_soc_percent;
    s_soc.result.model_cell_mv = 0.0f;
    s_soc.result.ocv_cell_mv = 0.0f;
    s_soc.result.polar_v1_mv = 0.0f;
    s_soc.result.polar_v2_mv = 0.0f;
    s_soc.result.residual_mv = 0.0f;
    s_soc.result.kalman_gain_soc = 0.0f;
    s_soc.result.kalman_gain_v1 = 0.0f;
    s_soc.result.kalman_gain_v2 = 0.0f;
    s_soc.result.p_soc = 0.0f;
    s_soc.result.p_v1 = 0.0f;
    s_soc.result.p_v2 = 0.0f;
    s_soc.result.active_capacity_mah = (float)s_soc.config.capacity_mah;
    s_soc.result.measured_cell_mv = 0.0f;
    s_soc.result.ir0_mv = 0.0f;
    s_soc.result.soc_raw_percent = s_soc.config.default_soc_percent;
    s_soc.result.rest_ms = 0u;
    s_soc.result.voltage_stable_ms = 0u;
    s_soc.result.current_stable_ms = 0u;
    s_soc.result.ekf_used_avg_voltage = false;
    Com_SOC_ResetCovariance();
    Com_SOC_UpdatePublicResult();
}

void Com_SOC_Update(const Com_SOC_SampleTypeDef *sample)
{
    int32_t current_ma = 0;
    float capacity_mah;

    if (sample == 0)
    {
        return;
    }

    s_soc.result.voltage_update_used = false;
    s_soc.result.full_anchor_used = false;
    s_soc.result.empty_anchor_used = false;
    s_soc.result.dynamic_ekf_used = false;
    s_soc.result.ekf_used_avg_voltage = false;
    s_soc.result.residual_mv = 0.0f;
    s_soc.result.kalman_gain_soc = 0.0f;
    s_soc.result.kalman_gain_v1 = 0.0f;
    s_soc.result.kalman_gain_v2 = 0.0f;

    if (Com_SOC_IsCellSampleValid(sample))
    {
        Com_SOC_SeedByVoltage(sample->cell_avg_mv);
    }

    if (!s_soc.result.seeded)
    {
        s_soc.rest_ms = 0u;
        s_soc.voltage_stable_ms = 0u;
        s_soc.rest_start_cell_avg_mv = 0u;
        s_soc.last_cell_avg_mv = 0u;
        s_soc.full_anchor_ms = 0u;
        s_soc.empty_anchor_ms = 0u;
        s_soc.last_current_ma = 0;
        s_soc.current_stable_ms = 0u;
        s_soc.result.rest_ready = false;
        Com_SOC_UpdateDisplay(sample->interval_ms);
        Com_SOC_UpdatePublicResult();
        Com_SOC_UpdateConfidence(sample);
        return;
    }

    if (!sample->current_valid)
    {
        s_soc.rest_ms = 0u;
        s_soc.voltage_stable_ms = 0u;
        s_soc.rest_start_cell_avg_mv = 0u;
        s_soc.last_cell_avg_mv = 0u;
        s_soc.full_anchor_ms = 0u;
        s_soc.empty_anchor_ms = 0u;
        s_soc.last_current_ma = 0;
        s_soc.current_stable_ms = 0u;
        s_soc.result.rest_ready = false;
        Com_SOC_UpdateDisplay(sample->interval_ms);
        Com_SOC_UpdatePublicResult();
        Com_SOC_UpdateConfidence(sample);
        return;
    }

    if (sample->current_valid)
    {
        current_ma = Com_SOC_NormalizeCurrentMa(sample->current_ma);
    }

    capacity_mah = Com_SOC_GetCapacityMah(sample);
    Com_SOC_Predict(sample->interval_ms, current_ma, capacity_mah);
    Com_SOC_UpdateCurrentStable(sample->interval_ms, current_ma);
    Com_SOC_UpdateRestState(sample, current_ma);
    Com_SOC_UpdateVoltageModelDebug(sample, current_ma);
    Com_SOC_EkfVoltageUpdate(sample, current_ma);
    Com_SOC_AnchorByVoltage(sample, current_ma);
    Com_SOC_UpdateDisplay(sample->interval_ms);
    Com_SOC_UpdatePublicResult();
    Com_SOC_UpdateConfidence(sample);
}

void Com_SOC_GetResult(Com_SOC_ResultTypeDef *result)
{
    if (result == 0)
    {
        return;
    }

    *result = s_soc.result;
}

float Com_SOC_GetSocPercent(void)
{
    return s_soc.result.soc_percent;
}

float Com_SOC_GetDisplayPercent(void)
{
    return s_soc.result.display_percent;
}

uint32_t Com_SOC_GetRemainMah(void)
{
    return s_soc.result.remain_mah;
}

bool Com_SOC_IsSeeded(void)
{
    return s_soc.result.seeded;
}
