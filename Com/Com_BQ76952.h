#ifndef COM_BQ76952_H
#define COM_BQ76952_H

#include <stdint.h>

/**
 * @file Com_BQ76952.h
 * @brief BQ76952 业务辅助层。
 *
 * 本文件故意保持得很薄，风格对齐旧项目 `Com_Bq769`：
 * 1. 不访问 I2C；
 * 2. 不读写 BQ76952 direct command / subcommand / Data Memory；
 * 3. 不做均衡、FET、保护策略；
 * 4. 只提供查表、单位换算和已经确认的电池/NTC 参数。
 *
 * 真正的 BQ 业务流程放在 `App_BatMan.c`，由 APP 直接调用
 * `Int_BQ76952_*` 基本方法。这样教学时可以从 APP 一路读到
 * INT，不需要在 COM 里再跳一层厚封装。
 */

/* 当前项目使用 EVE INR21700/50E，第一版按 6S1P 处理。 */
#define COM_BQ76952_CELL_COUNT                  (6u)
#define COM_BQ76952_CELL_CAP_TYP_MAH            (5000u)
#define COM_BQ76952_CELL_CAP_MIN_MAH            (4900u)
#define COM_BQ76952_CELL_NOMINAL_MV             (3650u)
#define COM_BQ76952_CELL_FULL_STD_MV            (4200u)
#define COM_BQ76952_PACK_FULL_STD_MV            (25200u)

/*
 * 5A 充电对应 EVE 50E 规格书中的最大持续充电条件：
 * 4.10V/cell，6S 即 24.6V。SC8815 具体充电闭环暂时不放在 BQ 逻辑里。
 */
#define COM_BQ76952_CELL_CHARGE_5A_MV           (4100u)
#define COM_BQ76952_PACK_CHARGE_5A_MV           (24600u)

/* 放电目标：系统目标 10A，电芯规格允许最大持续 15A。 */
#define COM_BQ76952_CHARGE_TARGET_MA            (5000)
#define COM_BQ76952_DISCHARGE_TARGET_MA         (10000)
#define COM_BQ76952_DISCHARGE_MAX_CONT_MA       (15000)

/*
 * TS1/TS3 热敏信息来自你补充的实物参数：
 * MF52AT，10K，B=3950，1%，PH2.0，250mm。
 * BQ76952 的 TSx Temperature direct command 已返回 0.1K 温度值，
 * 因此本层暂时只记录参数，不在 COM 里强行做 ADC 分压反算。
 */
#define COM_BQ76952_NTC_R25_OHM                 (10000u)
#define COM_BQ76952_NTC_BETA_K                  (3950u)
#define COM_BQ76952_NTC_TOLERANCE_PERCENT       (1u)

/**
 * @brief 把 BQ76952 温度 direct command 的 0.1K 单位转换成摄氏度。
 * @param temp_0p1k BQ 返回的温度，单位 0.1K。
 * @return 四舍五入后的摄氏度，单位 C。
 */
int16_t Com_BQ76952_Temp0p1KToC(int16_t temp_0p1k);

/**
 * @brief 根据单体电压粗略估算 SOC 百分比。
 * @param cell_mv 单体静置或低电流近似 OCV 电压，单位 mV。
 * @return 0~100 的 SOC 粗估值。
 *
 * 说明：
 * 这里不是最终 EVE 50E 精密 OCV 曲线，只用于上电初始 SOC
 * 或低电流时的教学型修正。长期版本应替换成实测/厂家曲线。
 */
uint8_t Com_BQ76952_GetPercentByVoltage(uint16_t cell_mv);

/**
 * @brief 根据单体 OCV 查询 SOC，精度 0.01%。
 * @param cell_mv 单体开路电压或接近开路电压，单位 mV。
 * @return 0~10000，表示 0.00%~100.00%。
 */
uint16_t Com_BQ76952_GetSoc0p01ByVoltage(uint16_t cell_mv);

/**
 * @brief 根据 SOC 反查单体 OCV。
 * @param soc_0p01 0~10000，表示 0.00%~100.00%。
 * @return 单体 OCV，单位 mV。
 */
uint16_t Com_BQ76952_GetVoltageBySoc0p01(uint16_t soc_0p01);

/**
 * @brief 获取 OCV 曲线在当前 SOC 附近的斜率。
 * @param soc_0p01 0~10000，表示 0.00%~100.00%。
 * @return 单位 mV/%SOC，用于 EKF 观测矩阵 H。
 */
float Com_BQ76952_GetOcvSlopeMvPerPercent(uint16_t soc_0p01);

#endif /* COM_BQ76952_H */
