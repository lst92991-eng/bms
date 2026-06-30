#include "Com_SOC.h"

#include "Com_BatteryParam.h"

#include <limits.h>

#define COM_SOC_CELL_MIN_VALID_MV        (2500u)
#define COM_SOC_CELL_MAX_VALID_MV        (4350u)
#define COM_SOC_P_MIN                    (0.0001f)
#define COM_SOC_P_MAX                    (400.0f)

typedef struct
{
    Com_SOC_ConfigTypeDef config;
    Com_SOC_ResultTypeDef result;
    float soc_percent;
    float p_soc;
    uint32_t rest_ms;
    uint32_t voltage_stable_ms;
    uint16_t rest_start_cell_avg_mv;
    uint32_t full_anchor_ms;
    uint32_t empty_anchor_ms;
} Com_SOC_StateTypeDef;

static Com_SOC_StateTypeDef s_soc;

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

static uint32_t Com_SOC_AddMs(uint32_t old_ms, uint32_t add_ms)
{
    if ((0xFFFFFFFFu - old_ms) > add_ms)
    {
        return old_ms + add_ms;
    }
    return 0xFFFFFFFFu;
}

static bool Com_SOC_IsCellSampleValid(const Com_SOC_SampleTypeDef *sample)
{
    if ((sample == 0) || !sample->cells_valid)
    {
        return false;
    }
    if ((sample->cell_min_mv < COM_SOC_CELL_MIN_VALID_MV) ||
        (sample->cell_avg_mv < COM_SOC_CELL_MIN_VALID_MV) ||
        (sample->cell_max_mv < COM_SOC_CELL_MIN_VALID_MV) ||
        (sample->cell_min_mv > COM_SOC_CELL_MAX_VALID_MV) ||
        (sample->cell_avg_mv > COM_SOC_CELL_MAX_VALID_MV) ||
        (sample->cell_max_mv > COM_SOC_CELL_MAX_VALID_MV))
    {
        return false;
    }
    if ((sample->cell_min_mv > sample->cell_avg_mv) ||
        (sample->cell_avg_mv > sample->cell_max_mv))
    {
        return false;
    }
    return true;
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

    config->default_soc_percent =
        Com_SOC_ClampFloat(config->default_soc_percent, 0.0f, 100.0f);
    if ((config->current_sign != COM_SOC_CURRENT_POS_CHARGE) &&
        (config->current_sign != COM_SOC_CURRENT_POS_DISCHARGE))
    {
        config->current_sign = COM_SOC_CURRENT_POS_CHARGE;
    }

    config->current_deadband_ma = Com_SOC_MakePositiveI32(config->current_deadband_ma);
    config->rest_current_ma = Com_SOC_MakePositiveI32(config->rest_current_ma);
    if (config->rest_need_ms == 0u)
    {
        config->rest_need_ms = 600000u;
    }
    if (config->rest_voltage_stable_mv == 0u)
    {
        config->rest_voltage_stable_mv = 3u;
    }

    config->kalman_q = Com_SOC_ClampFloat(config->kalman_q, 0.000001f, 1.0f);
    config->kalman_r = Com_SOC_ClampFloat(config->kalman_r, 0.1f, 100.0f);
    config->ocv_update_limit_percent =
        Com_SOC_ClampFloat(config->ocv_update_limit_percent, 0.1f, 20.0f);

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
    if (config->empty_cell_mv >= config->full_cell_mv)
    {
        config->empty_cell_mv = 3000u;
        config->full_cell_mv = COM_BATTERY_PARAM_CELL_FULL_MV;
    }

    config->display_rise_percent_per_s =
        Com_SOC_ClampFloat(config->display_rise_percent_per_s, 0.1f, 100.0f);
    config->display_fall_percent_per_s =
        Com_SOC_ClampFloat(config->display_fall_percent_per_s, 0.1f, 100.0f);
}

static void Com_SOC_UpdatePublicResult(void)
{
    s_soc.soc_percent = Com_SOC_ClampFloat(s_soc.soc_percent, 0.0f, 100.0f);
    s_soc.p_soc = Com_SOC_ClampFloat(s_soc.p_soc, COM_SOC_P_MIN, COM_SOC_P_MAX);

    s_soc.result.soc_percent = s_soc.soc_percent;
    s_soc.result.soc_raw_percent = s_soc.soc_percent;
    s_soc.result.remain_mah =
        (uint32_t)((s_soc.soc_percent * s_soc.result.active_capacity_mah / 100.0f) + 0.5f);
    s_soc.result.p_soc = s_soc.p_soc;
    s_soc.result.rest_ms = s_soc.rest_ms;
    s_soc.result.voltage_stable_ms = s_soc.voltage_stable_ms;
}

static void Com_SOC_SeedByVoltage(uint16_t cell_avg_mv)
{
    uint16_t soc_0p01;

    if (s_soc.result.seeded ||
        (cell_avg_mv < COM_SOC_CELL_MIN_VALID_MV) ||
        (cell_avg_mv > COM_SOC_CELL_MAX_VALID_MV))
    {
        return;
    }

    soc_0p01 = Com_BatteryParam_GetSoc0p01ByVoltage(cell_avg_mv);
    s_soc.soc_percent = (float)soc_0p01 / 100.0f;
    s_soc.result.display_percent = s_soc.soc_percent;
    s_soc.result.seeded = true;
    s_soc.result.seed_source = COM_SOC_SEED_OCV;
    s_soc.p_soc = 25.0f;
    Com_SOC_UpdatePublicResult();
}

static void Com_SOC_Predict(uint32_t interval_ms, int32_t current_ma, float capacity_mah)
{
    float dt_s;
    float soc_delta;

    if (interval_ms == 0u)
    {
        return;
    }

    dt_s = (float)interval_ms / 1000.0f;
    soc_delta = ((float)current_ma * dt_s) / (capacity_mah * 36.0f);
    s_soc.soc_percent =
        Com_SOC_ClampFloat(s_soc.soc_percent + soc_delta, 0.0f, 100.0f);
    s_soc.p_soc += s_soc.config.kalman_q * dt_s;
}

static void Com_SOC_UpdateRestState(const Com_SOC_SampleTypeDef *sample, int32_t current_ma)
{
    uint16_t diff_mv;

    if (Com_SOC_AbsI32(current_ma) > (uint32_t)s_soc.config.rest_current_ma)
    {
        s_soc.rest_ms = 0u;
        s_soc.voltage_stable_ms = 0u;
        s_soc.rest_start_cell_avg_mv = 0u;
        s_soc.result.rest_ready = false;
        return;
    }

    s_soc.rest_ms = Com_SOC_AddMs(s_soc.rest_ms, sample->interval_ms);
    if (!Com_SOC_IsCellSampleValid(sample))
    {
        s_soc.voltage_stable_ms = 0u;
        s_soc.result.rest_ready = false;
        return;
    }

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

    s_soc.result.rest_ready =
        (s_soc.rest_ms >= s_soc.config.rest_need_ms) &&
        (s_soc.voltage_stable_ms >= s_soc.config.rest_need_ms);
}

static void Com_SOC_KalmanByOcv(const Com_SOC_SampleTypeDef *sample)
{
    uint16_t soc_0p01;
    float ocv_soc_percent;
    float residual;
    float gain;
    float correction;

    s_soc.result.voltage_update_used = false;
    if (!s_soc.result.rest_ready || !Com_SOC_IsCellSampleValid(sample))
    {
        return;
    }

    soc_0p01 = Com_BatteryParam_GetSoc0p01ByVoltage(sample->cell_avg_mv);
    ocv_soc_percent = (float)soc_0p01 / 100.0f;
    residual = ocv_soc_percent - s_soc.soc_percent;

    if ((residual > 30.0f) || (residual < -30.0f))
    {
        return;
    }

    gain = s_soc.p_soc / (s_soc.p_soc + s_soc.config.kalman_r);
    correction = gain * residual;
    correction = Com_SOC_ClampFloat(correction,
                                    -s_soc.config.ocv_update_limit_percent,
                                    s_soc.config.ocv_update_limit_percent);

    s_soc.soc_percent =
        Com_SOC_ClampFloat(s_soc.soc_percent + correction, 0.0f, 100.0f);
    s_soc.p_soc = (1.0f - gain) * s_soc.p_soc;

    s_soc.result.ocv_soc_percent = ocv_soc_percent;
    s_soc.result.ocv_cell_mv = (float)sample->cell_avg_mv;
    s_soc.result.measured_cell_mv = (float)sample->cell_avg_mv;
    s_soc.result.residual_percent = residual;
    s_soc.result.kalman_gain_soc = gain;
    s_soc.result.voltage_update_used = true;
}

static void Com_SOC_AnchorByVoltage(const Com_SOC_SampleTypeDef *sample, int32_t current_ma)
{
    bool full_condition;
    bool empty_condition;

    s_soc.result.full_anchor_used = false;
    s_soc.result.empty_anchor_used = false;
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

    s_soc.full_anchor_ms = full_condition ?
                           Com_SOC_AddMs(s_soc.full_anchor_ms, sample->interval_ms) :
                           0u;
    s_soc.empty_anchor_ms = empty_condition ?
                            Com_SOC_AddMs(s_soc.empty_anchor_ms, sample->interval_ms) :
                            0u;

    if (s_soc.full_anchor_ms >= s_soc.config.full_anchor_hold_ms)
    {
        s_soc.soc_percent = 100.0f;
        s_soc.p_soc = 1.0f;
        s_soc.result.full_anchor_used = true;
    }
    else if (s_soc.empty_anchor_ms >= s_soc.config.empty_anchor_hold_ms)
    {
        s_soc.soc_percent = 0.0f;
        s_soc.p_soc = 1.0f;
        s_soc.result.empty_anchor_used = true;
    }
}

static void Com_SOC_UpdateDisplay(uint32_t interval_ms)
{
    float target = s_soc.soc_percent;
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
    s_soc.config.capacity_mah = COM_BATTERY_PARAM_CAP_TYP_MAH;
    s_soc.config.default_soc_percent = 50.0f;
    s_soc.config.current_sign = COM_SOC_CURRENT_POS_CHARGE;
    s_soc.config.current_deadband_ma = 30;
    s_soc.config.rest_need_ms = 600000u;
    s_soc.config.rest_current_ma = 100;
    s_soc.config.rest_voltage_stable_mv = 3u;
    s_soc.config.kalman_q = 0.002f;
    s_soc.config.kalman_r = 9.0f;
    s_soc.config.ocv_update_limit_percent = 3.0f;
    s_soc.config.full_cell_mv = COM_BATTERY_PARAM_CELL_FULL_MV;
    s_soc.config.empty_cell_mv = 3000u;
    s_soc.config.full_current_ma = 250;
    s_soc.config.empty_current_ma = 250;
    s_soc.config.full_anchor_hold_ms = 5000u;
    s_soc.config.empty_anchor_hold_ms = 5000u;
    s_soc.config.display_rise_percent_per_s = 1.5f;
    s_soc.config.display_fall_percent_per_s = 2.5f;
    if (config != 0)
    {
        s_soc.config = *config;
    }
    Com_SOC_CheckConfig(&s_soc.config);

    s_soc.soc_percent = s_soc.config.default_soc_percent;
    s_soc.p_soc = 25.0f;
    s_soc.rest_ms = 0u;
    s_soc.voltage_stable_ms = 0u;
    s_soc.rest_start_cell_avg_mv = 0u;
    s_soc.full_anchor_ms = 0u;
    s_soc.empty_anchor_ms = 0u;

    s_soc.result.soc_percent = s_soc.soc_percent;
    s_soc.result.display_percent = s_soc.soc_percent;
    s_soc.result.remain_mah = 0u;
    s_soc.result.seeded = false;
    s_soc.result.seed_source = COM_SOC_SEED_NONE;
    s_soc.result.rest_ready = false;
    s_soc.result.voltage_update_used = false;
    s_soc.result.full_anchor_used = false;
    s_soc.result.empty_anchor_used = false;
    s_soc.result.confidence_percent = 0u;
    s_soc.result.ocv_soc_percent = 0.0f;
    s_soc.result.ocv_cell_mv = 0.0f;
    s_soc.result.residual_percent = 0.0f;
    s_soc.result.kalman_gain_soc = 0.0f;
    s_soc.result.p_soc = s_soc.p_soc;
    s_soc.result.active_capacity_mah = (float)s_soc.config.capacity_mah;
    s_soc.result.measured_cell_mv = 0.0f;
    s_soc.result.soc_raw_percent = s_soc.soc_percent;
    s_soc.result.rest_ms = 0u;
    s_soc.result.voltage_stable_ms = 0u;
    Com_SOC_UpdatePublicResult();
}

void Com_SOC_Update(const Com_SOC_SampleTypeDef *sample)
{
    int32_t current_ma;
    float capacity_mah;

    if (sample == 0)
    {
        return;
    }

    s_soc.result.voltage_update_used = false;
    s_soc.result.full_anchor_used = false;
    s_soc.result.empty_anchor_used = false;
    s_soc.result.residual_percent = 0.0f;
    s_soc.result.kalman_gain_soc = 0.0f;

    if (Com_SOC_IsCellSampleValid(sample))
    {
        Com_SOC_SeedByVoltage(sample->cell_avg_mv);
    }

    if (!s_soc.result.seeded || !sample->current_valid)
    {
        s_soc.rest_ms = 0u;
        s_soc.voltage_stable_ms = 0u;
        s_soc.rest_start_cell_avg_mv = 0u;
        s_soc.result.rest_ready = false;
        Com_SOC_UpdateDisplay(sample->interval_ms);
        Com_SOC_UpdatePublicResult();
        Com_SOC_UpdateConfidence(sample);
        return;
    }

    current_ma = sample->current_ma;
    if (Com_SOC_AbsI32(current_ma) <= (uint32_t)s_soc.config.current_deadband_ma)
    {
        current_ma = 0;
    }
    else if (s_soc.config.current_sign == COM_SOC_CURRENT_POS_DISCHARGE)
    {
        current_ma = (current_ma == INT32_MIN) ? INT32_MAX : -current_ma;
    }

    capacity_mah = (float)s_soc.config.capacity_mah;
    if (sample->soh_valid && (sample->soh_percent >= 50u))
    {
        capacity_mah *= (float)sample->soh_percent / 100.0f;
    }
    if (sample->temp_valid && (sample->temp_c < 0))
    {
        capacity_mah *= 0.90f;
    }
    if (capacity_mah < 1.0f)
    {
        capacity_mah = 1.0f;
    }
    s_soc.result.active_capacity_mah = capacity_mah;

    Com_SOC_Predict(sample->interval_ms, current_ma, capacity_mah);
    Com_SOC_UpdateRestState(sample, current_ma);
    Com_SOC_KalmanByOcv(sample);
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
