#include "Com_SOH.h"

#include "Com_BatteryParam.h"

typedef struct
{
    Com_SOH_ConfigTypeDef config;
    Com_SOH_ResultTypeDef result;
    float cycle_bucket_mah;
    float charge_bucket_mah;
    float discharge_bucket_mah;
} Com_SOH_StateTypeDef;

static Com_SOH_StateTypeDef s_soh;

static uint32_t Com_SOH_AbsCurrentMa(int32_t current_ma)
{
    if (current_ma < 0)
    {
        return (uint32_t)(-current_ma);
    }
    return (uint32_t)current_ma;
}

static void Com_SOH_FillDefaultConfig(Com_SOH_ConfigTypeDef *config)
{
    config->capacity_mah = COM_BATTERY_PARAM_CAP_TYP_MAH;
    config->delta_warn_mv = 80u;
    config->temp_warn_c = 55;
    config->cycle_warn_count = 300u;
}

static void Com_SOH_CheckConfig(Com_SOH_ConfigTypeDef *config)
{
    if (config->capacity_mah == 0u)
    {
        config->capacity_mah = COM_BATTERY_PARAM_CAP_TYP_MAH;
    }
    if (config->cycle_warn_count == 0u)
    {
        config->cycle_warn_count = 300u;
    }
}

static void Com_SOH_AddWholeMah(float *bucket, uint32_t *total)
{
    uint32_t whole_mah = (uint32_t)(*bucket);

    if (whole_mah > 0u)
    {
        *total += whole_mah;
        *bucket -= (float)whole_mah;
    }
}

static void Com_SOH_UpdateScore(void)
{
    uint8_t score = 100u;
    uint8_t confidence = 60u;

    if (s_soh.result.max_delta_mv >= s_soh.config.delta_warn_mv)
    {
        score = (uint8_t)(score - 5u);
    }
    if (s_soh.result.max_temp_c >= s_soh.config.temp_warn_c)
    {
        score = (uint8_t)(score - 5u);
    }
    if (s_soh.result.safety_fault_count > 0u)
    {
        score = (uint8_t)(score - 10u);
    }
    if (s_soh.result.cycle_count >= s_soh.config.cycle_warn_count)
    {
        score = (uint8_t)(score - 5u);
    }

    /*
     * 热敏未接是当前硬件验证阶段的真实情况。它不直接扣 SOH，
     * 只降低可信度，避免把 -273C 或兜底 IC 温度当作电芯老化证据。
     */
    if (s_soh.result.temp_invalid_count > 0u)
    {
        confidence = (uint8_t)(confidence - 20u);
    }
    if (s_soh.result.discharge_throughput_mah >= s_soh.config.capacity_mah)
    {
        confidence = (uint8_t)(confidence + 20u);
    }
    if (s_soh.result.cycle_count > 0u)
    {
        confidence = (uint8_t)(confidence + 20u);
    }
    if (confidence > 100u)
    {
        confidence = 100u;
    }

    s_soh.result.soh_percent = score;
    s_soh.result.confidence_percent = confidence;
}

void Com_SOH_Init(const Com_SOH_ConfigTypeDef *config)
{
    Com_SOH_FillDefaultConfig(&s_soh.config);
    if (config != 0)
    {
        s_soh.config = *config;
    }
    Com_SOH_CheckConfig(&s_soh.config);

    s_soh.result.charge_throughput_mah = 0u;
    s_soh.result.discharge_throughput_mah = 0u;
    s_soh.result.cycle_count = 0u;
    s_soh.result.safety_fault_count = 0u;
    s_soh.result.temp_invalid_count = 0u;
    s_soh.result.max_delta_mv = 0u;
    s_soh.result.max_temp_c = 0;
    s_soh.result.soh_percent = 100u;
    s_soh.result.confidence_percent = 60u;
    s_soh.cycle_bucket_mah = 0.0f;
    s_soh.charge_bucket_mah = 0.0f;
    s_soh.discharge_bucket_mah = 0.0f;
}

void Com_SOH_Update(const Com_SOH_SampleTypeDef *sample)
{
    uint32_t abs_ma;
    float delta_mah;

    if (sample == 0)
    {
        return;
    }

    if (sample->cells_valid && (sample->cell_delta_mv > s_soh.result.max_delta_mv))
    {
        s_soh.result.max_delta_mv = sample->cell_delta_mv;
    }

    if (sample->temp_cell_valid)
    {
        if (sample->temp_cell_c > s_soh.result.max_temp_c)
        {
            s_soh.result.max_temp_c = sample->temp_cell_c;
        }
    }
    else
    {
        s_soh.result.temp_invalid_count++;
    }

    if ((sample->safety_status_a != 0u) ||
        (sample->safety_status_b != 0u) ||
        (sample->safety_status_c != 0u))
    {
        s_soh.result.safety_fault_count++;
    }

    if (sample->current_valid && (sample->interval_ms > 0u))
    {
        abs_ma = Com_SOH_AbsCurrentMa(sample->current_ma);
        delta_mah = ((float)abs_ma * (float)sample->interval_ms) / 3600000.0f;

        if (sample->current_ma > 0)
        {
            s_soh.charge_bucket_mah += delta_mah;
            Com_SOH_AddWholeMah(&s_soh.charge_bucket_mah,
                                &s_soh.result.charge_throughput_mah);
        }
        else if (sample->current_ma < 0)
        {
            s_soh.discharge_bucket_mah += delta_mah;
            Com_SOH_AddWholeMah(&s_soh.discharge_bucket_mah,
                                &s_soh.result.discharge_throughput_mah);

            s_soh.cycle_bucket_mah += delta_mah;
            while (s_soh.cycle_bucket_mah >= (float)s_soh.config.capacity_mah)
            {
                s_soh.cycle_bucket_mah -= (float)s_soh.config.capacity_mah;
                s_soh.result.cycle_count++;
            }
        }
    }

    Com_SOH_UpdateScore();
}

void Com_SOH_GetResult(Com_SOH_ResultTypeDef *result)
{
    if (result == 0)
    {
        return;
    }

    *result = s_soh.result;
}
