#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "bms_fault.h"

#define TEST_ACTION_STOP_CHARGE    (UINT32_C(1) << 0u)
#define TEST_ACTION_STOP_DISCHARGE (UINT32_C(1) << 1u)

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

static const BmsFault_Descriptor s_descriptors[] =
{
    {
        .id = 0u,
        .severity = BMS_FAULT_SEVERITY_TRIP,
        .latch_policy = BMS_FAULT_LATCH_NONE,
        .trip_debounce_ms = 100u,
        .recovery_debounce_ms = 200u,
        .action_mask = TEST_ACTION_STOP_CHARGE,
    },
    {
        .id = 1u,
        .severity = BMS_FAULT_SEVERITY_LATCHED_TRIP,
        .latch_policy = BMS_FAULT_LATCH_MANUAL_CLEAR,
        .trip_debounce_ms = 0u,
        .recovery_debounce_ms = 50u,
        .action_mask = TEST_ACTION_STOP_DISCHARGE,
    },
};

static BmsFault_Manager make_manager(void)
{
    BmsFault_Manager manager;
    const BmsStatus status =
        BmsFault_Init(&manager,
                      s_descriptors,
                      sizeof(s_descriptors) / sizeof(s_descriptors[0]));

    EXPECT_EQ_U32(BMS_STATUS_OK, status);
    return manager;
}

static void test_trip_debounce_and_auto_recovery(void)
{
    BmsFault_Manager manager = make_manager();

    EXPECT_EQ_U32(BMS_STATUS_OK, BmsFault_Update(&manager, 0u, true, 40u, 40u));
    EXPECT_FALSE(BmsFault_IsActive(&manager, 0u));

    EXPECT_EQ_U32(BMS_STATUS_OK, BmsFault_Update(&manager, 0u, true, 59u, 99u));
    EXPECT_FALSE(BmsFault_IsActive(&manager, 0u));

    EXPECT_EQ_U32(BMS_STATUS_OK, BmsFault_Update(&manager, 0u, true, 1u, 100u));
    EXPECT_TRUE(BmsFault_IsActive(&manager, 0u));
    EXPECT_EQ_U32(TEST_ACTION_STOP_CHARGE, BmsFault_GetActionMask(&manager));

    EXPECT_EQ_U32(BMS_STATUS_OK, BmsFault_Update(&manager, 0u, false, 199u, 299u));
    EXPECT_TRUE(BmsFault_IsActive(&manager, 0u));

    EXPECT_EQ_U32(BMS_STATUS_OK, BmsFault_Update(&manager, 0u, false, 1u, 300u));
    EXPECT_FALSE(BmsFault_IsActive(&manager, 0u));
    EXPECT_FALSE(BmsFault_IsLatched(&manager, 0u));
    EXPECT_EQ_U32(0u, BmsFault_GetActionMask(&manager));
}

static void test_manual_latch_persists_until_clear(void)
{
    BmsFault_Manager manager = make_manager();
    const BmsFault_Runtime *runtime;

    EXPECT_EQ_U32(BMS_STATUS_OK, BmsFault_Update(&manager, 1u, true, 0u, 10u));
    EXPECT_TRUE(BmsFault_IsActive(&manager, 1u));
    EXPECT_TRUE(BmsFault_IsLatched(&manager, 1u));
    EXPECT_EQ_U32(BMS_STATUS_STATE_ERROR, BmsFault_RequestClear(&manager, 1u, 11u));

    EXPECT_EQ_U32(BMS_STATUS_OK, BmsFault_Update(&manager, 1u, false, 49u, 59u));
    EXPECT_TRUE(BmsFault_IsActive(&manager, 1u));

    EXPECT_EQ_U32(BMS_STATUS_OK, BmsFault_Update(&manager, 1u, false, 1u, 60u));
    EXPECT_FALSE(BmsFault_IsActive(&manager, 1u));
    EXPECT_TRUE(BmsFault_IsLatched(&manager, 1u));
    EXPECT_EQ_U32(TEST_ACTION_STOP_DISCHARGE, BmsFault_GetActionMask(&manager));

    EXPECT_EQ_U32(BMS_STATUS_OK, BmsFault_RequestClear(&manager, 1u, 61u));
    EXPECT_FALSE(BmsFault_IsLatched(&manager, 1u));
    EXPECT_EQ_U32(0u, BmsFault_GetActionMask(&manager));

    runtime = BmsFault_GetRuntime(&manager, 1u);
    EXPECT_TRUE(runtime != NULL);
    if (runtime != NULL)
    {
        EXPECT_EQ_U32(1u, runtime->occurrence_count);
        EXPECT_EQ_U32(10u, runtime->first_seen_ms);
        EXPECT_EQ_U32(61u, runtime->last_change_ms);
    }
}

static void test_multiple_fault_actions_are_aggregated(void)
{
    BmsFault_Manager manager = make_manager();

    EXPECT_EQ_U32(BMS_STATUS_OK, BmsFault_Update(&manager, 0u, true, 100u, 100u));
    EXPECT_EQ_U32(BMS_STATUS_OK, BmsFault_Update(&manager, 1u, true, 0u, 100u));
    EXPECT_EQ_U32(TEST_ACTION_STOP_CHARGE | TEST_ACTION_STOP_DISCHARGE,
                  BmsFault_GetActionMask(&manager));
}

static void test_invalid_descriptor_table_is_rejected(void)
{
    BmsFault_Manager manager;
    const BmsFault_Descriptor invalid[] =
    {
        {
            .id = 1u,
            .severity = BMS_FAULT_SEVERITY_TRIP,
            .latch_policy = BMS_FAULT_LATCH_NONE,
            .trip_debounce_ms = 0u,
            .recovery_debounce_ms = 0u,
            .action_mask = 0u,
        },
    };

    EXPECT_EQ_U32(BMS_STATUS_CONFIG_MISMATCH,
                  BmsFault_Init(&manager, invalid, 1u));
}

int main(void)
{
    test_trip_debounce_and_auto_recovery();
    test_manual_latch_persists_until_clear();
    test_multiple_fault_actions_are_aggregated();
    test_invalid_descriptor_table_is_rejected();

    if (s_failures != 0u)
    {
        (void)printf("%u test assertion(s) failed\n", s_failures);
        return 1;
    }

    (void)printf("bms_fault: all tests passed\n");
    return 0;
}
