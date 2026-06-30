#ifndef COM_SOC_H
#define COM_SOC_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @file Com_SOC.h
 * @brief SOC 教学算法层。
 *
 * 当前版本保留三条主线：
 * 1. Ah 积分负责连续跟踪；
 * 2. OCV 查表负责上电播种和静置校正；
 * 3. 一维 Kalman 负责把 OCV 校正“慢慢拉回来”。
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

    float kalman_q;
    float kalman_r;
    float ocv_update_limit_percent;

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
    uint8_t confidence_percent;

    float ocv_soc_percent;
    float ocv_cell_mv;
    float residual_percent;
    float kalman_gain_soc;
    float p_soc;
    float active_capacity_mah;
    float measured_cell_mv;
    float soc_raw_percent;
    uint32_t rest_ms;
    uint32_t voltage_stable_ms;
} Com_SOC_ResultTypeDef;

void Com_SOC_Init(const Com_SOC_ConfigTypeDef *config);
void Com_SOC_Update(const Com_SOC_SampleTypeDef *sample);
void Com_SOC_GetResult(Com_SOC_ResultTypeDef *result);

#endif /* COM_SOC_H */
