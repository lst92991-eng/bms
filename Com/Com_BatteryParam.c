#include "Com_BatteryParam.h"

typedef struct
{
    uint16_t mv;
    uint8_t percent;
} Com_BatteryParam_OcvPointTypeDef;

/*
 * EVE 50E 初始 OCV-SOC 表，参考 25C NMC/NCA 公开曲线细化到 1%。
 * 这张表只作为 SOC 测试起点；最终必须用本电池组静置 OCV 数据重新标定。
 */
static const Com_BatteryParam_OcvPointTypeDef voltage_soc_ocv_table[] =
{
    {3000u, 0u},
    {3150u, 1u},
    {3205u, 2u},
    {3245u, 3u},
    {3275u, 4u},
    {3300u, 5u},
    {3324u, 6u},
    {3348u, 7u},
    {3372u, 8u},
    {3396u, 9u},
    {3420u, 10u},
    {3434u, 11u},
    {3448u, 12u},
    {3462u, 13u},
    {3476u, 14u},
    {3490u, 15u},
    {3502u, 16u},
    {3514u, 17u},
    {3526u, 18u},
    {3538u, 19u},
    {3550u, 20u},
    {3558u, 21u},
    {3566u, 22u},
    {3574u, 23u},
    {3582u, 24u},
    {3590u, 25u},
    {3598u, 26u},
    {3606u, 27u},
    {3614u, 28u},
    {3622u, 29u},
    {3630u, 30u},
    {3636u, 31u},
    {3642u, 32u},
    {3648u, 33u},
    {3654u, 34u},
    {3660u, 35u},
    {3666u, 36u},
    {3672u, 37u},
    {3678u, 38u},
    {3684u, 39u},
    {3690u, 40u},
    {3696u, 41u},
    {3702u, 42u},
    {3708u, 43u},
    {3714u, 44u},
    {3720u, 45u},
    {3726u, 46u},
    {3732u, 47u},
    {3738u, 48u},
    {3744u, 49u},
    {3750u, 50u},
    {3756u, 51u},
    {3762u, 52u},
    {3768u, 53u},
    {3774u, 54u},
    {3780u, 55u},
    {3786u, 56u},
    {3792u, 57u},
    {3798u, 58u},
    {3804u, 59u},
    {3810u, 60u},
    {3816u, 61u},
    {3822u, 62u},
    {3828u, 63u},
    {3834u, 64u},
    {3840u, 65u},
    {3848u, 66u},
    {3856u, 67u},
    {3864u, 68u},
    {3872u, 69u},
    {3880u, 70u},
    {3888u, 71u},
    {3896u, 72u},
    {3904u, 73u},
    {3912u, 74u},
    {3920u, 75u},
    {3926u, 76u},
    {3932u, 77u},
    {3938u, 78u},
    {3944u, 79u},
    {3950u, 80u},
    {3958u, 81u},
    {3966u, 82u},
    {3974u, 83u},
    {3982u, 84u},
    {3990u, 85u},
    {4002u, 86u},
    {4014u, 87u},
    {4026u, 88u},
    {4038u, 89u},
    {4050u, 90u},
    {4064u, 91u},
    {4078u, 92u},
    {4092u, 93u},
    {4106u, 94u},
    {4120u, 95u},
    {4136u, 96u},
    {4152u, 97u},
    {4168u, 98u},
    {4184u, 99u},
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
