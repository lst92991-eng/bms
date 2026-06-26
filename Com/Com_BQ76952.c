#include "Com_BQ76952.h"

/**
 * @brief EVE 50E 第一版粗 OCV-SOC 表。
 *
 * 表格只承担“薄 COM 查表”的职责，不参与 BQ 访问。
 * 点数故意少，便于教学时看清插值关系；后续拿到 EVE 50E
 * 实测曲线后，只替换这张表即可。
 */
typedef struct
{
    uint16_t mv;
    uint8_t percent;
} Com_BQ76952_OcvPointTypeDef;

static const Com_BQ76952_OcvPointTypeDef voltage_soc_ocv_table[] =
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

int16_t Com_BQ76952_Temp0p1KToC(int16_t temp_0p1k)
{
    int32_t temp_c_x10;

    /*
     * BQ76952 TS/内部温度 direct command 的单位是 0.1K。
     * 摄氏度 = K - 273.15。这里用整数近似到 0.1C 后四舍五入。
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
    const uint8_t table_count = (uint8_t)(sizeof(voltage_soc_ocv_table) /
                                         sizeof(voltage_soc_ocv_table[0]));

    if (cell_mv <= voltage_soc_ocv_table[0].mv)
    {
        return voltage_soc_ocv_table[0].percent;
    }

    for (uint8_t i = 0u; i < (uint8_t)(table_count - 1u); i++)
    {
        const Com_BQ76952_OcvPointTypeDef *left = &voltage_soc_ocv_table[i];
        const Com_BQ76952_OcvPointTypeDef *right = &voltage_soc_ocv_table[i + 1u];

        if (cell_mv <= right->mv)
        {
            uint32_t dv = (uint32_t)right->mv - left->mv;
            uint32_t dp = (uint32_t)right->percent - left->percent;
            uint32_t off = (uint32_t)cell_mv - left->mv;

            if (dv == 0u)
            {
                return left->percent;
            }

            return (uint8_t)(left->percent + ((off * dp) / dv));
        }
    }

    return voltage_soc_ocv_table[table_count - 1u].percent;
}
