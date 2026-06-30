#include "Com_BatteryParam.h"

typedef struct
{
    uint16_t mv;
    uint8_t percent;
} Com_BatteryParam_OcvPointTypeDef;

/*
 * EVE 50E 第一版粗 OCV-SOC 表。
 * 当前点数故意少，便于教学和 bring-up；正式算法应替换成实测静置 OCV 曲线。
 */
static const Com_BatteryParam_OcvPointTypeDef voltage_soc_ocv_table[] =
{
    {2500u, 0u},
    {3000u, 5u},
    {3300u, 10u},
    {3500u, 20u},
    {3600u, 30u},
    {3650u, 40u},
    {3700u, 50u},
    {3750u, 60u},
    {3850u, 75u},
    {3950u, 90u},
    {4100u, 98u},
    {4200u, 100u},
};

uint16_t Com_BatteryParam_GetSoc0p01ByVoltage(uint16_t cell_mv)
{
    const uint8_t table_count = (uint8_t)(sizeof(voltage_soc_ocv_table) /
                                         sizeof(voltage_soc_ocv_table[0]));

    if (cell_mv <= voltage_soc_ocv_table[0].mv)
    {
        return (uint16_t)voltage_soc_ocv_table[0].percent * 100u;
    }

    for (uint8_t i = 0u; i < (uint8_t)(table_count - 1u); i++)
    {
        const Com_BatteryParam_OcvPointTypeDef *left = &voltage_soc_ocv_table[i];
        const Com_BatteryParam_OcvPointTypeDef *right = &voltage_soc_ocv_table[i + 1u];

        if (cell_mv <= right->mv)
        {
            uint32_t dv = (uint32_t)right->mv - left->mv;
            uint32_t dp = (uint32_t)right->percent - left->percent;
            uint32_t off = (uint32_t)cell_mv - left->mv;

            if (dv == 0u)
            {
                return (uint16_t)left->percent * 100u;
            }

            return (uint16_t)(((uint32_t)left->percent * 100u) +
                              ((off * dp * 100u) / dv));
        }
    }

    return (uint16_t)voltage_soc_ocv_table[table_count - 1u].percent * 100u;
}
