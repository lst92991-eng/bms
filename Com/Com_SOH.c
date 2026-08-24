#include "Com_SOH.h"

#include "Com_BatteryParam.h"

typedef struct
{
    Com_SOH_ConfigTypeDef config;
    Com_SOH_ResultTypeDef result;
    float cycle_bucket_mah;
    float charge_bucket_mah;
    float discharge_bucket_mah;
    float learning_net_discharge_mah;
    bool safety_active_last;
    bool full_anchor_last;
    bool empty_anchor_last;
} Com_SOH_StateTypeDef;

static Com_SOH_StateTypeDef s_soh;

static uint32_t Com_SOH_AbsCurrentMa(int32_t current_ma)
{
    if (current_ma < 0)
    {
        /* 先加一再取反，避免 INT32_MIN 直接取负产生有符号溢出。 */
        return (uint32_t)(-(current_ma + 1)) + 1u;
    }
    return (uint32_t)current_ma;
}

static uint32_t Com_SOH_AddU32Saturated(uint32_t value, uint32_t add)
{
    if (value > (UINT32_MAX - add))
    {
        return UINT32_MAX;
    }
    return value + add;
}

static uint32_t Com_SOH_AddU64Saturated(uint32_t value, uint64_t add)
{
    if ((add >= UINT32_MAX) || (value > (UINT32_MAX - (uint32_t)add)))
    {
        return UINT32_MAX;
    }
    return value + (uint32_t)add;
}

static void Com_SOH_CalculateDeltaMah(uint32_t current_ma,
                                      uint32_t interval_ms,
                                      uint64_t *whole_mah,
                                      float *fraction_mah)
{
    const uint64_t ma_ms = (uint64_t)current_ma * (uint64_t)interval_ms;

    *whole_mah = ma_ms / 3600000u;
    *fraction_mah = (float)(ma_ms % 3600000u) / 3600000.0f;
}

static void Com_SOH_AccumulateThroughput(uint64_t whole_mah,
                                         float fraction_mah,
                                         float *fraction_bucket,
                                         uint32_t *total_mah)
{
    fraction_mah += *fraction_bucket;
    if (fraction_mah >= 1.0f)
    {
        whole_mah++;
        fraction_mah -= 1.0f;
    }
    *fraction_bucket = fraction_mah;
    *total_mah = Com_SOH_AddU64Saturated(*total_mah, whole_mah);
}

static void Com_SOH_AccumulateCycles(uint64_t whole_mah, float fraction_mah)
{
    const uint32_t capacity_mah = s_soh.config.capacity_mah;
    uint32_t bucket_whole_mah = (uint32_t)s_soh.cycle_bucket_mah;
    float bucket_fraction_mah = s_soh.cycle_bucket_mah - (float)bucket_whole_mah;
    uint64_t combined_whole_mah;
    uint64_t completed_cycles;

    bucket_fraction_mah += fraction_mah;
    if (bucket_fraction_mah >= 1.0f)
    {
        bucket_whole_mah++;
        bucket_fraction_mah -= 1.0f;
    }

    combined_whole_mah = whole_mah + bucket_whole_mah;
    completed_cycles = combined_whole_mah / capacity_mah;
    bucket_whole_mah = (uint32_t)(combined_whole_mah % capacity_mah);
    s_soh.cycle_bucket_mah = (float)bucket_whole_mah + bucket_fraction_mah;
    s_soh.result.cycle_count = Com_SOH_AddU64Saturated(s_soh.result.cycle_count, completed_cycles);
}

static void Com_SOH_UpdateScore(void)
{
    uint8_t score = 100u;
    uint8_t confidence = 0u;
    uint32_t soh_percent;

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

    if (s_soh.result.capacity_learning_count > 0u)
    {
        confidence = 80u;
    }
    if (s_soh.result.capacity_learning_count > 1u)
    {
        confidence = 90u;
    }
    if (s_soh.result.capacity_learning_count > 2u)
    {
        confidence = 100u;
    }

    if (s_soh.result.capacity_valid)
    {
        soh_percent = (uint32_t)((((uint64_t)s_soh.result.learned_capacity_mah * 100u) +
                                  (s_soh.config.capacity_mah / 2u)) /
                                 s_soh.config.capacity_mah);
        if (soh_percent > 100u)
        {
            soh_percent = 100u;
        }
        s_soh.result.soh_percent = (uint8_t)soh_percent;
    }
    else
    {
        s_soh.result.soh_percent = 0u;
    }

    s_soh.result.health_score_percent = score;
    s_soh.result.confidence_percent = confidence;
}

static uint32_t Com_SOH_LearnMinMah(void)
{
    return (uint32_t)(((uint64_t)s_soh.config.capacity_mah * s_soh.config.learn_min_percent) /
                      100u);
}

static uint32_t Com_SOH_LearnMaxMah(void)
{
    return (uint32_t)(((uint64_t)s_soh.config.capacity_mah * s_soh.config.learn_max_percent) /
                      100u);
}

static void Com_SOH_CancelCapacityLearning(void)
{
    s_soh.learning_net_discharge_mah = 0.0f;
    s_soh.result.learning_net_discharge_mah = 0u;
    s_soh.result.learning_active = false;
}

static void Com_SOH_StartCapacityLearning(void)
{
    s_soh.learning_net_discharge_mah = 0.0f;
    s_soh.result.learning_net_discharge_mah = 0u;
    s_soh.result.learning_active = true;
}

static void Com_SOH_FinishCapacityLearning(void)
{
    uint32_t candidate_mah;

    if (!s_soh.result.learning_active)
    {
        return;
    }

    if (s_soh.learning_net_discharge_mah <= 0.0f)
    {
        Com_SOH_CancelCapacityLearning();
        return;
    }

    candidate_mah = (uint32_t)(s_soh.learning_net_discharge_mah + 0.5f);
    s_soh.result.learning_active = false;

    if ((candidate_mah < Com_SOH_LearnMinMah()) || (candidate_mah > Com_SOH_LearnMaxMah()))
    {
        return;
    }

    if (s_soh.result.capacity_valid)
    {
        s_soh.result.learned_capacity_mah =
            (uint32_t)((((uint64_t)s_soh.result.learned_capacity_mah * 3u) + candidate_mah + 2u) /
                       4u);
    }
    else
    {
        s_soh.result.learned_capacity_mah = candidate_mah;
    }

    if (s_soh.result.capacity_learning_count < UINT16_MAX)
    {
        s_soh.result.capacity_learning_count++;
    }
    s_soh.result.capacity_valid = true;
    s_soh.result.capacity_updated = true;
}

void Com_SOH_Init(const Com_SOH_ConfigTypeDef *config)
{
    s_soh.config.capacity_mah = COM_BATTERY_PARAM_CAP_TYP_MAH;
    s_soh.config.delta_warn_mv = 80u;
    s_soh.config.temp_warn_c = 55;
    s_soh.config.cycle_warn_count = 300u;
    s_soh.config.learn_min_percent = 50u;
    s_soh.config.learn_max_percent = 110u;

    if (config != 0)
    {
        s_soh.config = *config;
    }
    if (s_soh.config.capacity_mah == 0u)
    {
        s_soh.config.capacity_mah = COM_BATTERY_PARAM_CAP_TYP_MAH;
    }
    if (s_soh.config.capacity_mah > COM_SOH_CAPACITY_MAX_MAH)
    {
        s_soh.config.capacity_mah = COM_SOH_CAPACITY_MAX_MAH;
    }
    if (s_soh.config.cycle_warn_count == 0u)
    {
        s_soh.config.cycle_warn_count = 300u;
    }
    if ((s_soh.config.learn_min_percent == 0u) || (s_soh.config.learn_min_percent >= 100u))
    {
        s_soh.config.learn_min_percent = 50u;
    }
    if ((s_soh.config.learn_max_percent < 100u) || (s_soh.config.learn_max_percent > 120u) ||
        (s_soh.config.learn_max_percent <= s_soh.config.learn_min_percent))
    {
        s_soh.config.learn_max_percent = 110u;
    }

    s_soh.result.charge_throughput_mah = 0u;
    s_soh.result.discharge_throughput_mah = 0u;
    s_soh.result.cycle_count = 0u;
    s_soh.result.safety_fault_count = 0u;
    s_soh.result.temp_invalid_count = 0u;
    s_soh.result.max_delta_mv = 0u;
    s_soh.result.max_temp_c = 0;
    s_soh.result.learned_capacity_mah = 0u;
    s_soh.result.learning_net_discharge_mah = 0u;
    s_soh.result.capacity_learning_count = 0u;
    s_soh.result.soh_percent = 0u;
    s_soh.result.health_score_percent = 100u;
    s_soh.result.confidence_percent = 0u;
    s_soh.result.capacity_valid = false;
    s_soh.result.learning_active = false;
    s_soh.result.capacity_updated = false;
    s_soh.cycle_bucket_mah = 0.0f;
    s_soh.charge_bucket_mah = 0.0f;
    s_soh.discharge_bucket_mah = 0.0f;
    s_soh.learning_net_discharge_mah = 0.0f;
    s_soh.safety_active_last = false;
    s_soh.full_anchor_last = false;
    s_soh.empty_anchor_last = false;
}

void Com_SOH_Update(const Com_SOH_SampleTypeDef *sample)
{
    uint32_t abs_ma;
    uint64_t delta_whole_mah;
    float delta_fraction_mah;
    float delta_mah;
    bool safety_active;
    bool full_anchor_edge;
    bool empty_anchor_edge;

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
        s_soh.result.temp_invalid_count =
            Com_SOH_AddU32Saturated(s_soh.result.temp_invalid_count, 1u);
    }

    s_soh.result.capacity_updated = false;
    safety_active = ((sample->safety_status_a != 0u) || (sample->safety_status_b != 0u) ||
                     (sample->safety_status_c != 0u));
    if (safety_active && !s_soh.safety_active_last)
    {
        s_soh.result.safety_fault_count =
            Com_SOH_AddU32Saturated(s_soh.result.safety_fault_count, 1u);
    }
    s_soh.safety_active_last = safety_active;

    full_anchor_edge = sample->full_anchor_used && !s_soh.full_anchor_last;
    empty_anchor_edge = sample->empty_anchor_used && !s_soh.empty_anchor_last;
    s_soh.full_anchor_last = sample->full_anchor_used;
    s_soh.empty_anchor_last = sample->empty_anchor_used;

    if (full_anchor_edge)
    {
        Com_SOH_StartCapacityLearning();
    }

    /* 容量学习不能跨越电流采样缺口，否则缺失库仑量可能被误判为容量衰减。 */
    if (s_soh.result.learning_active && (sample->interval_ms > 0u) && !sample->current_valid)
    {
        Com_SOH_CancelCapacityLearning();
    }

    if (sample->current_valid && (sample->interval_ms > 0u))
    {
        abs_ma = Com_SOH_AbsCurrentMa(sample->current_ma);
        Com_SOH_CalculateDeltaMah(
            abs_ma, sample->interval_ms, &delta_whole_mah, &delta_fraction_mah);
        delta_mah = (float)delta_whole_mah + delta_fraction_mah;

        if (sample->current_ma > 0)
        {
            Com_SOH_AccumulateThroughput(delta_whole_mah,
                                         delta_fraction_mah,
                                         &s_soh.charge_bucket_mah,
                                         &s_soh.result.charge_throughput_mah);

            if (s_soh.result.learning_active)
            {
                s_soh.learning_net_discharge_mah -= delta_mah;
            }
        }
        else if (sample->current_ma < 0)
        {
            Com_SOH_AccumulateThroughput(delta_whole_mah,
                                         delta_fraction_mah,
                                         &s_soh.discharge_bucket_mah,
                                         &s_soh.result.discharge_throughput_mah);
            /* 商/余数一次完成，极端 interval/current 也不会形成数据相关循环。 */
            Com_SOH_AccumulateCycles(delta_whole_mah, delta_fraction_mah);

            if (s_soh.result.learning_active)
            {
                s_soh.learning_net_discharge_mah += delta_mah;
            }
        }
    }

    if (s_soh.result.learning_active &&
        ((s_soh.learning_net_discharge_mah > (float)Com_SOH_LearnMaxMah()) ||
         (s_soh.learning_net_discharge_mah < -(float)Com_SOH_LearnMaxMah())))
    {
        /* 超出合理容量窗口后本轮样本作废，等待下一次满电锚点重新学习。 */
        Com_SOH_CancelCapacityLearning();
    }

    if (s_soh.learning_net_discharge_mah > 0.0f)
    {
        s_soh.result.learning_net_discharge_mah =
            (s_soh.learning_net_discharge_mah >= (float)UINT32_MAX)
                ? UINT32_MAX
                : (uint32_t)(s_soh.learning_net_discharge_mah + 0.5f);
    }
    else
    {
        s_soh.result.learning_net_discharge_mah = 0u;
    }

    if (empty_anchor_edge)
    {
        Com_SOH_FinishCapacityLearning();
    }

    Com_SOH_UpdateScore();
}

bool Com_SOH_ValidatePersistent(const Com_SOH_PersistentTypeDef *state)
{
    bool learned_valid;

    if (state == 0)
    {
        return false;
    }

    learned_valid = (state->capacity_learning_count > 0u) &&
                    (state->learned_capacity_mah >= Com_SOH_LearnMinMah()) &&
                    (state->learned_capacity_mah <= Com_SOH_LearnMaxMah());

    if ((state->cycle_remainder_mah >= s_soh.config.capacity_mah) ||
        ((state->capacity_learning_count == 0u) && (state->learned_capacity_mah != 0u)) ||
        ((state->capacity_learning_count > 0u) && !learned_valid) ||
        (state->max_delta_mv > 5000u) || (state->max_temp_c < 0) || (state->max_temp_c > 125))
    {
        return false;
    }

    return true;
}

bool Com_SOH_RestorePersistent(const Com_SOH_PersistentTypeDef *state)
{
    bool learned_valid;

    if (!Com_SOH_ValidatePersistent(state))
    {
        return false;
    }

    learned_valid = (state->capacity_learning_count > 0u);

    s_soh.result.charge_throughput_mah = state->charge_throughput_mah;
    s_soh.result.discharge_throughput_mah = state->discharge_throughput_mah;
    s_soh.result.cycle_count = state->cycle_count;
    s_soh.result.safety_fault_count = state->safety_fault_count;
    s_soh.result.max_delta_mv = state->max_delta_mv;
    s_soh.result.max_temp_c = state->max_temp_c;
    s_soh.result.learned_capacity_mah = state->learned_capacity_mah;
    s_soh.result.capacity_learning_count = state->capacity_learning_count;
    s_soh.result.capacity_valid = learned_valid;
    /*
     * 复位期间主回路是否保持零电流无法由当前记录证明，因此只恢复已完成学习结果，
     * 不把掉电前后的两段库仑量拼成一次容量样本。
     */
    s_soh.result.learning_active = false;
    s_soh.result.learning_net_discharge_mah = 0u;
    s_soh.result.capacity_updated = false;
    s_soh.cycle_bucket_mah = (float)state->cycle_remainder_mah;
    s_soh.learning_net_discharge_mah = 0.0f;
    Com_SOH_UpdateScore();
    return true;
}

void Com_SOH_ExportPersistent(Com_SOH_PersistentTypeDef *state)
{
    if (state == 0)
    {
        return;
    }

    state->charge_throughput_mah = s_soh.result.charge_throughput_mah;
    state->discharge_throughput_mah = s_soh.result.discharge_throughput_mah;
    state->cycle_count = s_soh.result.cycle_count;
    state->safety_fault_count = s_soh.result.safety_fault_count;
    /* 余量必须严格小于额定容量；向下取整同时只损失不足 1 mAh 的小数。 */
    state->cycle_remainder_mah = (uint32_t)s_soh.cycle_bucket_mah;
    state->learned_capacity_mah = s_soh.result.learned_capacity_mah;
    state->capacity_learning_count = s_soh.result.capacity_learning_count;
    state->max_delta_mv = s_soh.result.max_delta_mv;
    state->max_temp_c = s_soh.result.max_temp_c;
}

void Com_SOH_GetResult(Com_SOH_ResultTypeDef *result)
{
    if (result == 0)
    {
        return;
    }

    *result = s_soh.result;
}
