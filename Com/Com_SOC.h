#ifndef COM_SOC_H
#define COM_SOC_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @file Com_SOC.h
 * @brief SOC 核心算法层。
 *
 * 第一阶段算法目标：
 * 1. Ah 积分负责连续 SOC 推进；
 * 2. 二阶 RC 模型估计极化电压，避免动态端电压直接查 OCV；
 * 3. EKF 在低电流并静置足够久后用单体电压修正 SOC；
 * 4. 显示 SOC 单独限速/滤波，不污染真实估计值。
 */

typedef enum
{
    COM_SOC_CURRENT_POS_CHARGE = 0,
    COM_SOC_CURRENT_POS_DISCHARGE
} Com_SOC_CurrentSignTypeDef;

typedef enum
{
    COM_SOC_SEED_NONE = 0,
    COM_SOC_SEED_DEFAULT,
    COM_SOC_SEED_OCV,
    COM_SOC_SEED_NVM
} Com_SOC_SeedSourceTypeDef;

typedef struct
{
    uint32_t capacity_mah;
    float default_soc_percent;
    Com_SOC_CurrentSignTypeDef current_sign;

    int32_t current_deadband_ma;
    uint32_t rest_need_ms;
    int32_t rest_current_ma;
    uint16_t rest_voltage_stable_mv;

    float r0_ohm;
    float r1_ohm;
    float c1_f;
    float r2_ohm;
    float c2_f;

    float process_q_soc;
    float process_q_v;
    float process_q_v1;
    float process_q_v2;
    float measure_r_mv;
    bool dynamic_ekf_enable;
    float measure_r_dynamic_mv;
    float residual_reject_mv;
    int32_t dynamic_current_delta_limit_ma;
    uint32_t dynamic_current_stable_ms;

    uint16_t full_cell_mv;
    uint16_t empty_cell_mv;
    int32_t full_current_ma;
    int32_t empty_current_ma;
    uint32_t full_anchor_hold_ms;
    uint32_t empty_anchor_hold_ms;

    float display_rise_percent_per_s;
    float display_fall_percent_per_s;
} Com_SOC_ConfigTypeDef;

typedef struct
{
    uint32_t interval_ms;
    bool current_valid;
    int32_t current_ma;

    bool cells_valid;
    uint16_t cell_min_mv;
    uint16_t cell_max_mv;
    uint16_t cell_avg_mv;

    bool temp_valid;
    int16_t temp_c;
    bool soh_valid;
    uint8_t soh_percent;
} Com_SOC_SampleTypeDef;

typedef struct
{
    float soc_percent;
    float display_percent;
    uint32_t remain_mah;
    bool seeded;
    Com_SOC_SeedSourceTypeDef seed_source;
    bool rest_ready;
    bool voltage_update_used;
    bool full_anchor_used;
    bool empty_anchor_used;
    bool dynamic_ekf_used;
    uint8_t confidence_percent;
    float model_cell_mv;
    float ocv_cell_mv;
    float polar_v1_mv;
    float polar_v2_mv;
    float residual_mv;
    float kalman_gain_soc;
    float kalman_gain_v1;
    float kalman_gain_v2;
    float p_soc;
    float p_v1;
    float p_v2;
    float active_capacity_mah;
    float measured_cell_mv;
    float ir0_mv;
    float soc_raw_percent;
    uint32_t rest_ms;
    uint32_t voltage_stable_ms;
    uint32_t current_stable_ms;
    bool ekf_used_avg_voltage;
} Com_SOC_ResultTypeDef;

/**
 * @brief 初始化 SOC 算法。
 */
void Com_SOC_Init(const Com_SOC_ConfigTypeDef *config);

/**
 * @brief 根据一次采样更新 SOC。
 */
void Com_SOC_Update(const Com_SOC_SampleTypeDef *sample);

/**
 * @brief 获取 SOC 全量结果。
 */
void Com_SOC_GetResult(Com_SOC_ResultTypeDef *result);

/**
 * @brief 获取 EKF 估计 SOC。
 */
float Com_SOC_GetSocPercent(void);

/**
 * @brief 获取显示 SOC。
 */
float Com_SOC_GetDisplayPercent(void);

/**
 * @brief 获取剩余容量估算。
 */
uint32_t Com_SOC_GetRemainMah(void);

/**
 * @brief 判断是否完成上电播种。
 */
bool Com_SOC_IsSeeded(void);

#endif /* COM_SOC_H */
