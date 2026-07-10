#include "Int_SC8815.h"

#include "Int_SC8815_BSP.h"
#include "main.h"

enum
{
    INT_SC8815_REG_MAX = SC8815_REG_RESERVED_1B,
    INT_SC8815_CTRL3_STANDBY_CHANGE_MASK = SC8815_CTRL3_SET_ILIM_BW_SEL_MASK |
                                           SC8815_CTRL3_SET_LOOP_SET_MASK |
                                           SC8815_CTRL3_SET_EOC_SET_MASK
};

static bool s_sc8815_standby = true;
static bool s_sc8815_iic_swapped = (SC8815_PROJECT_IIC_LINE_SWAPPED != 0u);

static uint32_t Int_SC8815_EnterCritical(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    return primask;
}

static void Int_SC8815_ExitCritical(uint32_t primask)
{
    __set_PRIMASK(primask);
}

static void Int_SC8815_IicDelay(void)
{
    for (volatile uint32_t i = 0u; i < SC8815_SW_I2C_DELAY_CYCLES; i++)
    {
    }
}

static void Int_SC8815_IicWriteScl(GPIO_PinState state)
{
    if (s_sc8815_iic_swapped)
    {
        HAL_GPIO_WritePin(SC8815_SW_I2C_SDA_GPIO_Port, SC8815_SW_I2C_SDA_Pin, state);
    }
    else
    {
        HAL_GPIO_WritePin(SC8815_SW_I2C_SCL_GPIO_Port, SC8815_SW_I2C_SCL_Pin, state);
    }
    Int_SC8815_IicDelay();
}

static void Int_SC8815_IicWriteSda(GPIO_PinState state)
{
    if (s_sc8815_iic_swapped)
    {
        HAL_GPIO_WritePin(SC8815_SW_I2C_SCL_GPIO_Port, SC8815_SW_I2C_SCL_Pin, state);
    }
    else
    {
        HAL_GPIO_WritePin(SC8815_SW_I2C_SDA_GPIO_Port, SC8815_SW_I2C_SDA_Pin, state);
    }
    Int_SC8815_IicDelay();
}

static bool Int_SC8815_IicSdaRead(void)
{
    if (s_sc8815_iic_swapped)
    {
        return HAL_GPIO_ReadPin(SC8815_SW_I2C_SCL_GPIO_Port, SC8815_SW_I2C_SCL_Pin) == GPIO_PIN_SET;
    }

    return HAL_GPIO_ReadPin(SC8815_SW_I2C_SDA_GPIO_Port, SC8815_SW_I2C_SDA_Pin) == GPIO_PIN_SET;
}

static void Int_SC8815_BusStart(void)
{
    Int_SC8815_IicWriteSda(GPIO_PIN_SET);
    Int_SC8815_IicWriteScl(GPIO_PIN_SET);
    Int_SC8815_IicWriteSda(GPIO_PIN_RESET);
    Int_SC8815_IicWriteScl(GPIO_PIN_RESET);
}

static void Int_SC8815_BusStop(void)
{
    Int_SC8815_IicWriteSda(GPIO_PIN_RESET);
    Int_SC8815_IicWriteScl(GPIO_PIN_SET);
    Int_SC8815_IicWriteSda(GPIO_PIN_SET);
}

static bool Int_SC8815_BusWriteByte(uint8_t data)
{
    for (uint8_t mask = 0x80u; mask != 0u; mask >>= 1u)
    {
        if ((data & mask) != 0u)
        {
            Int_SC8815_IicWriteSda(GPIO_PIN_SET);
        }
        else
        {
            Int_SC8815_IicWriteSda(GPIO_PIN_RESET);
        }

        Int_SC8815_IicWriteScl(GPIO_PIN_SET);
        Int_SC8815_IicWriteScl(GPIO_PIN_RESET);
    }

    Int_SC8815_IicWriteSda(GPIO_PIN_SET);
    Int_SC8815_IicWriteScl(GPIO_PIN_SET);
    const bool ack = !Int_SC8815_IicSdaRead();
    Int_SC8815_IicWriteScl(GPIO_PIN_RESET);

    return ack;
}

static uint8_t Int_SC8815_BusReadByte(bool ack)
{
    uint8_t data = 0u;

    Int_SC8815_IicWriteSda(GPIO_PIN_SET);

    for (uint8_t bit = 0u; bit < 8u; bit++)
    {
        data <<= 1u;
        Int_SC8815_IicWriteScl(GPIO_PIN_SET);
        if (Int_SC8815_IicSdaRead())
        {
            data |= 0x01u;
        }
        Int_SC8815_IicWriteScl(GPIO_PIN_RESET);
    }

    if (ack)
    {
        Int_SC8815_IicWriteSda(GPIO_PIN_RESET);
    }
    else
    {
        Int_SC8815_IicWriteSda(GPIO_PIN_SET);
    }

    Int_SC8815_IicWriteScl(GPIO_PIN_SET);
    Int_SC8815_IicWriteScl(GPIO_PIN_RESET);
    Int_SC8815_IicWriteSda(GPIO_PIN_SET);

    return data;
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

static Int_SC8815_StatusTypeDef Int_SC8815_GuardWrite(uint8_t reg,
                                                      uint8_t old_value,
                                                      uint8_t new_value)
{
    if (reg > INT_SC8815_REG_MAX)
    {
        return INT_SC8815_ERROR_PARAM;
    }

    if (((reg >= SC8815_REG_VBUS_FB_VALUE) && (reg <= SC8815_REG_STATUS)) ||
        (reg == SC8815_REG_RESERVED_18) ||
        (reg == SC8815_REG_RESERVED_1A) ||
        (reg == SC8815_REG_RESERVED_1B))
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

        if ((old_value & SC8815_RATIO_RESERVED_MASK) !=
            (new_value & SC8815_RATIO_RESERVED_MASK))
        {
            return INT_SC8815_ERROR_GUARD;
        }

        if (((new_value & SC8815_RATIO_IBUS_RATIO_MASK) >> SC8815_RATIO_IBUS_RATIO_SHIFT) !=
            SC8815_RATIO_IBUS_RATIO_6X)
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

        if ((old_value & SC8815_CTRL0_SET_RESERVED_MASK) !=
            (new_value & SC8815_CTRL0_SET_RESERVED_MASK))
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

        if ((old_value & SC8815_CTRL1_SET_RESERVED_MASK) !=
            (new_value & SC8815_CTRL1_SET_RESERVED_MASK))
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
        if ((old_value & SC8815_CTRL2_SET_RESERVED_MASK) !=
            (new_value & SC8815_CTRL2_SET_RESERVED_MASK))
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

static Int_SC8815_StatusTypeDef Int_SC8815_ReadRegRawOnce(uint8_t reg, uint8_t *value)
{
    uint32_t primask;
    Int_SC8815_StatusTypeDef ret = INT_SC8815_OK;

    if ((value == NULL) || (reg > INT_SC8815_REG_MAX))
    {
        return INT_SC8815_ERROR_PARAM;
    }

    primask = Int_SC8815_EnterCritical();
    Int_SC8815_BusStart();

    if (!Int_SC8815_BusWriteByte(SC8815_I2C_ADDR_WRITE_8BIT) ||
        !Int_SC8815_BusWriteByte(reg))
    {
        Int_SC8815_BusStop();
        ret = INT_SC8815_ERROR_ACK;
    }
    else
    {
        Int_SC8815_BusStart();

        if (!Int_SC8815_BusWriteByte(SC8815_I2C_ADDR_READ_8BIT))
        {
            Int_SC8815_BusStop();
            ret = INT_SC8815_ERROR_ACK;
        }
        else
        {
            *value = Int_SC8815_BusReadByte(false);
            Int_SC8815_BusStop();
        }
    }

    Int_SC8815_ExitCritical(primask);
    return ret;
}

static Int_SC8815_StatusTypeDef Int_SC8815_WriteRegRawOnce(uint8_t reg, uint8_t value)
{
    uint32_t primask;
    Int_SC8815_StatusTypeDef ret = INT_SC8815_OK;

    if (reg > INT_SC8815_REG_MAX)
    {
        return INT_SC8815_ERROR_PARAM;
    }

    primask = Int_SC8815_EnterCritical();
    Int_SC8815_BusStart();

    if (!Int_SC8815_BusWriteByte(SC8815_I2C_ADDR_WRITE_8BIT) ||
        !Int_SC8815_BusWriteByte(reg) ||
        !Int_SC8815_BusWriteByte(value))
    {
        ret = INT_SC8815_ERROR_ACK;
    }

    Int_SC8815_BusStop();
    Int_SC8815_ExitCritical(primask);
    return ret;
}

static Int_SC8815_StatusTypeDef Int_SC8815_ReadRegRaw(uint8_t reg, uint8_t *value)
{
    Int_SC8815_StatusTypeDef ret;

    ret = Int_SC8815_ReadRegRawOnce(reg, value);
    if (ret == INT_SC8815_ERROR_ACK)
    {
        s_sc8815_iic_swapped = !s_sc8815_iic_swapped;
        ret = Int_SC8815_ReadRegRawOnce(reg, value);
        if (ret != INT_SC8815_OK)
        {
            s_sc8815_iic_swapped = !s_sc8815_iic_swapped;
        }
    }

    return ret;
}

static Int_SC8815_StatusTypeDef Int_SC8815_WriteRegRaw(uint8_t reg, uint8_t value)
{
    Int_SC8815_StatusTypeDef ret;

    ret = Int_SC8815_WriteRegRawOnce(reg, value);
    if (ret == INT_SC8815_ERROR_ACK)
    {
        s_sc8815_iic_swapped = !s_sc8815_iic_swapped;
        ret = Int_SC8815_WriteRegRawOnce(reg, value);
        if (ret != INT_SC8815_OK)
        {
            s_sc8815_iic_swapped = !s_sc8815_iic_swapped;
        }
    }

    return ret;
}

static Int_SC8815_StatusTypeDef Int_SC8815_ReadAdcPair(uint8_t high_reg, uint16_t *raw)
{
    uint8_t high;
    uint8_t low;
    Int_SC8815_StatusTypeDef ret;

    ret = Int_SC8815_ReadRegRaw(high_reg, &high);
    if (ret != INT_SC8815_OK)
    {
        return ret;
    }

    /* 五组 ADC 的低 2 bit 寄存器都紧跟在高 8 bit 寄存器之后。 */
    ret = Int_SC8815_ReadRegRaw((uint8_t)(high_reg + 1u), &low);
    if (ret != INT_SC8815_OK)
    {
        return ret;
    }

    *raw = (uint16_t)(((uint16_t)high * SC8815_ADC_HIGH_MULTIPLIER) |
                      ((low & SC8815_ADC_LOW2_MASK) >> SC8815_ADC_LOW2_SHIFT));
    return INT_SC8815_OK;
}

void Int_SC8815_InitSafe(void)
{
    GPIO_InitTypeDef gpio = {0};
    uint8_t i;
    bool default_swapped;

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* 先写安全电平再切换输出模式，避免 GPIO 接管瞬间误启动功率级。 */
    HAL_GPIO_WritePin(SC8815_SW_I2C_SDA_GPIO_Port,
                      SC8815_SW_I2C_SDA_Pin,
                      GPIO_PIN_SET);
    HAL_GPIO_WritePin(SC8815_SW_I2C_SCL_GPIO_Port,
                      SC8815_SW_I2C_SCL_Pin,
                      GPIO_PIN_SET);

    gpio.Pin = SC8815_SW_I2C_SDA_Pin | SC8815_SW_I2C_SCL_Pin;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = SC8815_PSTOP_Pin | SC8815_CE_N_Pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);

    HAL_GPIO_WritePin(SC8815_PSTOP_GPIO_Port, SC8815_PSTOP_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SC8815_CE_N_GPIO_Port, SC8815_CE_N_Pin, GPIO_PIN_SET);

    s_sc8815_standby = true;
    default_swapped = (SC8815_PROJECT_IIC_LINE_SWAPPED != 0u);
    s_sc8815_iic_swapped = false;

    /*
     * PA6/PA7 曾出现线序争议，bring-up 时两种线序都释放一次总线。
     * 只打 SCL 恢复脉冲和 STOP，不访问寄存器，也不启动功率级。
     */
    for (i = 0u; i < 2u; i++)
    {
        s_sc8815_iic_swapped = (i != 0u);
        Int_SC8815_IicWriteScl(GPIO_PIN_SET);
        Int_SC8815_IicWriteSda(GPIO_PIN_SET);
        for (uint8_t pulse = 0u; pulse < 9u; pulse++)
        {
            Int_SC8815_IicWriteScl(GPIO_PIN_RESET);
            Int_SC8815_IicWriteScl(GPIO_PIN_SET);
        }
        Int_SC8815_BusStop();
    }

    s_sc8815_iic_swapped = default_swapped;
}

void Int_SC8815_SetChipEnabled(bool enabled)
{
    HAL_GPIO_WritePin(SC8815_CE_N_GPIO_Port,
                      SC8815_CE_N_Pin,
                      enabled ? GPIO_PIN_RESET : GPIO_PIN_SET);

}

void Int_SC8815_SetStandby(bool standby)
{
    HAL_GPIO_WritePin(SC8815_PSTOP_GPIO_Port,
                      SC8815_PSTOP_Pin,
                      standby ? GPIO_PIN_SET : GPIO_PIN_RESET);
    s_sc8815_standby = standby;
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

    if (raw == NULL)
    {
        return INT_SC8815_ERROR_PARAM;
    }

    switch (channel)
    {
    case INT_SC8815_ADC_VBUS:
        high_reg = SC8815_REG_VBUS_FB_VALUE;
        break;

    case INT_SC8815_ADC_VBAT:
        high_reg = SC8815_REG_VBAT_FB_VALUE;
        break;

    case INT_SC8815_ADC_ADIN:
        high_reg = SC8815_REG_ADIN_VALUE;
        break;

    default:
        return INT_SC8815_ERROR_PARAM;
    }

    return Int_SC8815_ReadAdcPair(high_reg, raw);
}

Int_SC8815_StatusTypeDef Int_SC8815_ReadAdcCurrentRaw(Int_SC8815_CurrentChannelTypeDef channel, uint16_t *raw)
{
    uint8_t high_reg;

    if (raw == NULL)
    {
        return INT_SC8815_ERROR_PARAM;
    }

    switch (channel)
    {
    case INT_SC8815_CURRENT_IBUS:
        high_reg = SC8815_REG_IBUS_VALUE;
        break;

    case INT_SC8815_CURRENT_IBAT:
        high_reg = SC8815_REG_IBAT_VALUE;
        break;

    default:
        return INT_SC8815_ERROR_PARAM;
    }

    return Int_SC8815_ReadAdcPair(high_reg, raw);
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
        ratio = SC8815_PROJECT_IBUS_RATIO_X;
        rsense_mohm = SC8815_PROJECT_RSNS_IBUS_MOHM;
        break;

    case INT_SC8815_CURRENT_IBAT:
        ratio = SC8815_PROJECT_IBAT_RATIO_X;
        rsense_mohm = SC8815_PROJECT_RSNS_IBAT_MOHM;
        break;

    default:
        return INT_SC8815_ERROR_PARAM;
    }

    ret = Int_SC8815_ReadAdcCurrentRaw(channel, &raw);
    if (ret != INT_SC8815_OK)
    {
        return ret;
    }

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

Int_SC8815_StatusTypeDef Int_SC8815_ProbeAddress(uint8_t addr_7bit, bool swapped)
{
    bool old_swapped = s_sc8815_iic_swapped;
    uint32_t primask;
    Int_SC8815_StatusTypeDef ret = INT_SC8815_OK;

    if (addr_7bit > 0x7Fu)
    {
        return INT_SC8815_ERROR_PARAM;
    }

    s_sc8815_iic_swapped = swapped;
    primask = Int_SC8815_EnterCritical();
    Int_SC8815_BusStart();
    if (!Int_SC8815_BusWriteByte((uint8_t)(addr_7bit << 1u)))
    {
        ret = INT_SC8815_ERROR_ACK;
    }
    Int_SC8815_BusStop();
    Int_SC8815_ExitCritical(primask);
    s_sc8815_iic_swapped = old_swapped;

    return ret;
}

Int_SC8815_StatusTypeDef Int_SC8815_ReadRegWithLineOrder(uint8_t reg, bool swapped, uint8_t *value)
{
    bool old_swapped = s_sc8815_iic_swapped;
    Int_SC8815_StatusTypeDef ret;

    s_sc8815_iic_swapped = swapped;
    ret = Int_SC8815_ReadRegRawOnce(reg, value);
    s_sc8815_iic_swapped = old_swapped;

    return ret;
}

bool Int_SC8815_IsIicLineSwapped(void)
{
    return s_sc8815_iic_swapped;
}

uint8_t Int_SC8815_GetBusLevels(void)
{
    uint8_t levels = 0u;

    if (HAL_GPIO_ReadPin(SC8815_SW_I2C_SCL_GPIO_Port, SC8815_SW_I2C_SCL_Pin) == GPIO_PIN_SET)
    {
        levels |= 0x01u;
    }
    if (HAL_GPIO_ReadPin(SC8815_SW_I2C_SDA_GPIO_Port, SC8815_SW_I2C_SDA_Pin) == GPIO_PIN_SET)
    {
        levels |= 0x02u;
    }

    return levels;
}
