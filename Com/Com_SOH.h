#ifndef COM_SOH_H
#define COM_SOH_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @file Com_SOH.h
 * @brief 容量 SOH 学习与寿命统计纯算法层。
 *
 * 满电锚点开始累计净放电量，空电锚点完成一次容量学习；累计吞吐量和已完成的
 * 容量结果可由 APP 导出到 EEPROM。复位期间电流未知，因此未完成学习不会跨复位续接。
 * COM 层不直接访问硬件。
 */

typedef struct
{
    uint32_t capacity_mah;
    uint16_t delta_warn_mv;
    int16_t temp_warn_c;
    uint32_t cycle_warn_count;
    uint8_t learn_min_percent;
    uint8_t learn_max_percent;
} Com_SOH_ConfigTypeDef;

typedef struct
{
    uint32_t interval_ms;
    bool current_valid;
    int32_t current_ma;
    bool cells_valid;
    uint16_t cell_delta_mv;
    bool temp_cell_valid;
    int16_t temp_cell_c;
    uint8_t safety_status_a;
    uint8_t safety_status_b;
    uint8_t safety_status_c;
    bool full_anchor_used;
    bool empty_anchor_used;
} Com_SOH_SampleTypeDef;

/**
 * @brief 可跨掉电保存的 SOH 状态。
 * @note 该结构是逻辑数据，不可直接按结构体内存布局写入 EEPROM。
 */
typedef struct
{
    uint32_t charge_throughput_mah;
    uint32_t discharge_throughput_mah;
    uint32_t cycle_count;
    uint32_t safety_fault_count;
    uint32_t cycle_remainder_mah;
    uint32_t learned_capacity_mah;
    uint16_t capacity_learning_count;
    uint16_t max_delta_mv;
    int16_t max_temp_c;
} Com_SOH_PersistentTypeDef;

typedef struct
{
    uint32_t charge_throughput_mah;
    uint32_t discharge_throughput_mah;
    uint32_t cycle_count;
    uint32_t safety_fault_count;
    uint32_t temp_invalid_count;
    uint16_t max_delta_mv;
    int16_t max_temp_c;
    uint32_t learned_capacity_mah;
    uint32_t learning_net_discharge_mah;
    uint16_t capacity_learning_count;
    uint8_t soh_percent;
    uint8_t health_score_percent;
    uint8_t confidence_percent;
    bool capacity_valid;
    bool learning_active;
    bool capacity_updated;
} Com_SOH_ResultTypeDef;

/**
 * @brief 初始化 SOH 统计。
 */
void Com_SOH_Init(const Com_SOH_ConfigTypeDef *config);

/**
 * @brief 根据一次 BQ 采样更新 SOH 统计。
 */
void Com_SOH_Update(const Com_SOH_SampleTypeDef *sample);

/**
 * @brief 只校验持久化字段范围，不修改当前算法状态。
 */
bool Com_SOH_ValidatePersistent(const Com_SOH_PersistentTypeDef *state);

/**
 * @brief 恢复通过 CRC 校验后的持久化状态；未完成的容量学习会安全作废。
 * @return true 表示字段范围有效并已恢复。
 */
bool Com_SOH_RestorePersistent(const Com_SOH_PersistentTypeDef *state);

/**
 * @brief 导出当前可持久化状态。
 */
void Com_SOH_ExportPersistent(Com_SOH_PersistentTypeDef *state);

/**
 * @brief 读取 SOH 当前结果。
 */
void Com_SOH_GetResult(Com_SOH_ResultTypeDef *result);

#endif /* COM_SOH_H */
