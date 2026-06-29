#ifndef COM_SOH_H
#define COM_SOH_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @file Com_SOH.h
 * @brief SOH 纯算法层。
 *
 * 第一版先做教学友好的融合框架：统计吞吐量、等效循环、最大压差、
 * 最高有效温度和 BQ safety 事件。容量学习、内阻学习以后可以继续
 * 填进这个模块，不需要改 APP 的采样流程。
 */

typedef struct
{
    uint32_t capacity_mah;
    uint16_t delta_warn_mv;
    int16_t temp_warn_c;
    uint32_t cycle_warn_count;
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
} Com_SOH_SampleTypeDef;

typedef struct
{
    uint32_t charge_throughput_mah;
    uint32_t discharge_throughput_mah;
    uint32_t cycle_count;
    uint32_t safety_fault_count;
    uint32_t temp_invalid_count;
    uint16_t max_delta_mv;
    int16_t max_temp_c;
    uint8_t soh_percent;
    uint8_t confidence_percent;
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
 * @brief 读取 SOH 当前结果。
 */
void Com_SOH_GetResult(Com_SOH_ResultTypeDef *result);

#endif /* COM_SOH_H */
