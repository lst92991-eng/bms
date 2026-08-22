/**
 * @file bms_fault.c
 * @brief BMS 故障去抖、锁存、恢复和动作聚合实现。
 *
 * @ownership 由 BmsSafetyTask 单任务调用；本模块不访问 RTOS 或 HAL。
 * @safety 计时和计数均使用饱和运算，避免回绕导致故障意外恢复。
 * @timing 所有操作均为固定上界 O(BMS_FAULT_MAX_COUNT)。
 */

#include "bms_fault.h"

#include <string.h>

static uint32_t add_u32_saturated(uint32_t value, uint32_t increment)
{
    if (increment > (UINT32_MAX - value))
    {
        return UINT32_MAX;
    }

    return value + increment;
}

static uint64_t fault_bit(BmsFault_Id id)
{
    return UINT64_C(1) << id;
}

static bool manager_and_id_are_valid(const BmsFault_Manager *manager, BmsFault_Id id)
{
    return (manager != NULL) && manager->initialized &&
           ((size_t)id < manager->descriptor_count);
}

static void set_active(BmsFault_Manager *manager,
                       BmsFault_Id id,
                       uint32_t now_ms)
{
    BmsFault_Runtime *runtime = &manager->runtime[id];
    const BmsFault_Descriptor *descriptor = &manager->descriptors[id];
    const uint64_t bit = fault_bit(id);

    runtime->active = true;
    runtime->recovery_elapsed_ms = 0u;
    runtime->last_change_ms = now_ms;
    runtime->occurrence_count = add_u32_saturated(runtime->occurrence_count, 1u);

    if (runtime->occurrence_count == 1u)
    {
        runtime->first_seen_ms = now_ms;
    }

    if (descriptor->latch_policy != BMS_FAULT_LATCH_NONE)
    {
        runtime->latched = true;
        manager->latched_mask |= bit;
    }

    manager->active_mask |= bit;
    manager->history_mask |= bit;
}

static void set_inactive(BmsFault_Manager *manager,
                         BmsFault_Id id,
                         uint32_t now_ms)
{
    BmsFault_Runtime *runtime = &manager->runtime[id];
    const BmsFault_Descriptor *descriptor = &manager->descriptors[id];
    const uint64_t bit = fault_bit(id);

    runtime->active = false;
    runtime->trip_elapsed_ms = 0u;
    runtime->recovery_elapsed_ms = 0u;
    runtime->last_change_ms = now_ms;
    manager->active_mask &= ~bit;

    if (descriptor->latch_policy == BMS_FAULT_LATCH_NONE)
    {
        runtime->latched = false;
        manager->latched_mask &= ~bit;
    }
}

BmsStatus BmsFault_Init(BmsFault_Manager *manager,
                        const BmsFault_Descriptor *descriptors,
                        size_t descriptor_count)
{
    if ((manager == NULL) || (descriptors == NULL) || (descriptor_count == 0u) ||
        (descriptor_count > BMS_FAULT_MAX_COUNT))
    {
        return BMS_STATUS_INVALID_ARGUMENT;
    }

    for (size_t index = 0u; index < descriptor_count; index++)
    {
        const BmsFault_Descriptor *descriptor = &descriptors[index];

        if (((size_t)descriptor->id != index) ||
            (descriptor->severity > BMS_FAULT_SEVERITY_LOCKOUT) ||
            (descriptor->latch_policy > BMS_FAULT_LATCH_POWER_CYCLE))
        {
            return BMS_STATUS_CONFIG_MISMATCH;
        }
    }

    (void)memset(manager, 0, sizeof(*manager));
    manager->descriptors = descriptors;
    manager->descriptor_count = descriptor_count;
    manager->initialized = true;
    return BMS_STATUS_OK;
}

BmsStatus BmsFault_Update(BmsFault_Manager *manager,
                          BmsFault_Id id,
                          bool raw_active,
                          uint32_t elapsed_ms,
                          uint32_t now_ms)
{
    BmsFault_Runtime *runtime;
    const BmsFault_Descriptor *descriptor;

    if (!manager_and_id_are_valid(manager, id))
    {
        return (manager == NULL) ? BMS_STATUS_INVALID_ARGUMENT : BMS_STATUS_STATE_ERROR;
    }

    runtime = &manager->runtime[id];
    descriptor = &manager->descriptors[id];
    runtime->raw_active = raw_active;

    if (raw_active)
    {
        runtime->recovery_elapsed_ms = 0u;

        if (!runtime->active)
        {
            runtime->trip_elapsed_ms =
                add_u32_saturated(runtime->trip_elapsed_ms, elapsed_ms);

            if (runtime->trip_elapsed_ms >= descriptor->trip_debounce_ms)
            {
                set_active(manager, id, now_ms);
            }
        }

        return BMS_STATUS_OK;
    }

    runtime->trip_elapsed_ms = 0u;

    if (runtime->active)
    {
        runtime->recovery_elapsed_ms =
            add_u32_saturated(runtime->recovery_elapsed_ms, elapsed_ms);

        if (runtime->recovery_elapsed_ms >= descriptor->recovery_debounce_ms)
        {
            set_inactive(manager, id, now_ms);
        }
    }
    else
    {
        runtime->recovery_elapsed_ms = 0u;
    }

    return BMS_STATUS_OK;
}

BmsStatus BmsFault_RequestClear(BmsFault_Manager *manager,
                                BmsFault_Id id,
                                uint32_t now_ms)
{
    BmsFault_Runtime *runtime;
    const BmsFault_Descriptor *descriptor;
    const uint64_t bit = fault_bit(id);

    if (!manager_and_id_are_valid(manager, id))
    {
        return (manager == NULL) ? BMS_STATUS_INVALID_ARGUMENT : BMS_STATUS_STATE_ERROR;
    }

    runtime = &manager->runtime[id];
    descriptor = &manager->descriptors[id];

    if (descriptor->latch_policy != BMS_FAULT_LATCH_MANUAL_CLEAR)
    {
        return BMS_STATUS_STATE_ERROR;
    }

    if (runtime->raw_active || runtime->active)
    {
        return BMS_STATUS_STATE_ERROR;
    }

    runtime->latched = false;
    runtime->last_change_ms = now_ms;
    manager->latched_mask &= ~bit;
    return BMS_STATUS_OK;
}

bool BmsFault_IsActive(const BmsFault_Manager *manager, BmsFault_Id id)
{
    return manager_and_id_are_valid(manager, id) &&
           ((manager->active_mask & fault_bit(id)) != 0u);
}

bool BmsFault_IsLatched(const BmsFault_Manager *manager, BmsFault_Id id)
{
    return manager_and_id_are_valid(manager, id) &&
           ((manager->latched_mask & fault_bit(id)) != 0u);
}

bool BmsFault_IsEffective(const BmsFault_Manager *manager, BmsFault_Id id)
{
    return BmsFault_IsActive(manager, id) || BmsFault_IsLatched(manager, id);
}

uint32_t BmsFault_GetActionMask(const BmsFault_Manager *manager)
{
    uint32_t action_mask = 0u;

    if ((manager == NULL) || !manager->initialized)
    {
        return 0u;
    }

    for (size_t index = 0u; index < manager->descriptor_count; index++)
    {
        const BmsFault_Id id = (BmsFault_Id)index;

        if (BmsFault_IsEffective(manager, id))
        {
            action_mask |= manager->descriptors[index].action_mask;
        }
    }

    return action_mask;
}

const BmsFault_Runtime *BmsFault_GetRuntime(const BmsFault_Manager *manager,
                                            BmsFault_Id id)
{
    if (!manager_and_id_are_valid(manager, id))
    {
        return NULL;
    }

    return &manager->runtime[id];
}
