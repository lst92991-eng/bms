#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "Com_BatteryParam.h"
#include "Com_Format.h"
#include "Com_SOC.h"
#include "Com_SOH.h"

static unsigned int s_failure_count;

#define CHECK(condition)                                                                           \
    do                                                                                             \
    {                                                                                              \
        if (!(condition))                                                                          \
        {                                                                                          \
            (void)fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);    \
            s_failure_count++;                                                                     \
        }                                                                                          \
    } while (0)

static float Test_AbsFloat(float value)
{
    return (value < 0.0f) ? -value : value;
}

static Com_SOC_ConfigTypeDef Test_SocConfig(void)
{
    const Com_SOC_ConfigTypeDef config = {
        .capacity_mah = 1000u,
        .default_soc_percent = 50.0f,
        .current_sign = COM_SOC_CURRENT_POS_CHARGE,
        .current_deadband_ma = 10,
        .rest_need_ms = 2000u,
        .rest_current_ma = 50,
        .rest_voltage_stable_mv = 3u,
        .kalman_q = 0.002f,
        .kalman_r = 9.0f,
        .ocv_update_limit_percent = 3.0f,
        .full_cell_mv = 4200u,
        .empty_cell_mv = 3000u,
        .full_current_ma = 250,
        .empty_current_ma = 250,
        .full_anchor_hold_ms = 1000u,
        .empty_anchor_hold_ms = 1000u,
        .display_rise_percent_per_s = 100.0f,
        .display_fall_percent_per_s = 100.0f,
    };

    return config;
}

static Com_SOC_SampleTypeDef Test_SocSample(void)
{
    const Com_SOC_SampleTypeDef sample = {
        .interval_ms = 1000u,
        .current_valid = true,
        .current_ma = 0,
        .cells_valid = true,
        .cell_min_mv = 3749u,
        .cell_max_mv = 3751u,
        .cell_avg_mv = 3750u,
        .temp_valid = true,
        .temp_c = 25,
        .soh_valid = false,
        .soh_percent = 0u,
    };

    return sample;
}

static void Test_OcvTable(void)
{
    uint16_t previous = 0u;
    uint16_t mv;

    CHECK(Com_BatteryParam_GetSoc0p01ByVoltage(2500u) == 0u);
    CHECK(Com_BatteryParam_GetSoc0p01ByVoltage(3000u) == 0u);
    CHECK(Com_BatteryParam_GetSoc0p01ByVoltage(4200u) == 10000u);
    CHECK(Com_BatteryParam_GetSoc0p01ByVoltage(4300u) == 10000u);

    for (mv = 3000u; mv <= 4200u; mv = (uint16_t)(mv + 5u))
    {
        const uint16_t current = Com_BatteryParam_GetSoc0p01ByVoltage(mv);

        CHECK(current >= previous);
        CHECK(current <= 10000u);
        previous = current;
    }
}

static void Test_SocInvalidUntilTrustedSeed(void)
{
    Com_SOC_ConfigTypeDef config = Test_SocConfig();
    Com_SOC_SampleTypeDef sample = Test_SocSample();
    Com_SOC_ResultTypeDef result;

    Com_SOC_Init(&config);
    Com_SOC_GetResult(&result);
    CHECK(!result.seeded);
    CHECK(!result.valid);
    CHECK(result.seed_source == COM_SOC_SEED_NONE);

    sample.current_valid = false;
    Com_SOC_Update(&sample);
    Com_SOC_GetResult(&result);
    CHECK(!result.seeded);
    CHECK(!result.valid);

    sample.current_valid = true;
    Com_SOC_Update(&sample);
    Com_SOC_Update(&sample);
    Com_SOC_Update(&sample);
    Com_SOC_GetResult(&result);
    CHECK(result.seeded);
    CHECK(result.valid);
    CHECK(result.seed_source == COM_SOC_SEED_OCV);
    CHECK(result.rest_ready);
    CHECK(Test_AbsFloat(result.soc_percent - 50.0f) < 0.1f);
}

static void Test_SocPersistentAndCoulombCounting(void)
{
    Com_SOC_ConfigTypeDef config = Test_SocConfig();
    Com_SOC_SampleTypeDef sample = Test_SocSample();
    Com_SOC_PersistentTypeDef persistent = {
        .soc_0p01_percent = 5000u,
        .display_0p01_percent = 5000u,
        .covariance_1e6 = 1000000u,
    };
    Com_SOC_ResultTypeDef result;

    Com_SOC_Init(&config);
    CHECK(Com_SOC_ValidatePersistent(&persistent));
    CHECK(Com_SOC_RestorePersistent(&persistent));

    sample.interval_ms = 360000u;
    sample.current_ma = 1000;
    Com_SOC_Update(&sample);
    Com_SOC_GetResult(&result);
    CHECK(Test_AbsFloat(result.soc_percent - 60.0f) < 0.1f);

    CHECK(Com_SOC_RestorePersistent(&persistent));
    sample.current_ma = -1000;
    Com_SOC_Update(&sample);
    Com_SOC_GetResult(&result);
    CHECK(Test_AbsFloat(result.soc_percent - 40.0f) < 0.1f);

    persistent.soc_0p01_percent = 10001u;
    CHECK(!Com_SOC_ValidatePersistent(&persistent));
    persistent.soc_0p01_percent = 5000u;
    persistent.covariance_1e6 = 99u;
    CHECK(!Com_SOC_ValidatePersistent(&persistent));
}

static void Test_SocAnchorGuardsAndTimeout(void)
{
    Com_SOC_ConfigTypeDef config = Test_SocConfig();
    Com_SOC_SampleTypeDef sample = Test_SocSample();
    Com_SOC_PersistentTypeDef persistent = {
        .soc_0p01_percent = 5000u,
        .display_0p01_percent = 5000u,
        .covariance_1e6 = 1000000u,
    };
    Com_SOC_ResultTypeDef result;

    Com_SOC_Init(&config);
    CHECK(Com_SOC_RestorePersistent(&persistent));
    Com_SOC_NotifyAnchorEvent(COM_SOC_ANCHOR_FULL_COMPLETE);
    sample.cell_min_mv = 4000u;
    sample.cell_avg_mv = 4100u;
    sample.cell_max_mv = 4200u;
    Com_SOC_Update(&sample);
    Com_SOC_GetResult(&result);
    CHECK(!result.full_anchor_used);
    CHECK(result.soc_percent < 100.0f);

    sample.cell_min_mv = 4160u;
    sample.cell_avg_mv = 4175u;
    sample.cell_max_mv = 4190u;
    Com_SOC_Update(&sample);
    Com_SOC_GetResult(&result);
    CHECK(result.full_anchor_used);
    CHECK(result.soc_percent == 100.0f);

    CHECK(Com_SOC_RestorePersistent(&persistent));
    Com_SOC_NotifyAnchorEvent(COM_SOC_ANCHOR_EMPTY_CUTOFF);
    sample.cell_min_mv = 3150u;
    sample.cell_avg_mv = 3160u;
    sample.cell_max_mv = 3170u;
    Com_SOC_Update(&sample);
    Com_SOC_GetResult(&result);
    CHECK(!result.empty_anchor_used);
    CHECK(result.soc_percent > 0.0f);

    sample.cell_min_mv = 3050u;
    sample.cell_avg_mv = 3070u;
    sample.cell_max_mv = 3090u;
    Com_SOC_Update(&sample);
    Com_SOC_GetResult(&result);
    CHECK(result.empty_anchor_used);
    CHECK(result.soc_percent == 0.0f);

    CHECK(Com_SOC_RestorePersistent(&persistent));
    Com_SOC_NotifyAnchorEvent(COM_SOC_ANCHOR_FULL_COMPLETE);
    sample.interval_ms = 11000u;
    sample.cells_valid = false;
    Com_SOC_Update(&sample);
    sample.interval_ms = 1000u;
    sample.cells_valid = true;
    sample.cell_min_mv = 4160u;
    sample.cell_avg_mv = 4175u;
    sample.cell_max_mv = 4190u;
    Com_SOC_Update(&sample);
    Com_SOC_GetResult(&result);
    CHECK(!result.full_anchor_used);
    CHECK(result.soc_percent < 100.0f);
}

static Com_SOH_ConfigTypeDef Test_SohConfig(void)
{
    const Com_SOH_ConfigTypeDef config = {
        .capacity_mah = 1000u,
        .delta_warn_mv = 80u,
        .temp_warn_c = 55,
        .cycle_warn_count = 300u,
        .learn_min_percent = 50u,
        .learn_max_percent = 110u,
    };

    return config;
}

static Com_SOH_SampleTypeDef Test_SohSample(void)
{
    const Com_SOH_SampleTypeDef sample = {
        .interval_ms = 0u,
        .current_valid = true,
        .current_ma = 0,
        .cells_valid = true,
        .cell_delta_mv = 10u,
        .temp_cell_valid = true,
        .temp_cell_c = 25,
        .safety_status_a = 0u,
        .safety_status_b = 0u,
        .safety_status_c = 0u,
        .full_anchor_used = false,
        .empty_anchor_used = false,
    };

    return sample;
}

static void Test_SohCapacityLearning(void)
{
    Com_SOH_ConfigTypeDef config = Test_SohConfig();
    Com_SOH_SampleTypeDef sample = Test_SohSample();
    Com_SOH_ResultTypeDef result;

    Com_SOH_Init(&config);
    sample.full_anchor_used = true;
    Com_SOH_Update(&sample);
    sample.full_anchor_used = false;
    sample.current_ma = -1000;
    sample.interval_ms = 3600000u;
    Com_SOH_Update(&sample);
    sample.current_ma = 0;
    sample.interval_ms = 0u;
    sample.empty_anchor_used = true;
    Com_SOH_Update(&sample);
    Com_SOH_GetResult(&result);

    CHECK(result.capacity_valid);
    CHECK(result.learned_capacity_mah == 1000u);
    CHECK(result.capacity_learning_count == 1u);
    CHECK(result.soh_percent == 100u);
    CHECK(result.cycle_count == 1u);
}

static void Test_SohGapCancelsLearningAndFaultEdges(void)
{
    Com_SOH_ConfigTypeDef config = Test_SohConfig();
    Com_SOH_SampleTypeDef sample = Test_SohSample();
    Com_SOH_ResultTypeDef result;

    Com_SOH_Init(&config);
    sample.full_anchor_used = true;
    Com_SOH_Update(&sample);
    sample.full_anchor_used = false;
    sample.current_valid = false;
    sample.interval_ms = 1000u;
    Com_SOH_Update(&sample);
    sample.current_valid = true;
    sample.interval_ms = 0u;
    sample.empty_anchor_used = true;
    Com_SOH_Update(&sample);
    Com_SOH_GetResult(&result);
    CHECK(!result.capacity_valid);
    CHECK(!result.learning_active);

    sample.empty_anchor_used = false;
    sample.safety_status_a = 1u;
    Com_SOH_Update(&sample);
    Com_SOH_Update(&sample);
    sample.safety_status_a = 0u;
    Com_SOH_Update(&sample);
    sample.safety_status_a = 1u;
    Com_SOH_Update(&sample);
    Com_SOH_GetResult(&result);
    CHECK(result.safety_fault_count == 2u);
}

static void Test_SohPersistentValidation(void)
{
    Com_SOH_ConfigTypeDef config = Test_SohConfig();
    Com_SOH_PersistentTypeDef persistent = {
        .charge_throughput_mah = 10u,
        .discharge_throughput_mah = 20u,
        .cycle_count = 1u,
        .safety_fault_count = 0u,
        .cycle_remainder_mah = 100u,
        .learned_capacity_mah = 900u,
        .capacity_learning_count = 1u,
        .max_delta_mv = 50u,
        .max_temp_c = 45,
    };

    Com_SOH_Init(&config);
    CHECK(Com_SOH_ValidatePersistent(&persistent));
    CHECK(Com_SOH_RestorePersistent(&persistent));
    persistent.cycle_remainder_mah = 1000u;
    CHECK(!Com_SOH_ValidatePersistent(&persistent));
    persistent.cycle_remainder_mah = 100u;
    persistent.learned_capacity_mah = 2000u;
    CHECK(!Com_SOH_ValidatePersistent(&persistent));
}

static void Test_SohExtremeCycleAndCapacityBounds(void)
{
    Com_SOH_ConfigTypeDef config = Test_SohConfig();
    Com_SOH_SampleTypeDef sample = Test_SohSample();
    Com_SOH_PersistentTypeDef persistent = {0};
    Com_SOH_ResultTypeDef result;

    /* 最小容量和最大积分输入必须 O(1) 完成，并饱和而不是循环数十亿次。 */
    config.capacity_mah = 1u;
    Com_SOH_Init(&config);
    sample.current_ma = INT32_MIN;
    sample.interval_ms = UINT32_MAX;
    Com_SOH_Update(&sample);
    Com_SOH_GetResult(&result);
    Com_SOH_ExportPersistent(&persistent);
    CHECK(result.cycle_count == UINT32_MAX);
    CHECK(result.discharge_throughput_mah == UINT32_MAX);
    CHECK(persistent.cycle_remainder_mah < config.capacity_mah);

    /* 超范围容量按与 SOC 一致的 200 Ah 产品上界收敛。 */
    config.capacity_mah = UINT32_MAX;
    Com_SOH_Init(&config);
    sample.current_ma = -200000;
    sample.interval_ms = 3600000u;
    Com_SOH_Update(&sample);
    Com_SOH_GetResult(&result);
    CHECK(result.cycle_count == 1u);

    persistent = (Com_SOH_PersistentTypeDef){0};
    persistent.capacity_learning_count = 1u;
    persistent.learned_capacity_mah = 220000u;
    CHECK(Com_SOH_ValidatePersistent(&persistent));
    persistent.learned_capacity_mah = 220001u;
    CHECK(!Com_SOH_ValidatePersistent(&persistent));
}

static void Test_BoundedIntegerFormatter(void)
{
    char output[96];
    char truncated[5];
    int length;

    length = Com_Format(output,
                        sizeof(output),
                        "%s %02X %04x %d %u %08lx %lu %%",
                        "BMS",
                        0xA5u,
                        0x2Bu,
                        -42,
                        42u,
                        0x1234ul,
                        7ul);
    CHECK(length == 31);
    CHECK(strcmp(output, "BMS A5 002b -42 42 00001234 7 %") == 0);

    length = Com_Format(truncated, sizeof(truncated), "abcdef");
    CHECK(length == 6);
    CHECK(strcmp(truncated, "abcd") == 0);
    CHECK(truncated[sizeof(truncated) - 1u] == '\0');

    length = Com_Format(output, sizeof(output), "%d", INT_MIN);
    CHECK(length == 11);
    CHECK(strcmp(output, "-2147483648") == 0);
    CHECK(Com_Format(NULL, 0u, "%04x", 0x2bu) == 4);
    CHECK(Com_Format(output, sizeof(output), "%s", (const char *)NULL) == 6);
    CHECK(strcmp(output, "(null)") == 0);
    CHECK(Com_Format(output, sizeof(output), "%385u", 1u) == -1);
    CHECK(Com_Format(output, sizeof(output), "%f", 1.0) == -1);
}

int main(void)
{
    Test_OcvTable();
    Test_SocInvalidUntilTrustedSeed();
    Test_SocPersistentAndCoulombCounting();
    Test_SocAnchorGuardsAndTimeout();
    Test_SohCapacityLearning();
    Test_SohGapCancelsLearningAndFaultEdges();
    Test_SohPersistentValidation();
    Test_SohExtremeCycleAndCapacityBounds();
    Test_BoundedIntegerFormatter();

    if (s_failure_count != 0u)
    {
        (void)fprintf(stderr, "%u algorithm check(s) failed\n", s_failure_count);
        return 1;
    }
    (void)puts("all algorithm checks passed");
    return 0;
}
