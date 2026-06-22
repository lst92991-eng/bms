#include "Int_SC8815.h"

#include "Int_SC8815_BSP.h"
#include "main.h"

#ifndef INT_SC8815_SCL_GPIO_PORT
#define INT_SC8815_SCL_GPIO_PORT GPIOA
#endif

#ifndef INT_SC8815_SCL_PIN
#define INT_SC8815_SCL_PIN GPIO_PIN_7
#endif

#ifndef INT_SC8815_SDA_GPIO_PORT
#define INT_SC8815_SDA_GPIO_PORT GPIOA
#endif

#ifndef INT_SC8815_SDA_PIN
#define INT_SC8815_SDA_PIN GPIO_PIN_6
#endif

#ifndef INT_SC8815_CE_N_GPIO_PORT
#define INT_SC8815_CE_N_GPIO_PORT GPIOB
#endif

#ifndef INT_SC8815_CE_N_PIN
#define INT_SC8815_CE_N_PIN GPIO_PIN_1
#endif

#ifndef INT_SC8815_PSTOP_GPIO_PORT
#define INT_SC8815_PSTOP_GPIO_PORT GPIOB
#endif

#ifndef INT_SC8815_PSTOP_PIN
#define INT_SC8815_PSTOP_PIN GPIO_PIN_0
#endif

#ifndef INT_SC8815_IIC_DELAY_CYCLES
#define INT_SC8815_IIC_DELAY_CYCLES (32u)
#endif

#define INT_SC8815_REG_MAX                       SC8815_REG_RESERVED_1B
#define INT_SC8815_CTRL3_STANDBY_CHANGE_MASK     (SC8815_CTRL3_SET_ILIM_BW_SEL_MASK | \
                                                   SC8815_CTRL3_SET_LOOP_SET_MASK | \
                                                   SC8815_CTRL3_SET_EOC_SET_MASK)

static bool s_sc8815_standby = true;

static void Int_SC8815_IicDelay(void)
{
    for (volatile uint32_t i = 0u; i < INT_SC8815_IIC_DELAY_CYCLES; i++)
    {
    }
}

static void Int_SC8815_IicSclHigh(void)
{
    HAL_GPIO_WritePin(INT_SC8815_SCL_GPIO_PORT, INT_SC8815_SCL_PIN, GPIO_PIN_SET);
    Int_SC8815_IicDelay();
}

static void Int_SC8815_IicSclLow(void)
{
    HAL_GPIO_WritePin(INT_SC8815_SCL_GPIO_PORT, INT_SC8815_SCL_PIN, GPIO_PIN_RESET);
    Int_SC8815_IicDelay();
}

static void Int_SC8815_IicSdaHigh(void)
{
    HAL_GPIO_WritePin(INT_SC8815_SDA_GPIO_PORT, INT_SC8815_SDA_PIN, GPIO_PIN_SET);
    Int_SC8815_IicDelay();
}

static void Int_SC8815_IicSdaLow(void)
{
    HAL_GPIO_WritePin(INT_SC8815_SDA_GPIO_PORT, INT_SC8815_SDA_PIN, GPIO_PIN_RESET);
    Int_SC8815_IicDelay();
}

static bool Int_SC8815_IicSdaRead(void)
{
    return HAL_GPIO_ReadPin(INT_SC8815_SDA_GPIO_PORT, INT_SC8815_SDA_PIN) == GPIO_PIN_SET;
}

static void Int_SC8815_BusStart(void)
{
    Int_SC8815_IicSdaHigh();
    Int_SC8815_IicSclHigh();
    Int_SC8815_IicSdaLow();
    Int_SC8815_IicSclLow();
}

static void Int_SC8815_BusStop(void)
{
    Int_SC8815_IicSdaLow();
    Int_SC8815_IicSclHigh();
    Int_SC8815_IicSdaHigh();
}

static bool Int_SC8815_BusWriteByte(uint8_t data)
{
    for (uint8_t mask = 0x80u; mask != 0u; mask >>= 1u)
    {
        if ((data & mask) != 0u)
        {
            Int_SC8815_IicSdaHigh();
        }
        else
        {
            Int_SC8815_IicSdaLow();
        }

        Int_SC8815_IicSclHigh();
        Int_SC8815_IicSclLow();
    }

    Int_SC8815_IicSdaHigh();
    Int_SC8815_IicSclHigh();
    const bool ack = !Int_SC8815_IicSdaRead();
    Int_SC8815_IicSclLow();

    return ack;
}

static uint8_t Int_SC8815_BusReadByte(bool ack)
{
    uint8_t data = 0u;

    Int_SC8815_IicSdaHigh();

    for (uint8_t bit = 0u; bit < 8u; bit++)
    {
        data <<= 1u;
        Int_SC8815_IicSclHigh();
        if (Int_SC8815_IicSdaRead())
        {
            data |= 0x01u;
        }
        Int_SC8815_IicSclLow();
    }

    if (ack)
    {
        Int_SC8815_IicSdaLow();
    }
    else
    {
        Int_SC8815_IicSdaHigh();
    }

    Int_SC8815_IicSclHigh();
    Int_SC8815_IicSclLow();
    Int_SC8815_IicSdaHigh();

    return data;
}

static bool Int_SC8815_IsRegAddressValid(uint8_t reg)
{
    return reg <= INT_SC8815_REG_MAX;
}

static bool Int_SC8815_IsReadOnlyReg(uint8_t reg)
{
    return (reg >= SC8815_REG_VBUS_FB_VALUE) && (reg <= SC8815_REG_STATUS);
}

static bool Int_SC8815_IsReservedReg(uint8_t reg)
{
    return (reg == SC8815_REG_RESERVED_18) ||
           (reg == SC8815_REG_RESERVED_1A) ||
           (reg == SC8815_REG_RESERVED_1B);
}

static uint32_t Int_SC8815_CurrentLimitCodeToMa(uint8_t code,
                                                uint8_t ratio,
                                                uint16_t rsense_mohm)
{
    const uint32_t numerator = ((uint32_t)code + SC8815_CURRENT_LIMIT_CODE_OFFSET) *
                               (uint32_t)ratio *
                               SC8815_CURRENT_LIMIT_REF_RSENSE_MOHM *
                               1000u;
    const uint32_t denominator = SC8815_CURRENT_LIMIT_CODE_DENOMINATOR *
                                 (uint32_t)rsense_mohm;

    return numerator / denominator;
}

static uint8_t Int_SC8815_CurrentLimitMaToCode(uint16_t current_ma,
                                               uint8_t ratio,
                                               uint16_t rsense_mohm)
{
    const uint32_t numerator = (uint32_t)current_ma *
                               SC8815_CURRENT_LIMIT_CODE_DENOMINATOR *
                               (uint32_t)rsense_mohm;
    const uint32_t denominator = (uint32_t)ratio *
                                 SC8815_CURRENT_LIMIT_REF_RSENSE_MOHM *
                                 1000u;
    uint32_t units = numerator / denominator;

    if (units == 0u)
    {
        units = 1u;
    }

    if (units > (SC8815_CURRENT_LIMIT_CODE_MAX + 1u))
    {
        units = SC8815_CURRENT_LIMIT_CODE_MAX + 1u;
    }

    uint8_t code = (uint8_t)(units - 1u);

    while ((code < SC8815_CURRENT_LIMIT_CODE_MAX) &&
           (Int_SC8815_CurrentLimitCodeToMa(code, ratio, rsense_mohm) <
            SC8815_PROJECT_MIN_LIMIT_CURRENT_MA))
    {
        code++;
    }

    return code;
}

static Int_SC8815_StatusTypeDef Int_SC8815_CheckReservedBits(uint8_t old_value,
                                                             uint8_t new_value,
                                                             uint8_t reserved_mask)
{
    if ((old_value & reserved_mask) != (new_value & reserved_mask))
    {
        return INT_SC8815_ERROR_GUARD;
    }

    return INT_SC8815_OK;
}

static Int_SC8815_StatusTypeDef Int_SC8815_GuardWrite(uint8_t reg,
                                                      uint8_t old_value,
                                                      uint8_t new_value)
{
    if (!Int_SC8815_IsRegAddressValid(reg))
    {
        return INT_SC8815_ERROR_PARAM;
    }

    if (Int_SC8815_IsReadOnlyReg(reg) || Int_SC8815_IsReservedReg(reg))
    {
        return INT_SC8815_ERROR_GUARD;
    }

    if ((reg >= SC8815_REG_VBUSREF_I_SET) && (reg <= SC8815_REG_VBUSREF_E_SET2))
    {
        return INT_SC8815_ERROR_GUARD;
    }

    switch (reg)
    {
    case SC8815_REG_VBAT_SET:
        if (!s_sc8815_standby)
        {
            return INT_SC8815_ERROR_STATE;
        }

        if ((new_value & SC8815_VBAT_SET_VBAT_SEL_MASK) == 0u)
        {
            return INT_SC8815_ERROR_GUARD;
        }
        break;

    case SC8815_REG_IBUS_LIM_SET:
        if (Int_SC8815_CurrentLimitCodeToMa(new_value,
                                            SC8815_PROJECT_IBUS_RATIO_X,
                                            SC8815_PROJECT_RSNS_IBUS_MOHM) <
            SC8815_PROJECT_MIN_LIMIT_CURRENT_MA)
        {
            return INT_SC8815_ERROR_RANGE;
        }
        break;

    case SC8815_REG_IBAT_LIM_SET:
        if (Int_SC8815_CurrentLimitCodeToMa(new_value,
                                            SC8815_PROJECT_IBAT_RATIO_X,
                                            SC8815_PROJECT_RSNS_IBAT_MOHM) <
            SC8815_PROJECT_MIN_LIMIT_CURRENT_MA)
        {
            return INT_SC8815_ERROR_RANGE;
        }
        break;

    case SC8815_REG_RATIO:
        if (!s_sc8815_standby)
        {
            return INT_SC8815_ERROR_STATE;
        }

        if (Int_SC8815_CheckReservedBits(old_value,
                                         new_value,
                                         SC8815_RATIO_RESERVED_MASK) != INT_SC8815_OK)
        {
            return INT_SC8815_ERROR_GUARD;
        }

        if (((new_value & SC8815_RATIO_IBUS_RATIO_MASK) >> SC8815_RATIO_IBUS_RATIO_SHIFT) !=
            SC8815_RATIO_IBUS_RATIO_3X)
        {
            return INT_SC8815_ERROR_GUARD;
        }

        if (((new_value & SC8815_RATIO_IBAT_RATIO_MASK) >> SC8815_RATIO_IBAT_RATIO_SHIFT) !=
            SC8815_RATIO_IBAT_RATIO_6X)
        {
            return INT_SC8815_ERROR_GUARD;
        }

        if (((new_value & SC8815_RATIO_VBAT_MON_RATIO_MASK) >> SC8815_RATIO_VBAT_MON_RATIO_SHIFT) !=
            SC8815_RATIO_VBAT_MON_RATIO_12P5X)
        {
            return INT_SC8815_ERROR_GUARD;
        }

        if (((new_value & SC8815_RATIO_VBUS_RATIO_MASK) >> SC8815_RATIO_VBUS_RATIO_SHIFT) !=
            SC8815_RATIO_VBUS_RATIO_12P5X)
        {
            return INT_SC8815_ERROR_GUARD;
        }
        break;

    case SC8815_REG_CTRL0_SET:
        if ((new_value & SC8815_PROJECT_FORBID_CTRL0_SET_MASK) != 0u)
        {
            return INT_SC8815_ERROR_GUARD;
        }

        if (Int_SC8815_CheckReservedBits(old_value,
                                         new_value,
                                         SC8815_CTRL0_SET_RESERVED_MASK) != INT_SC8815_OK)
        {
            return INT_SC8815_ERROR_GUARD;
        }

        if (!s_sc8815_standby &&
            (((old_value ^ new_value) & (SC8815_CTRL0_SET_FREQ_SET_MASK |
                                         SC8815_CTRL0_SET_DT_SET_MASK)) != 0u))
        {
            return INT_SC8815_ERROR_STATE;
        }
        break;

    case SC8815_REG_CTRL1_SET:
        if ((new_value & SC8815_PROJECT_FORBID_CTRL1_SET_MASK) != 0u)
        {
            return INT_SC8815_ERROR_GUARD;
        }

        if (Int_SC8815_CheckReservedBits(old_value,
                                         new_value,
                                         SC8815_CTRL1_SET_RESERVED_MASK) != INT_SC8815_OK)
        {
            return INT_SC8815_ERROR_GUARD;
        }

        if (!s_sc8815_standby &&
            (((old_value ^ new_value) & (SC8815_CTRL1_SET_ICHAR_SEL_MASK |
                                         SC8815_CTRL1_SET_TRICKLE_SET_MASK)) != 0u))
        {
            return INT_SC8815_ERROR_STATE;
        }
        break;

    case SC8815_REG_CTRL2_SET:
        if (Int_SC8815_CheckReservedBits(old_value,
                                         new_value,
                                         SC8815_CTRL2_SET_RESERVED_MASK) != INT_SC8815_OK)
        {
            return INT_SC8815_ERROR_GUARD;
        }

        if ((new_value & SC8815_CTRL2_SET_FACTORY_MASK) == 0u)
        {
            return INT_SC8815_ERROR_GUARD;
        }

        if (!s_sc8815_standby &&
            (((old_value ^ new_value) & (SC8815_CTRL2_SET_EN_DITHER_MASK |
                                         SC8815_CTRL2_SET_SLEW_SET_MASK)) != 0u))
        {
            return INT_SC8815_ERROR_STATE;
        }
        break;

    case SC8815_REG_CTRL3_SET:
        if ((new_value & SC8815_PROJECT_FORBID_CTRL3_SET_MASK) != 0u)
        {
            return INT_SC8815_ERROR_GUARD;
        }

        if (!s_sc8815_standby &&
            (((old_value ^ new_value) & INT_SC8815_CTRL3_STANDBY_CHANGE_MASK) != 0u))
        {
            return INT_SC8815_ERROR_STATE;
        }
        break;

    case SC8815_REG_MASK:
        if (((old_value ^ new_value) & (SC8815_MASK_RESERVED_MASK &
                                        (uint8_t)~SC8815_MASK_POWER_UP_INTERNAL_SET_MASK)) != 0u)
        {
            return INT_SC8815_ERROR_GUARD;
        }

        if ((new_value & SC8815_MASK_POWER_UP_INTERNAL_SET_MASK) == 0u)
        {
            return INT_SC8815_ERROR_GUARD;
        }
        break;

    default:
        break;
    }

    return INT_SC8815_OK;
}

static Int_SC8815_StatusTypeDef Int_SC8815_ReadRegRaw(uint8_t reg, uint8_t *value)
{
    if ((value == NULL) || !Int_SC8815_IsRegAddressValid(reg))
    {
        return INT_SC8815_ERROR_PARAM;
    }

    Int_SC8815_BusStart();

    if (!Int_SC8815_BusWriteByte(SC8815_I2C_ADDR_WRITE_8BIT) ||
        !Int_SC8815_BusWriteByte(reg))
    {
        Int_SC8815_BusStop();
        return INT_SC8815_ERROR_ACK;
    }

    Int_SC8815_BusStart();

    if (!Int_SC8815_BusWriteByte(SC8815_I2C_ADDR_READ_8BIT))
    {
        Int_SC8815_BusStop();
        return INT_SC8815_ERROR_ACK;
    }

    *value = Int_SC8815_BusReadByte(false);
    Int_SC8815_BusStop();

    return INT_SC8815_OK;
}

static Int_SC8815_StatusTypeDef Int_SC8815_WriteRegRaw(uint8_t reg, uint8_t value)
{
    if (!Int_SC8815_IsRegAddressValid(reg))
    {
        return INT_SC8815_ERROR_PARAM;
    }

    Int_SC8815_BusStart();

    if (!Int_SC8815_BusWriteByte(SC8815_I2C_ADDR_WRITE_8BIT) ||
        !Int_SC8815_BusWriteByte(reg) ||
        !Int_SC8815_BusWriteByte(value))
    {
        Int_SC8815_BusStop();
        return INT_SC8815_ERROR_ACK;
    }

    Int_SC8815_BusStop();

    return INT_SC8815_OK;
}

static uint16_t Int_SC8815_CombineAdcRaw(uint8_t high, uint8_t low)
{
    return (uint16_t)(((uint16_t)high * SC8815_ADC_HIGH_MULTIPLIER) |
                      ((low & SC8815_ADC_LOW2_MASK) >> SC8815_ADC_LOW2_SHIFT));
}

Int_SC8815_StatusTypeDef Int_SC8815_InitSafe(void)
{
    HAL_GPIO_WritePin(INT_SC8815_PSTOP_GPIO_PORT, INT_SC8815_PSTOP_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(INT_SC8815_CE_N_GPIO_PORT, INT_SC8815_CE_N_PIN, GPIO_PIN_SET);

    s_sc8815_standby = true;

    Int_SC8815_IicSclHigh();
    Int_SC8815_IicSdaHigh();

    return INT_SC8815_OK;
}

Int_SC8815_StatusTypeDef Int_SC8815_SetChipEnabled(bool enabled)
{
    HAL_GPIO_WritePin(INT_SC8815_CE_N_GPIO_PORT,
                      INT_SC8815_CE_N_PIN,
                      enabled ? GPIO_PIN_RESET : GPIO_PIN_SET);

    return INT_SC8815_OK;
}

Int_SC8815_StatusTypeDef Int_SC8815_SetStandby(bool standby)
{
    HAL_GPIO_WritePin(INT_SC8815_PSTOP_GPIO_PORT,
                      INT_SC8815_PSTOP_PIN,
                      standby ? GPIO_PIN_SET : GPIO_PIN_RESET);
    s_sc8815_standby = standby;

    return INT_SC8815_OK;
}

Int_SC8815_StatusTypeDef Int_SC8815_ReadReg(uint8_t reg, uint8_t *value)
{
    return Int_SC8815_ReadRegRaw(reg, value);
}

Int_SC8815_StatusTypeDef Int_SC8815_WriteReg(uint8_t reg, uint8_t value)
{
    uint8_t old_value;
    Int_SC8815_StatusTypeDef ret;

    ret = Int_SC8815_ReadRegRaw(reg, &old_value);
    if (ret != INT_SC8815_OK)
    {
        return ret;
    }

    ret = Int_SC8815_GuardWrite(reg, old_value, value);
    if (ret != INT_SC8815_OK)
    {
        return ret;
    }

    return Int_SC8815_WriteRegRaw(reg, value);
}

Int_SC8815_StatusTypeDef Int_SC8815_UpdateReg(uint8_t reg, uint8_t clear_mask, uint8_t set_mask)
{
    uint8_t old_value;
    uint8_t new_value;
    Int_SC8815_StatusTypeDef ret;

    ret = Int_SC8815_ReadRegRaw(reg, &old_value);
    if (ret != INT_SC8815_OK)
    {
        return ret;
    }

    new_value = (uint8_t)((old_value & (uint8_t)~clear_mask) | set_mask);

    ret = Int_SC8815_GuardWrite(reg, old_value, new_value);
    if (ret != INT_SC8815_OK)
    {
        return ret;
    }

    return Int_SC8815_WriteRegRaw(reg, new_value);
}

Int_SC8815_StatusTypeDef Int_SC8815_ReadStatus(Int_SC8815_StatusFlagsTypeDef *status)
{
    uint8_t raw;
    Int_SC8815_StatusTypeDef ret;

    if (status == NULL)
    {
        return INT_SC8815_ERROR_PARAM;
    }

    ret = Int_SC8815_ReadRegRaw(SC8815_REG_STATUS, &raw);
    if (ret != INT_SC8815_OK)
    {
        return ret;
    }

    status->raw = raw;
    status->ac_ok = (raw & SC8815_STATUS_AC_OK_MASK) != 0u;
    status->indet = (raw & SC8815_STATUS_INDET_MASK) != 0u;
    status->vbus_short = (raw & SC8815_STATUS_VBUS_SHORT_MASK) != 0u;
    status->otp = (raw & SC8815_STATUS_OTP_MASK) != 0u;
    status->eoc = (raw & SC8815_STATUS_EOC_MASK) != 0u;

    return INT_SC8815_OK;
}

Int_SC8815_StatusTypeDef Int_SC8815_SetAdcEnabled(bool enabled)
{
    return Int_SC8815_UpdateReg(SC8815_REG_CTRL3_SET,
                                enabled ? 0u : SC8815_CTRL3_SET_AD_START_MASK,
                                enabled ? SC8815_CTRL3_SET_AD_START_MASK : 0u);
}

Int_SC8815_StatusTypeDef Int_SC8815_ReadAdcRaw(Int_SC8815_AdcChannelTypeDef channel, uint16_t *raw)
{
    uint8_t high_reg;
    uint8_t low_reg;
    uint8_t high;
    uint8_t low;
    Int_SC8815_StatusTypeDef ret;

    if (raw == NULL)
    {
        return INT_SC8815_ERROR_PARAM;
    }

    switch (channel)
    {
    case INT_SC8815_ADC_VBUS:
        high_reg = SC8815_REG_VBUS_FB_VALUE;
        low_reg = SC8815_REG_VBUS_FB_VALUE2;
        break;

    case INT_SC8815_ADC_VBAT:
        high_reg = SC8815_REG_VBAT_FB_VALUE;
        low_reg = SC8815_REG_VBAT_FB_VALUE2;
        break;

    case INT_SC8815_ADC_ADIN:
        high_reg = SC8815_REG_ADIN_VALUE;
        low_reg = SC8815_REG_ADIN_VALUE2;
        break;

    default:
        return INT_SC8815_ERROR_PARAM;
    }

    ret = Int_SC8815_ReadRegRaw(high_reg, &high);
    if (ret != INT_SC8815_OK)
    {
        return ret;
    }

    ret = Int_SC8815_ReadRegRaw(low_reg, &low);
    if (ret != INT_SC8815_OK)
    {
        return ret;
    }

    *raw = Int_SC8815_CombineAdcRaw(high, low);
    return INT_SC8815_OK;
}

Int_SC8815_StatusTypeDef Int_SC8815_ReadAdcVoltageMv(Int_SC8815_AdcChannelTypeDef channel, uint32_t *mv)
{
    uint16_t raw;
    uint32_t ratio_x10;
    Int_SC8815_StatusTypeDef ret;

    if (mv == NULL)
    {
        return INT_SC8815_ERROR_PARAM;
    }

    switch (channel)
    {
    case INT_SC8815_ADC_VBUS:
        ratio_x10 = SC8815_ADC_VBUS_RATIO_DEFAULT_X10;
        break;

    case INT_SC8815_ADC_VBAT:
        ratio_x10 = SC8815_ADC_VBAT_RATIO_DEFAULT_X10;
        break;

    case INT_SC8815_ADC_ADIN:
        ratio_x10 = SC8815_ADC_ADIN_RATIO_X10;
        break;

    default:
        return INT_SC8815_ERROR_PARAM;
    }

    ret = Int_SC8815_ReadAdcRaw(channel, &raw);
    if (ret != INT_SC8815_OK)
    {
        return ret;
    }

    *mv = (((uint32_t)raw + SC8815_ADC_VALUE_OFFSET) *
           SC8815_ADC_LSB_MV *
           ratio_x10) /
          10u;

    return INT_SC8815_OK;
}

Int_SC8815_StatusTypeDef Int_SC8815_ReadAdcCurrentMa(Int_SC8815_CurrentChannelTypeDef channel, uint32_t *ma)
{
    uint8_t high_reg;
    uint8_t low_reg;
    uint8_t high;
    uint8_t low;
    uint8_t ratio;
    uint16_t rsense_mohm;
    uint16_t raw;
    Int_SC8815_StatusTypeDef ret;

    if (ma == NULL)
    {
        return INT_SC8815_ERROR_PARAM;
    }

    switch (channel)
    {
    case INT_SC8815_CURRENT_IBUS:
        high_reg = SC8815_REG_IBUS_VALUE;
        low_reg = SC8815_REG_IBUS_VALUE2;
        ratio = SC8815_PROJECT_IBUS_RATIO_X;
        rsense_mohm = SC8815_PROJECT_RSNS_IBUS_MOHM;
        break;

    case INT_SC8815_CURRENT_IBAT:
        high_reg = SC8815_REG_IBAT_VALUE;
        low_reg = SC8815_REG_IBAT_VALUE2;
        ratio = SC8815_PROJECT_IBAT_RATIO_X;
        rsense_mohm = SC8815_PROJECT_RSNS_IBAT_MOHM;
        break;

    default:
        return INT_SC8815_ERROR_PARAM;
    }

    ret = Int_SC8815_ReadRegRaw(high_reg, &high);
    if (ret != INT_SC8815_OK)
    {
        return ret;
    }

    ret = Int_SC8815_ReadRegRaw(low_reg, &low);
    if (ret != INT_SC8815_OK)
    {
        return ret;
    }

    raw = Int_SC8815_CombineAdcRaw(high, low);

    *ma = (((uint32_t)raw + SC8815_ADC_VALUE_OFFSET) *
           SC8815_ADC_CURRENT_NUMERATOR *
           (uint32_t)ratio *
           SC8815_ADC_CURRENT_REF_RSENSE_MOHM *
           1000u) /
          (SC8815_ADC_CURRENT_DENOMINATOR * (uint32_t)rsense_mohm);

    return INT_SC8815_OK;
}

Int_SC8815_StatusTypeDef Int_SC8815_SetCurrentLimitMa(Int_SC8815_CurrentLimitTypeDef type, uint16_t current_ma)
{
    uint8_t reg;
    uint8_t ratio;
    uint16_t rsense_mohm;
    uint16_t max_ma;
    uint8_t code;

    switch (type)
    {
    case INT_SC8815_LIMIT_IBUS:
        reg = SC8815_REG_IBUS_LIM_SET;
        ratio = SC8815_PROJECT_IBUS_RATIO_X;
        rsense_mohm = SC8815_PROJECT_RSNS_IBUS_MOHM;
        max_ma = SC8815_PROJECT_MAX_IBUS_LIMIT_MA;
        break;

    case INT_SC8815_LIMIT_IBAT:
        reg = SC8815_REG_IBAT_LIM_SET;
        ratio = SC8815_PROJECT_IBAT_RATIO_X;
        rsense_mohm = SC8815_PROJECT_RSNS_IBAT_MOHM;
        max_ma = SC8815_PROJECT_MAX_IBAT_LIMIT_MA;
        break;

    default:
        return INT_SC8815_ERROR_PARAM;
    }

    if ((current_ma < SC8815_PROJECT_MIN_LIMIT_CURRENT_MA) || (current_ma > max_ma))
    {
        return INT_SC8815_ERROR_RANGE;
    }

    code = Int_SC8815_CurrentLimitMaToCode(current_ma, ratio, rsense_mohm);

    return Int_SC8815_WriteReg(reg, code);
}
