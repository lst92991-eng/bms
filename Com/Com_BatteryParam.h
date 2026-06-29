#ifndef COM_BATTERY_PARAM_H
#define COM_BATTERY_PARAM_H

#include <stdint.h>

/**
 * @file Com_BatteryParam.h
 * @brief 电芯参数与 OCV-SOC 曲线。
 *
 * 这里放“电池本体参数”，不放 BQ76952 协议细节。后续换电芯、
 * 换实测 OCV 曲线、加入 SOC/T 二维参数表，都优先改本模块。
 */

#define COM_BATTERY_PARAM_CELL_COUNT             (6u)
#define COM_BATTERY_PARAM_CAP_TYP_MAH           (5000u)
#define COM_BATTERY_PARAM_CELL_FULL_MV          (4200u)
#define COM_BATTERY_PARAM_PACK_FULL_MV          (25200u)

uint8_t Com_BatteryParam_GetPercentByVoltage(uint16_t cell_mv);
uint16_t Com_BatteryParam_GetSoc0p01ByVoltage(uint16_t cell_mv);
uint16_t Com_BatteryParam_GetVoltageBySoc0p01(uint16_t soc_0p01);
float Com_BatteryParam_GetOcvSlopeMvPerPercent(uint16_t soc_0p01);

#endif /* COM_BATTERY_PARAM_H */
