#ifndef COM_BQ76952_H
#define COM_BQ76952_H

#include <stdint.h>

/* BQ direct-command 温度单位换算；电池参数与 SOC 曲线由 Com_BatteryParam 管理。 */
int16_t Com_BQ76952_Temp0p1KToC(int16_t temp_0p1k);

#endif /* COM_BQ76952_H */
