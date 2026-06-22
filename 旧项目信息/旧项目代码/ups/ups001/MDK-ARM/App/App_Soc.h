#ifndef __APP_SOC_H_
#define __APP_SOC_H_

/**
 * @file App_Soc.h
 * @brief 基于(扩展)卡尔曼滤波(EKF)的SOC估算模块
 *
 * 设计目标：
 * - 预测：用电流积分(库仑计量)推进SOC
 * - 校正：用端电压与OCV曲线(查表)对SOC做观测校正
 *
 * 重要说明（与本项目数据一致）：
 * - current_a 的符号沿用 App_BatMan.c 的约定：current_a > 0 表示充电，<=0 表示放电/静置
 * - pack_mv 为电池包总电压，单位 mV（App_BatMan.c 中 bat_mv）
 * - OCV表使用 Com_Bq769.c 中的 voltage_soc_ocv_table[101]（每节电芯 0~100% 的 OCV(mV)）
 */

#include <stdint.h>
#include <stdbool.h>

 

/**
 * @brief SOC EKF 配置参数
 */
typedef struct
{
    float capacity_ah;               /**< 电池包容量(Ah)，需要按你的电池实际容量填写 */
    float r0_ohm;                    /**< 电池包等效直流内阻(Ohm)，用于 I*R 的端电压补偿 */
    uint8_t cell_num;                /**< 串联电芯数 */

    float p_sigma_soc_per_s;               /**< 过程噪声方差系数(soc^2/s)，越大越“相信电流积分不准” */
    float r_sigma_soc;                 /**< SOC测量噪声标准差(0~1)，例如0.03=3% */
    float max_update_current_a;      /**< 仅在 |I| <= 该阈值时才使用电压观测做SOC校正（近似静置/小电流） */
    uint16_t update_need_low_current_cnt; /**< 需连续满足 |I| <= max_update_current_a 的次数，才允许进行电压观测校正（用于充放电结束后的“冷却”） */
    float r_sigma_mv;                  /**< 电压观测噪声(mV)，越大越不信电压（平台期更稳，但收敛更慢）5-15mv */
} App_Soc_Config_t;



 typedef struct
 {
     App_Soc_Config_t cfg;
     bool inited; //至少要做过一次同步电压观测后 ，数据才可被读取
     uint16_t low_current_cnt; // 连续低电流计数：用于“冷却”后才允许进行电压观测更新
     float soc;  // 状态：SOC，范围0~1
     float P;  // 累计预测方差
 } App_Soc_State_t;



/**
 * @brief 初始化SOC EKF（使用自定义参数）
 * @param cfg 配置参数指针
 * @return true 初始化成功
 */
void App_Soc_Init(void);

 

/**
 * @brief EKF更新一次（预测+可选电压校正）
 * @param delta_ms 时间间隔(ms)
 * @param pack_mv 电池包端电压(mV)
 * @param current_a 电流(A)，正=充电，负=放电
 * @return 当前SOC(%)，范围0~100
 */
float App_Soc_Update(uint32_t delta_ms, float pack_mv, float current_a);

/**
 * @brief 获取当前SOC(%)，范围0~100
 */
float App_Soc_GetSocPercent(void);

/**
 * @brief 强制设置SOC(%)，用于上电初始化/校准
 */
void App_Soc_SetSocPercent(float soc_percent);

 

 

#endif // __APP_SOC_H_
