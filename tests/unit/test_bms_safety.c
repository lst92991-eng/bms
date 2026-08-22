#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "bms_safety.h"

static unsigned int s_failures;

#define EXPECT_TRUE(condition)                                                          \
    do                                                                                  \
    {                                                                                   \
        if (!(condition))                                                               \
        {                                                                               \
            (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);          \
            s_failures++;                                                               \
        }                                                                               \
    } while (0)

#define EXPECT_FALSE(condition) EXPECT_TRUE(!(condition))

#define EXPECT_EQ_U32(expected, actual)                                                 \
    do                                                                                  \
    {                                                                                   \
        const uint32_t expected_value = (uint32_t)(expected);                           \
        const uint32_t actual_value = (uint32_t)(actual);                               \
        if (expected_value != actual_value)                                             \
        {                                                                               \
            (void)printf("FAIL %s:%d: expected %lu, actual %lu\n",                    \
                         __FILE__,                                                       \
                         __LINE__,                                                       \
                         (unsigned long)expected_value,                                  \
                         (unsigned long)actual_value);                                   \
            s_failures++;                                                               \
        }                                                                               \
    } while (0)

static BmsData_Meta valid_meta(uint32_t timestamp_ms)
{
    BmsData_Meta meta =
    {
        .quality = BMS_DATA_QUALITY_VALID,
        .timestamp_ms = timestamp_ms,
        .source_flags = 1u,
    };

    return meta;
}

static BmsSafety_Config make_config(void)
{
    BmsSafety_Config config =
    {
        .snapshot_max_age_ms = 100u,
        .cells_max_age_ms = 100u,
        .pack_voltage_max_age_ms = 100u,
        .current_max_age_ms = 100u,
        .thermal_max_age_ms = 100u,
        .bq_status_max_age_ms = 100u,
        .sc_status_max_age_ms = 100u,
        .permit_validity_ms = 50u,
        .wake_timeout_ms = 60000u,
        .cell_valid_min_mv = 2500u,
        .cell_valid_max_mv = 4350u,
        .charge_stop_cell_mv = 4200u,
        .charge_resume_cell_mv = 4180u,
        .discharge_stop_cell_mv = 3000u,
        .discharge_resume_cell_mv = 3200u,
        .balance_min_cell_mv = 3900u,
        .balance_max_cell_mv = 4200u,
        .balance_start_delta_mv = 40u,
        .balance_stop_delta_mv = 20u,
        .stack_cell_sum_tolerance_mv = 100u,
        .charge_temp_min_deg_c = 0,
        .charge_temp_max_deg_c = 45,
        .discharge_temp_min_deg_c = -20,
        .discharge_temp_max_deg_c = 60,
        .balance_temp_min_deg_c = 0,
        .balance_temp_max_deg_c = 45,
        .ic_temp_max_deg_c = 70,
        .max_charge_current_ma = 5000,
        .max_discharge_current_ma = 12000,
        .balance_max_abs_current_ma = 100,
        .required_charge_temp_sensor_count = 2u,
        .required_discharge_temp_sensor_count = 2u,
        .required_balance_temp_sensor_count = 2u,
    };

    return config;
}

static void update_stack_voltage(BmsSnapshot *snapshot, uint32_t timestamp_ms)
{
    uint32_t sum_mv = 0u;

    for (size_t index = 0u; index < BMS_CELL_COUNT; index++)
    {
        sum_mv += snapshot->cells.cell_mv[index];
    }

    snapshot->pack.stack_mv = sum_mv;
    snapshot->pack.pack_mv = sum_mv;
    snapshot->pack.voltage_meta = valid_meta(timestamp_ms);
}

static BmsSnapshot make_healthy_snapshot(uint32_t now_ms)
{
    BmsSnapshot snapshot = {0};

    snapshot.sequence = 1u;
    snapshot.published_at_ms = now_ms;
    snapshot.cells.meta = valid_meta(now_ms);
    for (size_t index = 0u; index < BMS_CELL_COUNT; index++)
    {
        snapshot.cells.cell_mv[index] = (uint16_t)(4000u - ((uint16_t)index * 10u));
    }

    update_stack_voltage(&snapshot, now_ms);
    snapshot.pack.current_meta = valid_meta(now_ms);
    snapshot.pack.current_ma = 0;

    for (size_t index = 0u; index < BMS_CELL_TEMP_SENSOR_COUNT; index++)
    {
        snapshot.thermal.cell_sensor[index].meta = valid_meta(now_ms);
        snapshot.thermal.cell_sensor[index].temperature_deg_c = 25;
    }
    snapshot.thermal.ic.meta = valid_meta(now_ms);
    snapshot.thermal.ic.temperature_deg_c = 30;

    snapshot.bq.meta = valid_meta(now_ms);
    snapshot.bq.is_online = true;
    snapshot.bq.is_config_valid = true;
    snapshot.bq.is_safety_active = false;
    snapshot.bq.is_permanent_failure_active = false;

    snapshot.sc.meta = valid_meta(now_ms);
    snapshot.sc.is_communication_ok = true;
    snapshot.sc.is_ac_present = true;
    snapshot.sc.is_vbus_short = false;
    snapshot.sc.is_over_temperature = false;
    snapshot.sc.is_standby = true;
    snapshot.sc.vbus_mv = 24000u;
    snapshot.sc.vbat_mv = snapshot.pack.stack_mv;

    return snapshot;
}

static BmsSafety_Input make_input(uint32_t now_ms, const BmsSnapshot *snapshot)
{
    BmsSafety_Input input =
    {
        .now_ms = now_ms,
        .snapshot = snapshot,
        .effective_fault_action_mask = BMS_ACTION_NONE,
        .request_charge_path = true,
        .request_discharge_path = true,
        .request_sc_charge = true,
        .request_balancing = true,
        .request_shutdown = false,
        .shutdown_evidence = {0},
    };

    return input;
}

static BmsSafety_State make_state(const BmsSafety_Config *config)
{
    BmsSafety_State state;

    EXPECT_EQ_U32(BMS_STATUS_OK, BmsSafety_Init(&state, config));
    return state;
}

static void expect_safe_target(const BmsActuator_Target *target)
{
    EXPECT_FALSE(target->allow_charge_path);
    EXPECT_FALSE(target->allow_discharge_path);
    EXPECT_FALSE(target->request_sc_charge);
    EXPECT_FALSE(target->allow_balancing);
    EXPECT_TRUE(target->require_pstop_asserted);
}

static void test_healthy_snapshot_releases_requested_paths(void)
{
    const uint32_t now_ms = 1000u;
    BmsSafety_Config config = make_config();
    BmsSafety_State state = make_state(&config);
    BmsSnapshot snapshot = make_healthy_snapshot(now_ms);
    BmsSafety_Input input = make_input(now_ms, &snapshot);
    BmsSafety_Decision decision;

    EXPECT_EQ_U32(BMS_STATUS_OK, BmsSafety_Evaluate(&config, &state, &input, &decision));
    EXPECT_TRUE(decision.snapshot_usable);
    EXPECT_EQ_U32(BMS_SAFETY_MODE_ACTIVE, decision.mode);
    EXPECT_TRUE(decision.target.allow_charge_path);
    EXPECT_TRUE(decision.target.allow_discharge_path);
    EXPECT_TRUE(decision.target.request_sc_charge);
    EXPECT_TRUE(decision.target.allow_balancing);
    EXPECT_FALSE(decision.target.require_pstop_asserted);
    EXPECT_EQ_U32(BMS_SAFETY_REASON_NONE, decision.target.reason_mask);
    EXPECT_EQ_U32(now_ms + config.permit_validity_ms, decision.target.valid_until_ms);
}

static void test_missing_snapshot_stays_boot_safe(void)
{
    BmsSafety_Config config = make_config();
    BmsSafety_State state = make_state(&config);
    BmsSafety_Input input = make_input(1000u, NULL);
    BmsSafety_Decision decision;

    EXPECT_EQ_U32(BMS_STATUS_OK, BmsSafety_Evaluate(&config, &state, &input, &decision));
    expect_safe_target(&decision.target);
    EXPECT_EQ_U32(BMS_SAFETY_MODE_BOOT_SAFE, decision.mode);
    EXPECT_TRUE((decision.target.reason_mask & BMS_SAFETY_REASON_SNAPSHOT_INVALID) != 0u);
}

static void test_temperature_redundancy_controls_each_path(void)
{
    const uint32_t now_ms = 1000u;
    BmsSafety_Config config = make_config();
    BmsSafety_State state;
    BmsSnapshot snapshot = make_healthy_snapshot(now_ms);
    BmsSafety_Input input = make_input(now_ms, &snapshot);
    BmsSafety_Decision decision;

    config.required_discharge_temp_sensor_count = 1u;
    state = make_state(&config);
    snapshot.thermal.cell_sensor[1].meta.quality = BMS_DATA_QUALITY_IO_ERROR;

    EXPECT_EQ_U32(BMS_STATUS_OK, BmsSafety_Evaluate(&config, &state, &input, &decision));
    EXPECT_EQ_U32(1u, decision.valid_cell_temperature_count);
    EXPECT_FALSE(decision.target.allow_charge_path);
    EXPECT_TRUE(decision.target.allow_discharge_path);
    EXPECT_FALSE(decision.target.request_sc_charge);
    EXPECT_FALSE(decision.target.allow_balancing);
    EXPECT_TRUE((decision.target.reason_mask & BMS_SAFETY_REASON_CHARGE_TEMPERATURE) != 0u);
    EXPECT_TRUE((decision.target.reason_mask & BMS_SAFETY_REASON_BALANCE_CONDITION) != 0u);
}

static void test_all_temperature_sensors_invalid_fail_safe(void)
{
    const uint32_t now_ms = 1000u;
    BmsSafety_Config config = make_config();
    BmsSafety_State state = make_state(&config);
    BmsSnapshot snapshot = make_healthy_snapshot(now_ms);
    BmsSafety_Input input = make_input(now_ms, &snapshot);
    BmsSafety_Decision decision;

    snapshot.thermal.cell_sensor[0].meta.quality = BMS_DATA_QUALITY_IO_ERROR;
    snapshot.thermal.cell_sensor[1].meta.quality = BMS_DATA_QUALITY_IO_ERROR;

    EXPECT_EQ_U32(BMS_STATUS_OK, BmsSafety_Evaluate(&config, &state, &input, &decision));
    expect_safe_target(&decision.target);
    EXPECT_FALSE(decision.snapshot_usable);
    EXPECT_TRUE((decision.target.reason_mask & BMS_SAFETY_REASON_THERMAL_INVALID) != 0u);
}

static void test_bq_config_mismatch_blocks_every_path(void)
{
    const uint32_t now_ms = 1000u;
    BmsSafety_Config config = make_config();
    BmsSafety_State state = make_state(&config);
    BmsSnapshot snapshot = make_healthy_snapshot(now_ms);
    BmsSafety_Input input = make_input(now_ms, &snapshot);
    BmsSafety_Decision decision;

    snapshot.bq.is_config_valid = false;

    EXPECT_EQ_U32(BMS_STATUS_OK, BmsSafety_Evaluate(&config, &state, &input, &decision));
    expect_safe_target(&decision.target);
    EXPECT_TRUE((decision.target.reason_mask & BMS_SAFETY_REASON_BQ_CONFIG_INVALID) != 0u);
}

static void test_charge_voltage_hysteresis(void)
{
    const uint32_t now_ms = 1000u;
    BmsSafety_Config config = make_config();
    BmsSafety_State state = make_state(&config);
    BmsSnapshot snapshot = make_healthy_snapshot(now_ms);
    BmsSafety_Input input = make_input(now_ms, &snapshot);
    BmsSafety_Decision decision;

    input.request_balancing = false;
    snapshot.cells.cell_mv[0] = config.charge_stop_cell_mv;
    update_stack_voltage(&snapshot, now_ms);
    EXPECT_EQ_U32(BMS_STATUS_OK, BmsSafety_Evaluate(&config, &state, &input, &decision));
    EXPECT_FALSE(decision.target.allow_charge_path);
    EXPECT_TRUE(decision.target.allow_discharge_path);

    snapshot.cells.cell_mv[0] = (uint16_t)(config.charge_resume_cell_mv + 10u);
    update_stack_voltage(&snapshot, now_ms);
    EXPECT_EQ_U32(BMS_STATUS_OK, BmsSafety_Evaluate(&config, &state, &input, &decision));
    EXPECT_FALSE(decision.target.allow_charge_path);

    snapshot.cells.cell_mv[0] = config.charge_resume_cell_mv;
    update_stack_voltage(&snapshot, now_ms);
    EXPECT_EQ_U32(BMS_STATUS_OK, BmsSafety_Evaluate(&config, &state, &input, &decision));
    EXPECT_TRUE(decision.target.allow_charge_path);
}

static void test_discharge_voltage_hysteresis(void)
{
    const uint32_t now_ms = 1000u;
    BmsSafety_Config config = make_config();
    BmsSafety_State state = make_state(&config);
    BmsSnapshot snapshot = make_healthy_snapshot(now_ms);
    BmsSafety_Input input = make_input(now_ms, &snapshot);
    BmsSafety_Decision decision;

    input.request_balancing = false;
    snapshot.cells.cell_mv[5] = config.discharge_stop_cell_mv;
    update_stack_voltage(&snapshot, now_ms);
    EXPECT_EQ_U32(BMS_STATUS_OK, BmsSafety_Evaluate(&config, &state, &input, &decision));
    EXPECT_FALSE(decision.target.allow_discharge_path);
    EXPECT_TRUE(decision.target.allow_charge_path);

    snapshot.cells.cell_mv[5] = (uint16_t)(config.discharge_resume_cell_mv - 100u);
    update_stack_voltage(&snapshot, now_ms);
    EXPECT_EQ_U32(BMS_STATUS_OK, BmsSafety_Evaluate(&config, &state, &input, &decision));
    EXPECT_FALSE(decision.target.allow_discharge_path);

    snapshot.cells.cell_mv[5] = config.discharge_resume_cell_mv;
    update_stack_voltage(&snapshot, now_ms);
    EXPECT_EQ_U32(BMS_STATUS_OK, BmsSafety_Evaluate(&config, &state, &input, &decision));
    EXPECT_TRUE(decision.target.allow_discharge_path);
}

static void test_sc_fault_only_blocks_sc_charge(void)
{
    const uint32_t now_ms = 1000u;
    BmsSafety_Config config = make_config();
    BmsSafety_State state = make_state(&config);
    BmsSnapshot snapshot = make_healthy_snapshot(now_ms);
    BmsSafety_Input input = make_input(now_ms, &snapshot);
    BmsSafety_Decision decision;

    input.request_balancing = false;
    snapshot.sc.is_vbus_short = true;

    EXPECT_EQ_U32(BMS_STATUS_OK, BmsSafety_Evaluate(&config, &state, &input, &decision));
    EXPECT_TRUE(decision.target.allow_charge_path);
    EXPECT_TRUE(decision.target.allow_discharge_path);
    EXPECT_FALSE(decision.target.request_sc_charge);
    EXPECT_TRUE(decision.target.require_pstop_asserted);
    EXPECT_TRUE((decision.target.reason_mask & BMS_SAFETY_REASON_SC_FAULT) != 0u);
}

static void test_fault_actions_inhibit_only_requested_paths(void)
{
    const uint32_t now_ms = 1000u;
    BmsSafety_Config config = make_config();
    BmsSafety_State state = make_state(&config);
    BmsSnapshot snapshot = make_healthy_snapshot(now_ms);
    BmsSafety_Input input = make_input(now_ms, &snapshot);
    BmsSafety_Decision decision;

    input.request_balancing = false;
    input.effective_fault_action_mask = BMS_ACTION_INHIBIT_CHARGE |
                                        BMS_ACTION_ASSERT_PSTOP;

    EXPECT_EQ_U32(BMS_STATUS_OK, BmsSafety_Evaluate(&config, &state, &input, &decision));
    EXPECT_FALSE(decision.target.allow_charge_path);
    EXPECT_TRUE(decision.target.allow_discharge_path);
    EXPECT_FALSE(decision.target.request_sc_charge);
    EXPECT_TRUE(decision.target.require_pstop_asserted);

    input.effective_fault_action_mask = BMS_ACTION_LOCKOUT;
    EXPECT_EQ_U32(BMS_STATUS_OK, BmsSafety_Evaluate(&config, &state, &input, &decision));
    expect_safe_target(&decision.target);
    EXPECT_EQ_U32(BMS_SAFETY_MODE_FAULT_LATCHED, decision.mode);
}

static void enter_shutdown_pending(const BmsSafety_Config *config,
                                   BmsSafety_State *state,
                                   BmsSnapshot *snapshot,
                                   uint32_t now_ms)
{
    BmsSafety_Input input = make_input(now_ms, snapshot);
    BmsSafety_Decision decision;

    input.request_shutdown = true;
    EXPECT_EQ_U32(BMS_STATUS_OK, BmsSafety_Evaluate(config, state, &input, &decision));
    EXPECT_EQ_U32(BMS_SAFETY_MODE_SHUTDOWN_PENDING, decision.mode);
    expect_safe_target(&decision.target);
}

static void test_complete_shutdown_evidence_allows_only_wake_charge(void)
{
    const uint32_t start_ms = 1000u;
    const uint32_t now_ms = 1010u;
    BmsSafety_Config config = make_config();
    BmsSafety_State state = make_state(&config);
    BmsSnapshot snapshot = make_healthy_snapshot(start_ms);
    BmsSafety_Input input;
    BmsSafety_Decision decision;

    enter_shutdown_pending(&config, &state, &snapshot, start_ms);

    snapshot.published_at_ms = now_ms;
    snapshot.bq.meta.quality = BMS_DATA_QUALITY_IO_ERROR;
    snapshot.sc.meta = valid_meta(now_ms);
    input = make_input(now_ms, &snapshot);
    input.shutdown_evidence.host_requested = true;
    input.shutdown_evidence.command_confirmed = true;
    input.shutdown_evidence.bq_shutdown_seen = true;
    input.shutdown_evidence.bq_offline_seen = true;
    input.shutdown_evidence.wake_started_at_ms = start_ms;

    EXPECT_EQ_U32(BMS_STATUS_OK, BmsSafety_Evaluate(&config, &state, &input, &decision));
    EXPECT_TRUE(decision.wake_charge_authorized);
    EXPECT_EQ_U32(BMS_SAFETY_MODE_WAKE, decision.mode);
    EXPECT_FALSE(decision.target.allow_charge_path);
    EXPECT_FALSE(decision.target.allow_discharge_path);
    EXPECT_TRUE(decision.target.request_sc_charge);
    EXPECT_FALSE(decision.target.allow_balancing);
    EXPECT_FALSE(decision.target.require_pstop_asserted);
}

static void test_ordinary_bq_offline_never_enters_wake_charge(void)
{
    const uint32_t now_ms = 1000u;
    BmsSafety_Config config = make_config();
    BmsSafety_State state = make_state(&config);
    BmsSnapshot snapshot = make_healthy_snapshot(now_ms);
    BmsSafety_Input input = make_input(now_ms, &snapshot);
    BmsSafety_Decision decision;

    input.request_balancing = false;
    EXPECT_EQ_U32(BMS_STATUS_OK, BmsSafety_Evaluate(&config, &state, &input, &decision));
    snapshot.bq.meta.quality = BMS_DATA_QUALITY_IO_ERROR;

    EXPECT_EQ_U32(BMS_STATUS_OK, BmsSafety_Evaluate(&config, &state, &input, &decision));
    EXPECT_FALSE(decision.wake_charge_authorized);
    expect_safe_target(&decision.target);
    EXPECT_EQ_U32(BMS_SAFETY_MODE_FAULT, decision.mode);
    EXPECT_TRUE((decision.target.reason_mask & BMS_SAFETY_REASON_WAKE_NOT_AUTHORIZED) != 0u);
}

static void test_wake_timeout_is_fail_safe(void)
{
    const uint32_t start_ms = 1000u;
    BmsSafety_Config config = make_config();
    BmsSafety_State state = make_state(&config);
    BmsSnapshot snapshot = make_healthy_snapshot(start_ms);
    BmsSafety_Input input;
    BmsSafety_Decision decision;
    const uint32_t now_ms = start_ms + config.wake_timeout_ms + 1u;

    enter_shutdown_pending(&config, &state, &snapshot, start_ms);
    snapshot.published_at_ms = now_ms;
    snapshot.bq.meta.quality = BMS_DATA_QUALITY_IO_ERROR;
    snapshot.sc.meta = valid_meta(now_ms);
    input = make_input(now_ms, &snapshot);
    input.shutdown_evidence.host_requested = true;
    input.shutdown_evidence.command_confirmed = true;
    input.shutdown_evidence.bq_shutdown_seen = true;
    input.shutdown_evidence.bq_offline_seen = true;
    input.shutdown_evidence.wake_started_at_ms = start_ms;

    EXPECT_EQ_U32(BMS_STATUS_OK, BmsSafety_Evaluate(&config, &state, &input, &decision));
    EXPECT_FALSE(decision.wake_charge_authorized);
    expect_safe_target(&decision.target);
    EXPECT_EQ_U32(BMS_SAFETY_MODE_SHUTDOWN, decision.mode);
}

static void test_stack_cell_sum_mismatch_is_rejected(void)
{
    const uint32_t now_ms = 1000u;
    BmsSafety_Config config = make_config();
    BmsSafety_State state = make_state(&config);
    BmsSnapshot snapshot = make_healthy_snapshot(now_ms);
    BmsSafety_Input input = make_input(now_ms, &snapshot);
    BmsSafety_Decision decision;

    snapshot.pack.stack_mv += (uint32_t)config.stack_cell_sum_tolerance_mv + 1u;

    EXPECT_EQ_U32(BMS_STATUS_OK, BmsSafety_Evaluate(&config, &state, &input, &decision));
    expect_safe_target(&decision.target);
    EXPECT_TRUE((decision.target.reason_mask & BMS_SAFETY_REASON_PACK_VOLTAGE_INVALID) != 0u);
}

static void test_balance_delta_hysteresis(void)
{
    const uint32_t now_ms = 1000u;
    BmsSafety_Config config = make_config();
    BmsSafety_State state = make_state(&config);
    BmsSnapshot snapshot = make_healthy_snapshot(now_ms);
    BmsSafety_Input input = make_input(now_ms, &snapshot);
    BmsSafety_Decision decision;
    const uint16_t spread_30_mv[BMS_CELL_COUNT] = {3980u, 3975u, 3970u, 3965u, 3960u, 3950u};
    const uint16_t spread_20_mv[BMS_CELL_COUNT] = {3970u, 3966u, 3962u, 3958u, 3954u, 3950u};

    input.request_charge_path = false;
    input.request_discharge_path = false;
    input.request_sc_charge = false;

    EXPECT_EQ_U32(BMS_STATUS_OK, BmsSafety_Evaluate(&config, &state, &input, &decision));
    EXPECT_TRUE(decision.target.allow_balancing);

    for (size_t index = 0u; index < BMS_CELL_COUNT; index++)
    {
        snapshot.cells.cell_mv[index] = spread_30_mv[index];
    }
    update_stack_voltage(&snapshot, now_ms);
    EXPECT_EQ_U32(BMS_STATUS_OK, BmsSafety_Evaluate(&config, &state, &input, &decision));
    EXPECT_TRUE(decision.target.allow_balancing);

    for (size_t index = 0u; index < BMS_CELL_COUNT; index++)
    {
        snapshot.cells.cell_mv[index] = spread_20_mv[index];
    }
    update_stack_voltage(&snapshot, now_ms);
    EXPECT_EQ_U32(BMS_STATUS_OK, BmsSafety_Evaluate(&config, &state, &input, &decision));
    EXPECT_FALSE(decision.target.allow_balancing);
}

static void test_unsigned_timestamp_wrap_is_fresh(void)
{
    const uint32_t now_ms = 10u;
    const uint32_t sample_ms = UINT32_MAX - 20u;
    BmsSafety_Config config = make_config();
    BmsSafety_State state = make_state(&config);
    BmsSnapshot snapshot = make_healthy_snapshot(sample_ms);
    BmsSafety_Input input = make_input(now_ms, &snapshot);
    BmsSafety_Decision decision;

    EXPECT_EQ_U32(BMS_STATUS_OK, BmsSafety_Evaluate(&config, &state, &input, &decision));
    EXPECT_TRUE(decision.snapshot_usable);
    EXPECT_TRUE(decision.target.allow_charge_path);
    EXPECT_TRUE(decision.target.allow_discharge_path);
}

static void test_invalid_configuration_is_rejected(void)
{
    BmsSafety_Config config = make_config();
    BmsSafety_State state;

    config.charge_resume_cell_mv = config.charge_stop_cell_mv;
    EXPECT_EQ_U32(BMS_STATUS_CONFIG_MISMATCH, BmsSafety_Init(&state, &config));
    EXPECT_FALSE(state.initialized);
}

int main(void)
{
    test_healthy_snapshot_releases_requested_paths();
    test_missing_snapshot_stays_boot_safe();
    test_temperature_redundancy_controls_each_path();
    test_all_temperature_sensors_invalid_fail_safe();
    test_bq_config_mismatch_blocks_every_path();
    test_charge_voltage_hysteresis();
    test_discharge_voltage_hysteresis();
    test_sc_fault_only_blocks_sc_charge();
    test_fault_actions_inhibit_only_requested_paths();
    test_complete_shutdown_evidence_allows_only_wake_charge();
    test_ordinary_bq_offline_never_enters_wake_charge();
    test_wake_timeout_is_fail_safe();
    test_stack_cell_sum_mismatch_is_rejected();
    test_balance_delta_hysteresis();
    test_unsigned_timestamp_wrap_is_fresh();
    test_invalid_configuration_is_rejected();

    if (s_failures != 0u)
    {
        (void)printf("%u test assertion(s) failed\n", s_failures);
        return 1;
    }

    (void)printf("bms_safety: all tests passed\n");
    return 0;
}
