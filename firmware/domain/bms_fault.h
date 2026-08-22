#ifndef BMS_FAULT_H
#define BMS_FAULT_H

/**
 * @file bms_fault.h
 * @brief BMS 故障去抖、锁存、恢复和动作聚合公共接口。
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bms_status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BMS_FAULT_MAX_COUNT (64u)

typedef uint8_t BmsFault_Id;

typedef enum
{
    BMS_FAULT_SEVERITY_INFO = 0,
    BMS_FAULT_SEVERITY_WARNING,
    BMS_FAULT_SEVERITY_DERATE,
    BMS_FAULT_SEVERITY_TRIP,
    BMS_FAULT_SEVERITY_LATCHED_TRIP,
    BMS_FAULT_SEVERITY_LOCKOUT
} BmsFault_Severity;

typedef enum
{
    BMS_FAULT_LATCH_NONE = 0,
    BMS_FAULT_LATCH_MANUAL_CLEAR,
    BMS_FAULT_LATCH_POWER_CYCLE
} BmsFault_LatchPolicy;

typedef struct
{
    BmsFault_Id id;
    BmsFault_Severity severity;
    BmsFault_LatchPolicy latch_policy;
    uint32_t trip_debounce_ms;
    uint32_t recovery_debounce_ms;
    uint32_t action_mask;
} BmsFault_Descriptor;

typedef struct
{
    bool raw_active;
    bool active;
    bool latched;
    uint32_t trip_elapsed_ms;
    uint32_t recovery_elapsed_ms;
    uint32_t occurrence_count;
    uint32_t first_seen_ms;
    uint32_t last_change_ms;
} BmsFault_Runtime;

typedef struct
{
    const BmsFault_Descriptor *descriptors;
    size_t descriptor_count;
    BmsFault_Runtime runtime[BMS_FAULT_MAX_COUNT];
    uint64_t active_mask;
    uint64_t latched_mask;
    uint64_t history_mask;
    bool initialized;
} BmsFault_Manager;

/**
 * @brief 初始化故障管理器并验证描述表。
 *
 * @param[out] manager 故障管理器。
 * @param[in] descriptors 描述表；表索引必须与 descriptor.id 一致。
 * @param[in] descriptor_count 描述数量，范围 1..BMS_FAULT_MAX_COUNT。
 * @return BMS_STATUS_OK 或参数/配置错误。
 *
 * @post 成功后所有故障均为 inactive、unlatched，历史和计数清零。
 * @concurrency 只能由故障状态所有者调用。
 */
BmsStatus BmsFault_Init(BmsFault_Manager *manager,
                        const BmsFault_Descriptor *descriptors,
                        size_t descriptor_count);

/**
 * @brief 更新单个故障的原始条件，并推进去抖和恢复状态。
 *
 * @param[in,out] manager 故障管理器。
 * @param[in] id 故障 ID。
 * @param[in] raw_active 当前原始故障条件。
 * @param[in] elapsed_ms 自上次更新此故障以来的时间。
 * @param[in] now_ms 当前单调时间戳，仅用于诊断记录。
 * @return BMS_STATUS_OK 或参数/状态错误。
 *
 * @safety 手动/掉电锁存故障在原始条件恢复后仍保持动作，直到合法清除或复位。
 */
BmsStatus BmsFault_Update(BmsFault_Manager *manager,
                          BmsFault_Id id,
                          bool raw_active,
                          uint32_t elapsed_ms,
                          uint32_t now_ms);

/**
 * @brief 请求清除一个允许手动清除且原始条件已恢复的锁存故障。
 *
 * @return BMS_STATUS_OK 表示已清除；BMS_STATUS_STATE_ERROR 表示条件不允许。
 */
BmsStatus BmsFault_RequestClear(BmsFault_Manager *manager,
                                BmsFault_Id id,
                                uint32_t now_ms);

bool BmsFault_IsActive(const BmsFault_Manager *manager, BmsFault_Id id);
bool BmsFault_IsLatched(const BmsFault_Manager *manager, BmsFault_Id id);
bool BmsFault_IsEffective(const BmsFault_Manager *manager, BmsFault_Id id);
uint32_t BmsFault_GetActionMask(const BmsFault_Manager *manager);
const BmsFault_Runtime *BmsFault_GetRuntime(const BmsFault_Manager *manager,
                                            BmsFault_Id id);

#ifdef __cplusplus
}
#endif

#endif /* BMS_FAULT_H */
