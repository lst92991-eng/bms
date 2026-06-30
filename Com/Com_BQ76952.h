#ifndef COM_BQ76952_H
#define COM_BQ76952_H

#include <stdint.h>

/*
 * BQ76952 的公共换算层只保留和芯片协议直接相关的内容。
 * 电芯 OCV/SOC 曲线已经移到 Com_BatteryParam，避免同一件事在两个 Com 文件里绕来绕去。
 */

#define COM_BQ76952_CELL_COUNT                  (6u)
#define COM_BQ76952_CELL_CAP_TYP_MAH            (5000u)
#define COM_BQ76952_CELL_CAP_MIN_MAH            (4900u)
#define COM_BQ76952_CELL_NOMINAL_MV             (3650u)
#define COM_BQ76952_CELL_FULL_STD_MV            (4200u)
#define COM_BQ76952_PACK_FULL_STD_MV            (25200u)

#define COM_BQ76952_CELL_CHARGE_5A_MV           (4100u)
#define COM_BQ76952_PACK_CHARGE_5A_MV           (24600u)

#define COM_BQ76952_CHARGE_TARGET_MA            (5000)
#define COM_BQ76952_DISCHARGE_TARGET_MA         (10000)
#define COM_BQ76952_DISCHARGE_MAX_CONT_MA       (15000)

#define COM_BQ76952_NTC_R25_OHM                 (10000u)
#define COM_BQ76952_NTC_BETA_K                  (3950u)
#define COM_BQ76952_NTC_TOLERANCE_PERCENT       (1u)

int16_t Com_BQ76952_Temp0p1KToC(int16_t temp_0p1k);

#endif /* COM_BQ76952_H */
