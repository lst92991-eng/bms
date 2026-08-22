/**
 * @file bms_safety.c
 * @brief BMS V2 统一安全许可与功率目标计算实现。
 *
 * @ownership 由 BmsSafetyTask 单任务调用；本模块不访问 RTOS、HAL 或外设。
 * @safety 每次计算先建立全安全目标，只有所有证据有效时才逐项释放许可。
 * @timing 固定遍历 6 路单体和 2 路温度传感器，执行时间具有常数上界。
 */

#include "bms_safety.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#define TARGET_MASK_CHARGE_PATH    (UINT8_C(1) << 0u)
#define TARGET_MASK_DISCHARGE_PATH (UINT8_C(1) << 1u)
#define TARGET_MASK_SC_CHARGE      (UINT8_C(1) << 2u)
#define TARGET_MASK_BALANCE        (UINT8_C(1) << 3u)

typedef struct
{
    bool header_valid;
    bool cells_valid;
    bool pack_voltage_valid;
    bool current_valid;
    bool thermal_base_valid;
    bool ic_temperature_ok;
    bool bq_online;
    bool bq_config_valid;
    bool bq_fault_free;
    bool sc_ready;
    bool sc_input_valid;
    bool sc_fault_free;
    bool charge_temperature_ok;
    bool discharge_temperature_ok;
    bool balance_temperature_ok;
    bool charge_current_ok;
    bool discharge_current_ok;
    bool balance_current_ok;
    uint8_t valid_cell_temperature_count;
    uint16_t minimum_cell_mv;
    uint16_t maximum_cell_mv;
    uint16_t cell_delta_mv;
} SafetyEvidence;

static bool config_is_valid(const BmsSafety_Config *config)
{
    if (config == NULL)
    {
        return false;
    }

    if ((config->snapshot_max_age_ms == 0u) || (config->cells_max_age_ms == 0u) ||
        (config->pack_voltage_max_age_ms == 0u) || (config->current_max_age_ms == 0u) ||
        (config->thermal_max_age_ms == 0u) || (config->bq_status_max_age_ms == 0u) ||
        (config->sc_status_max_age_ms == 0u) || (config->permit_validity_ms == 0u) ||
        (config->wake_timeout_ms == 0u))
    {
        return false;
    }

    if (!((config->cell_valid_min_mv < config->discharge_stop_cell_mv) &&
          (config->discharge_stop_cell_mv < config->discharge_resume_cell_mv) &&
          (config->discharge_resume_cell_mv < config->charge_resume_cell_mv) &&
          (config->charge_resume_cell_mv < config->charge_stop_cell_mv) &&
          (config->charge_stop_cell_mv <= config->cell_valid_max_mv)))
    {
        return false;
    }

    if (!((config->cell_valid_min_mv <= config->balance_min_cell_mv) &&
          (config->balance_min_cell_mv < config->balance_max_cell_mv) &&
          (config->balance_max_cell_mv <= config->cell_valid_max_mv) &&
          (config->balance_stop_delta_mv < config->balance_start_delta_mv) &&
          (config->stack_cell_sum_tolerance_mv > 0u)))
    {
        return false;
    }

    if (!((config->charge_temp_min_deg_c < config->charge_temp_max_deg_c) &&
          (config->discharge_temp_min_deg_c < config->discharge_temp_max_deg_c) &&
          (config->balance_temp_min_deg_c < config->balance_temp_max_deg_c)))
    {
        return false;
    }

    if ((config->max_charge_current_ma <= 0) || (config->max_discharge_current_ma <= 0) ||
        (config->balance_max_abs_current_ma <= 0))
    {
        return false;
    }

    if ((config->required_charge_temp_sensor_count == 0u) ||
        (config->required_charge_temp_sensor_count > BMS_CELL_TEMP_SENSOR_COUNT) ||
        (config->required_discharge_temp_sensor_count == 0u) ||
        (config->required_discharge_temp_sensor_count > BMS_CELL_TEMP_SENSOR_COUNT) ||
        (config->required_balance_temp_sensor_count == 0u) ||
        (config->required_balance_temp_sensor_count > BMS_CELL_TEMP_SENSOR_COUNT))
    {
        return false;
    }

    return true;
}

static bool meta_is_valid_and_fresh(const BmsData_Meta *meta,
                                    uint32_t now_ms,
                                    uint32_t maximum_age_ms)
{
    if ((meta == NULL) || (meta->quality != BMS_DATA_QUALITY_VALID))
    {
        return false;
    }

    return (uint32_t)(now_ms - meta->timestamp_ms) <= maximum_age_ms;
}

static uint32_t absolute_current_ma(int32_t current_ma)
{
    if (current_ma >= 0)
    {
        return (uint32_t)current_ma;
    }

    if (current_ma == INT32_MIN)
    {
        return UINT32_C(2147483648);
    }

    return (uint32_t)(-current_ma);
}

static uint32_t next_generation(uint32_t generation)
{
    generation++;
    if (generation == 0u)
    {
        generation = 1u;
    }

    return generation;
}

static void initialize_safe_decision(const BmsSafety_State *state,
                                     const BmsSafety_Input *input,
                                     BmsSafety_Decision *decision)
{
    (void)memset(decision, 0, sizeof(*decision));
    decision->mode = BMS_SAFETY_MODE_FAULT;
    decision->target.require_pstop_asserted = true;
    decision->target.valid_until_ms = input->now_ms;
    decision->target.permit_generation = state->permit_generation;

    if (input->snapshot != NULL)
    {
        decision->target.snapshot_sequence = input->snapshot->sequence;
    }
}

static uint8_t target_mask(const BmsActuator_Target *target)
{
    uint8_t mask = 0u;

    if (target->allow_charge_path)
    {
        mask |= TARGET_MASK_CHARGE_PATH;
    }
    if (target->allow_discharge_path)
    {
        mask |= TARGET_MASK_DISCHARGE_PATH;
    }
    if (target->request_sc_charge)
    {
        mask |= TARGET_MASK_SC_CHARGE;
    }
    if (target->allow_balancing)
    {
        mask |= TARGET_MASK_BALANCE;
    }

    return mask;
}

static void finalize_target(const BmsSafety_Config *config,
                            BmsSafety_State *state,
                            uint32_t now_ms,
                            BmsActuator_Target *target)
{
    const uint8_t new_target_mask = target_mask(target);

    if (new_target_mask != state->last_target_mask)
    {
        state->permit_generation = next_generation(state->permit_generation);
        state->last_target_mask = new_target_mask;
    }

    target->permit_generation = state->permit_generation;
    target->require_pstop_asserted = !target->request_sc_charge;
    target->valid_until_ms = (new_target_mask == 0u) ?
                                 now_ms :
                                 (uint32_t)(now_ms + config->permit_validity_ms);
}

static bool evaluate_cells(const BmsSafety_Config *config,
                           const BmsSafety_Input *input,
                           SafetyEvidence *evidence)
{
    const BmsSnapshot *snapshot = input->snapshot;
    uint16_t minimum_mv = UINT16_MAX;
    uint16_t maximum_mv = 0u;
    uint32_t sum_mv = 0u;

    if (!meta_is_valid_and_fresh(&snapshot->cells.meta,
                                 input->now_ms,
                                 config->cells_max_age_ms))
    {
        return false;
    }

    for (size_t index = 0u; index < BMS_CELL_COUNT; index++)
    {
        const uint16_t cell_mv = snapshot->cells.cell_mv[index];

        if ((cell_mv < config->cell_valid_min_mv) ||
            (cell_mv > config->cell_valid_max_mv))
        {
            return false;
        }

        minimum_mv = (cell_mv < minimum_mv) ? cell_mv : minimum_mv;
        maximum_mv = (cell_mv > maximum_mv) ? cell_mv : maximum_mv;
        sum_mv += cell_mv;
    }

    evidence->minimum_cell_mv = minimum_mv;
    evidence->maximum_cell_mv = maximum_mv;
    evidence->cell_delta_mv = (uint16_t)(maximum_mv - minimum_mv);

    if (!meta_is_valid_and_fresh(&snapshot->pack.voltage_meta,
                                 input->now_ms,
                                 config->pack_voltage_max_age_ms))
    {
        return true;
    }

    {
        const uint32_t stack_mv = snapshot->pack.stack_mv;
        const uint32_t difference_mv = (stack_mv >= sum_mv) ?
                                           (stack_mv - sum_mv) :
                                           (sum_mv - stack_mv);

        evidence->pack_voltage_valid =
            difference_mv <= config->stack_cell_sum_tolerance_mv;
    }

    return true;
}

static void evaluate_temperatures(const BmsSafety_Config *config,
                                  const BmsSafety_Input *input,
                                  SafetyEvidence *evidence)
{
    const BmsThermalMeasurements *thermal = &input->snapshot->thermal;
    bool charge_range_ok = true;
    bool discharge_range_ok = true;
    bool balance_range_ok = true;
    uint8_t valid_count = 0u;

    for (size_t index = 0u; index < BMS_CELL_TEMP_SENSOR_COUNT; index++)
    {
        const BmsTemperatureMeasurement *measurement = &thermal->cell_sensor[index];

        if (!meta_is_valid_and_fresh(&measurement->meta,
                                     input->now_ms,
                                     config->thermal_max_age_ms))
        {
            continue;
        }

        valid_count++;
        charge_range_ok = charge_range_ok &&
                          (measurement->temperature_deg_c >= config->charge_temp_min_deg_c) &&
                          (measurement->temperature_deg_c <= config->charge_temp_max_deg_c);
        discharge_range_ok = discharge_range_ok &&
                             (measurement->temperature_deg_c >=
                              config->discharge_temp_min_deg_c) &&
                             (measurement->temperature_deg_c <=
                              config->discharge_temp_max_deg_c);
        balance_range_ok = balance_range_ok &&
                           (measurement->temperature_deg_c >= config->balance_temp_min_deg_c) &&
                           (measurement->temperature_deg_c <= config->balance_temp_max_deg_c);
    }

    evidence->valid_cell_temperature_count = valid_count;
    evidence->ic_temperature_ok =
        meta_is_valid_and_fresh(&thermal->ic.meta,
                                input->now_ms,
                                config->thermal_max_age_ms) &&
        (thermal->ic.temperature_deg_c <= config->ic_temp_max_deg_c);
    evidence->thermal_base_valid = (valid_count > 0u) && evidence->ic_temperature_ok;
    evidence->charge_temperature_ok =
        evidence->ic_temperature_ok && charge_range_ok &&
        (valid_count >= config->required_charge_temp_sensor_count);
    evidence->discharge_temperature_ok =
        evidence->ic_temperature_ok && discharge_range_ok &&
        (valid_count >= config->required_discharge_temp_sensor_count);
    evidence->balance_temperature_ok =
        evidence->ic_temperature_ok && balance_range_ok &&
        (valid_count >= config->required_balance_temp_sensor_count);
}

static void evaluate_current(const BmsSafety_Config *config,
                             const BmsSafety_Input *input,
                             SafetyEvidence *evidence)
{
    const int32_t current_ma = input->snapshot->pack.current_ma;

    evidence->current_valid =
        meta_is_valid_and_fresh(&input->snapshot->pack.current_meta,
                                input->now_ms,
                                config->current_max_age_ms);
    if (!evidence->current_valid)
    {
        return;
    }

    evidence->charge_current_ok = current_ma <= config->max_charge_current_ma;
    evidence->discharge_current_ok =
        (int64_t)current_ma >= -(int64_t)config->max_discharge_current_ma;
    evidence->balance_current_ok =
        absolute_current_ma(current_ma) <= (uint32_t)config->balance_max_abs_current_ma;
}

static void evaluate_chip_status(const BmsSafety_Config *config,
                                 const BmsSafety_Input *input,
                                 SafetyEvidence *evidence)
{
    const BmsSnapshot *snapshot = input->snapshot;
    const bool bq_status_valid =
        meta_is_valid_and_fresh(&snapshot->bq.meta,
                                input->now_ms,
                                config->bq_status_max_age_ms);
    const bool sc_status_valid =
        meta_is_valid_and_fresh(&snapshot->sc.meta,
                                input->now_ms,
                                config->sc_status_max_age_ms);

    evidence->bq_online = bq_status_valid && snapshot->bq.is_online;
    evidence->bq_config_valid = evidence->bq_online && snapshot->bq.is_config_valid;
    evidence->bq_fault_free = evidence->bq_online &&
                              !snapshot->bq.is_safety_active &&
                              !snapshot->bq.is_permanent_failure_active;

    evidence->sc_ready = sc_status_valid && snapshot->sc.is_communication_ok;
    evidence->sc_input_valid = evidence->sc_ready && snapshot->sc.is_ac_present;
    evidence->sc_fault_free = evidence->sc_ready &&
                              !snapshot->sc.is_vbus_short &&
                              !snapshot->sc.is_over_temperature;
}

static bool wake_charge_is_authorized(const BmsSafety_Config *config,
                                      const BmsSafety_State *state,
                                      const BmsSafety_Input *input,
                                      const SafetyEvidence *evidence)
{
    const BmsShutdownEvidence *shutdown = &input->shutdown_evidence;
    const bool mode_supports_wake = (state->mode == BMS_SAFETY_MODE_SHUTDOWN_PENDING) ||
                                    (state->mode == BMS_SAFETY_MODE_SHUTDOWN) ||
                                    (state->mode == BMS_SAFETY_MODE_WAKE);
    const bool no_charge_inhibit =
        (input->effective_fault_action_mask &
         (BMS_ACTION_INHIBIT_CHARGE | BMS_ACTION_ASSERT_PSTOP | BMS_ACTION_LOCKOUT)) == 0u;
    const bool within_timeout =
        (uint32_t)(input->now_ms - shutdown->wake_started_at_ms) <= config->wake_timeout_ms;

    return input->request_sc_charge && mode_supports_wake && shutdown->host_requested &&
           shutdown->command_confirmed && shutdown->bq_shutdown_seen &&
           shutdown->bq_offline_seen && within_timeout && evidence->sc_ready &&
           evidence->sc_input_valid && evidence->sc_fault_free && no_charge_inhibit;
}

static void update_voltage_latches(const BmsSafety_Config *config,
                                   BmsSafety_State *state,
                                   const SafetyEvidence *evidence)
{
    if (!evidence->cells_valid)
    {
        return;
    }

    if (evidence->maximum_cell_mv >= config->charge_stop_cell_mv)
    {
        state->charge_voltage_latched = true;
    }
    else if (state->charge_voltage_latched &&
             (evidence->maximum_cell_mv <= config->charge_resume_cell_mv))
    {
        state->charge_voltage_latched = false;
    }

    if (evidence->minimum_cell_mv <= config->discharge_stop_cell_mv)
    {
        state->discharge_voltage_latched = true;
    }
    else if (state->discharge_voltage_latched &&
             (evidence->minimum_cell_mv >= config->discharge_resume_cell_mv))
    {
        state->discharge_voltage_latched = false;
    }
}

static bool balance_is_allowed(const BmsSafety_Config *config,
                               BmsSafety_State *state,
                               const BmsSafety_Input *input,
                               const SafetyEvidence *evidence)
{
    const bool action_allows_balance =
        (input->effective_fault_action_mask &
         (BMS_ACTION_INHIBIT_BALANCE | BMS_ACTION_LOCKOUT)) == 0u;
    const bool voltage_window_ok =
        (evidence->minimum_cell_mv >= config->balance_min_cell_mv) &&
        (evidence->maximum_cell_mv <= config->balance_max_cell_mv);
    bool delta_ok;

    if (state->balancing_active)
    {
        delta_ok = evidence->cell_delta_mv > config->balance_stop_delta_mv;
    }
    else
    {
        delta_ok = evidence->cell_delta_mv >= config->balance_start_delta_mv;
    }

    state->balancing_active = input->request_balancing && evidence->cells_valid &&
                              evidence->current_valid && evidence->bq_online &&
                              evidence->bq_config_valid && evidence->bq_fault_free &&
                              evidence->balance_temperature_ok &&
                              evidence->balance_current_ok && action_allows_balance &&
                              voltage_window_ok && delta_ok;
    return state->balancing_active;
}

BmsStatus BmsSafety_Init(BmsSafety_State *state, const BmsSafety_Config *config)
{
    if (state == NULL)
    {
        return BMS_STATUS_INVALID_ARGUMENT;
    }

    (void)memset(state, 0, sizeof(*state));
    state->mode = BMS_SAFETY_MODE_BOOT_SAFE;
    state->permit_generation = 1u;

    if (!config_is_valid(config))
    {
        return BMS_STATUS_CONFIG_MISMATCH;
    }

    state->initialized = true;
    return BMS_STATUS_OK;
}

BmsStatus BmsSafety_Evaluate(const BmsSafety_Config *config,
                             BmsSafety_State *state,
                             const BmsSafety_Input *input,
                             BmsSafety_Decision *decision)
{
    SafetyEvidence evidence = {0};
    bool normal_snapshot_usable;
    bool allow_charge;
    bool allow_discharge;
    bool allow_balance;
    bool allow_sc_charge;

    if ((config == NULL) || (state == NULL) || (input == NULL) || (decision == NULL))
    {
        return BMS_STATUS_INVALID_ARGUMENT;
    }

    initialize_safe_decision(state, input, decision);

    if (!state->initialized)
    {
        decision->target.reason_mask |= BMS_SAFETY_REASON_SNAPSHOT_INVALID;
        return BMS_STATUS_STATE_ERROR;
    }

    if (!config_is_valid(config))
    {
        decision->target.reason_mask |= BMS_SAFETY_REASON_SNAPSHOT_INVALID;
        finalize_target(config, state, input->now_ms, &decision->target);
        return BMS_STATUS_CONFIG_MISMATCH;
    }

    if (input->request_shutdown)
    {
        state->mode = BMS_SAFETY_MODE_SHUTDOWN_PENDING;
        state->balancing_active = false;
        decision->mode = state->mode;
        decision->target.reason_mask |= BMS_SAFETY_REASON_SHUTDOWN_REQUESTED;
        finalize_target(config, state, input->now_ms, &decision->target);
        return BMS_STATUS_OK;
    }

    if ((input->effective_fault_action_mask & BMS_ACTION_LOCKOUT) != 0u)
    {
        state->mode = BMS_SAFETY_MODE_FAULT_LATCHED;
        state->balancing_active = false;
        decision->mode = state->mode;
        decision->target.reason_mask |= BMS_SAFETY_REASON_FAULT_ACTION;
        finalize_target(config, state, input->now_ms, &decision->target);
        return BMS_STATUS_OK;
    }

    if (input->snapshot == NULL)
    {
        state->mode = (state->mode == BMS_SAFETY_MODE_BOOT_SAFE) ?
                          BMS_SAFETY_MODE_BOOT_SAFE :
                          BMS_SAFETY_MODE_FAULT;
        state->balancing_active = false;
        decision->mode = state->mode;
        decision->target.reason_mask |= BMS_SAFETY_REASON_SNAPSHOT_INVALID;
        finalize_target(config, state, input->now_ms, &decision->target);
        return BMS_STATUS_OK;
    }

    evidence.header_valid = (input->snapshot->sequence != 0u) &&
                            ((uint32_t)(input->now_ms - input->snapshot->published_at_ms) <=
                             config->snapshot_max_age_ms);
    evidence.cells_valid = evaluate_cells(config, input, &evidence);
    evaluate_current(config, input, &evidence);
    evaluate_temperatures(config, input, &evidence);
    evaluate_chip_status(config, input, &evidence);

    decision->valid_cell_temperature_count = evidence.valid_cell_temperature_count;
    decision->minimum_cell_mv = evidence.minimum_cell_mv;
    decision->maximum_cell_mv = evidence.maximum_cell_mv;
    decision->cell_delta_mv = evidence.cell_delta_mv;

    if (!evidence.header_valid)
    {
        decision->target.reason_mask |= BMS_SAFETY_REASON_SNAPSHOT_INVALID;
    }
    if (!evidence.cells_valid)
    {
        decision->target.reason_mask |= BMS_SAFETY_REASON_CELLS_INVALID;
    }
    if (!evidence.pack_voltage_valid)
    {
        decision->target.reason_mask |= BMS_SAFETY_REASON_PACK_VOLTAGE_INVALID;
    }
    if (!evidence.current_valid)
    {
        decision->target.reason_mask |= BMS_SAFETY_REASON_CURRENT_INVALID;
    }
    if (!evidence.thermal_base_valid)
    {
        decision->target.reason_mask |= BMS_SAFETY_REASON_THERMAL_INVALID;
    }
    if (!evidence.ic_temperature_ok)
    {
        decision->target.reason_mask |= BMS_SAFETY_REASON_IC_TEMPERATURE;
    }
    if (!evidence.bq_online)
    {
        decision->target.reason_mask |= BMS_SAFETY_REASON_BQ_OFFLINE;
    }
    if (evidence.bq_online && !evidence.bq_config_valid)
    {
        decision->target.reason_mask |= BMS_SAFETY_REASON_BQ_CONFIG_INVALID;
    }
    if (evidence.bq_online && !evidence.bq_fault_free)
    {
        decision->target.reason_mask |= BMS_SAFETY_REASON_BQ_FAULT;
    }
    if (input->effective_fault_action_mask != BMS_ACTION_NONE)
    {
        decision->target.reason_mask |= BMS_SAFETY_REASON_FAULT_ACTION;
    }

    if (evidence.bq_online && (state->mode == BMS_SAFETY_MODE_SHUTDOWN_PENDING))
    {
        state->balancing_active = false;
        decision->mode = state->mode;
        decision->target.reason_mask |= BMS_SAFETY_REASON_SHUTDOWN_REQUESTED;
        finalize_target(config, state, input->now_ms, &decision->target);
        return BMS_STATUS_OK;
    }

    if (!evidence.bq_online)
    {
        const bool wake_authorized =
            evidence.header_valid && wake_charge_is_authorized(config, state, input, &evidence);

        decision->wake_charge_authorized = wake_authorized;
        state->balancing_active = false;

        if (wake_authorized)
        {
            decision->target.request_sc_charge = true;
            state->mode = BMS_SAFETY_MODE_WAKE;
        }
        else if (input->shutdown_evidence.bq_shutdown_seen &&
                 input->shutdown_evidence.bq_offline_seen)
        {
            state->mode = BMS_SAFETY_MODE_SHUTDOWN;
            if (input->request_sc_charge)
            {
                decision->target.reason_mask |= BMS_SAFETY_REASON_WAKE_NOT_AUTHORIZED;
            }
        }
        else
        {
            state->mode = BMS_SAFETY_MODE_FAULT;
            if (input->request_sc_charge)
            {
                decision->target.reason_mask |= BMS_SAFETY_REASON_WAKE_NOT_AUTHORIZED;
            }
        }

        decision->mode = state->mode;
        finalize_target(config, state, input->now_ms, &decision->target);
        return BMS_STATUS_OK;
    }

    update_voltage_latches(config, state, &evidence);

    if (state->charge_voltage_latched)
    {
        decision->target.reason_mask |= BMS_SAFETY_REASON_CELL_HIGH;
    }
    if (state->discharge_voltage_latched)
    {
        decision->target.reason_mask |= BMS_SAFETY_REASON_CELL_LOW;
    }
    if (evidence.current_valid && !evidence.charge_current_ok)
    {
        decision->target.reason_mask |= BMS_SAFETY_REASON_CHARGE_OVERCURRENT;
    }
    if (evidence.current_valid && !evidence.discharge_current_ok)
    {
        decision->target.reason_mask |= BMS_SAFETY_REASON_DISCHARGE_OVERCURRENT;
    }
    if ((input->request_charge_path || input->request_sc_charge) &&
        !evidence.charge_temperature_ok)
    {
        decision->target.reason_mask |= BMS_SAFETY_REASON_CHARGE_TEMPERATURE;
    }
    if (input->request_discharge_path && !evidence.discharge_temperature_ok)
    {
        decision->target.reason_mask |= BMS_SAFETY_REASON_DISCHARGE_TEMPERATURE;
    }

    normal_snapshot_usable = evidence.header_valid && evidence.cells_valid &&
                             evidence.pack_voltage_valid && evidence.current_valid &&
                             evidence.thermal_base_valid && evidence.bq_config_valid &&
                             evidence.bq_fault_free;
    decision->snapshot_usable = normal_snapshot_usable;

    allow_charge = input->request_charge_path && normal_snapshot_usable &&
                   evidence.charge_temperature_ok && evidence.charge_current_ok &&
                   !state->charge_voltage_latched &&
                   ((input->effective_fault_action_mask & BMS_ACTION_INHIBIT_CHARGE) == 0u);
    allow_discharge = input->request_discharge_path && normal_snapshot_usable &&
                      evidence.discharge_temperature_ok && evidence.discharge_current_ok &&
                      !state->discharge_voltage_latched &&
                      ((input->effective_fault_action_mask &
                        BMS_ACTION_INHIBIT_DISCHARGE) == 0u);
    allow_balance = normal_snapshot_usable && balance_is_allowed(config, state, input, &evidence);

    if (input->request_sc_charge)
    {
        if (!evidence.sc_ready)
        {
            decision->target.reason_mask |= BMS_SAFETY_REASON_SC_INVALID;
        }
        if (!evidence.sc_input_valid)
        {
            decision->target.reason_mask |= BMS_SAFETY_REASON_SC_INPUT_INVALID;
        }
        if (!evidence.sc_fault_free)
        {
            decision->target.reason_mask |= BMS_SAFETY_REASON_SC_FAULT;
        }
    }

    allow_sc_charge = input->request_sc_charge && allow_charge && evidence.sc_ready &&
                      evidence.sc_input_valid && evidence.sc_fault_free &&
                      ((input->effective_fault_action_mask &
                        (BMS_ACTION_ASSERT_PSTOP | BMS_ACTION_INHIBIT_CHARGE)) == 0u);

    if (input->request_balancing && !allow_balance)
    {
        decision->target.reason_mask |= BMS_SAFETY_REASON_BALANCE_CONDITION;
    }

    decision->target.allow_charge_path = allow_charge;
    decision->target.allow_discharge_path = allow_discharge;
    decision->target.request_sc_charge = allow_sc_charge;
    decision->target.allow_balancing = allow_balance;

    if (allow_charge || allow_discharge || allow_sc_charge || allow_balance)
    {
        state->mode = BMS_SAFETY_MODE_ACTIVE;
    }
    else if (normal_snapshot_usable)
    {
        state->mode = BMS_SAFETY_MODE_STANDBY;
    }
    else
    {
        state->mode = BMS_SAFETY_MODE_FAULT;
    }

    decision->mode = state->mode;
    finalize_target(config, state, input->now_ms, &decision->target);
    return BMS_STATUS_OK;
}
