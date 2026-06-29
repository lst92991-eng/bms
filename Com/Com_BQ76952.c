#include "Com_BQ76952.h"

#include "Com_BatteryParam.h"

int16_t Com_BQ76952_Temp0p1KToC(int16_t temp_0p1k)
{
    int32_t temp_c_x10;

    /*
     * BQ76952 温度 direct command 单位是 0.1K。
     * 这里只做协议单位换算，电池 OCV/SOC 曲线放在 Com_BatteryParam。
     */
    temp_c_x10 = (int32_t)temp_0p1k - 2731;

    if (temp_c_x10 >= 0)
    {
        return (int16_t)((temp_c_x10 + 5) / 10);
    }

    return (int16_t)((temp_c_x10 - 5) / 10);
}

uint8_t Com_BQ76952_GetPercentByVoltage(uint16_t cell_mv)
{
    return Com_BatteryParam_GetPercentByVoltage(cell_mv);
}

uint16_t Com_BQ76952_GetSoc0p01ByVoltage(uint16_t cell_mv)
{
    return Com_BatteryParam_GetSoc0p01ByVoltage(cell_mv);
}

uint16_t Com_BQ76952_GetVoltageBySoc0p01(uint16_t soc_0p01)
{
    return Com_BatteryParam_GetVoltageBySoc0p01(soc_0p01);
}

float Com_BQ76952_GetOcvSlopeMvPerPercent(uint16_t soc_0p01)
{
    return Com_BatteryParam_GetOcvSlopeMvPerPercent(soc_0p01);
}
