#include "Int_SC8815.h"

#include "Int_SC8815_BSP.h"
#include "main.h"

enum
{
    INT_SC8815_REG_MAX = SC8815_REG_RESERVED_1B,
    INT_SC8815_ADC_COHERENCY_RETRY_COUNT = 3u,
    INT_SC8815_CTRL3_STANDBY_CHANGE_MASK = SC8815_CTRL3_SET_ILIM_BW_SEL_MASK |
                                           SC8815_CTRL3_SET_LOOP_SET_MASK |
                                           SC8815_CTRL3_SET_EOC_SET_MASK
};

static volatile bool s_sc8815_standby = true;
static volatile bool s_sc8815_interrupt_pending = false;
static volatile uint32_t s_sc8815_interrupt_sequence = 0u;
static volatile bool s_sc8815_bus_busy = false;
static Int_SC8815_BusStatsTypeDef s_sc8815_bus_stats;

void Int_SC8815_AssertBootSafeGpio(void)
{
    enum
    {
        SC8815_BOOT_PIN_MODE_MASK = (0x3u << (0u * 2u)) | (0x3u << (1u * 2u)),
        SC8815_BOOT_PIN_OUTPUT_MODE = (0x1u << (0u * 2u)) | (0x1u << (1u * 2u)),
        SC8815_BOOT_PIN_PULLUP = (0x1u << (0u * 2u)) | (0x1u << (1u * 2u))
    };
    const uint32_t safe_pins = (uint32_t)SC8815_PSTOP_Pin | (uint32_t)SC8815_CE_N_Pin;

    /*
     * 本板固定映射 PB0=PSTOP、PB1=#CE。先开启 GPIOB，再写 BSRR，最后切换输出模式，
     * 避免 watchdog reset 后 GPIO 接管瞬间把任一低电平送到功率级。
     */
    RCC->IOPENR |= RCC_IOPENR_GPIOBEN;
    (void)RCC->IOPENR;
    GPIOB->BSRR = safe_pins;
    GPIOB->OTYPER &= ~safe_pins;
    GPIOB->OSPEEDR &= ~((uint32_t)SC8815_BOOT_PIN_MODE_MASK);
    GPIOB->PUPDR =
        (GPIOB->PUPDR & ~((uint32_t)SC8815_BOOT_PIN_MODE_MASK)) | (uint32_t)SC8815_BOOT_PIN_PULLUP;
    GPIOB->MODER = (GPIOB->MODER & ~((uint32_t)SC8815_BOOT_PIN_MODE_MASK)) |
                   (uint32_t)SC8815_BOOT_PIN_OUTPUT_MODE;
    __DSB();
    s_sc8815_standby = true;
}

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

static GPIO_TypeDef *Int_SC8815_IicSclPort(void)
{
    return (SC8815_PROJECT_IIC_LINE_SWAPPED != 0u) ? SC8815_SW_I2C_SDA_GPIO_Port
                                                   : SC8815_SW_I2C_SCL_GPIO_Port;
}

static uint16_t Int_SC8815_IicSclPin(void)
{
    return (SC8815_PROJECT_IIC_LINE_SWAPPED != 0u) ? SC8815_SW_I2C_SDA_Pin : SC8815_SW_I2C_SCL_Pin;
}

static GPIO_TypeDef *Int_SC8815_IicSdaPort(void)
{
    return (SC8815_PROJECT_IIC_LINE_SWAPPED != 0u) ? SC8815_SW_I2C_SCL_GPIO_Port
                                                   : SC8815_SW_I2C_SDA_GPIO_Port;
}

static uint16_t Int_SC8815_IicSdaPin(void)
{
    return (SC8815_PROJECT_IIC_LINE_SWAPPED != 0u) ? SC8815_SW_I2C_SCL_Pin : SC8815_SW_I2C_SDA_Pin;
}

static void Int_SC8815_IicWriteSclLow(void)
{
    HAL_GPIO_WritePin(Int_SC8815_IicSclPort(), Int_SC8815_IicSclPin(), GPIO_PIN_RESET);
    Int_SC8815_IicDelay();
}

static void Int_SC8815_IicWriteSda(GPIO_PinState state)
{
    HAL_GPIO_WritePin(Int_SC8815_IicSdaPort(), Int_SC8815_IicSdaPin(), state);
    Int_SC8815_IicDelay();
}

static bool Int_SC8815_IicSdaRead(void)
{
    return HAL_GPIO_ReadPin(Int_SC8815_IicSdaPort(), Int_SC8815_IicSdaPin()) == GPIO_PIN_SET;
}

static Int_SC8815_StatusTypeDef Int_SC8815_IicReleaseScl(void)
{
    HAL_GPIO_WritePin(Int_SC8815_IicSclPort(), Int_SC8815_IicSclPin(), GPIO_PIN_SET);
    for (uint32_t count = 0u; count < SC8815_SW_I2C_STRETCH_TIMEOUT_LOOPS; count++)
    {
        if (HAL_GPIO_ReadPin(Int_SC8815_IicSclPort(), Int_SC8815_IicSclPin()) == GPIO_PIN_SET)
        {
            Int_SC8815_IicDelay();
            return INT_SC8815_OK;
        }
    }

    s_sc8815_bus_stats.timeout_count++;
    return INT_SC8815_ERROR_TIMEOUT;
}

static bool Int_SC8815_BusAcquire(void)
{
    uint32_t primask = Int_SC8815_EnterCritical();
    bool acquired = !s_sc8815_bus_busy;

    if (acquired)
    {
        s_sc8815_bus_busy = true;
    }
    Int_SC8815_ExitCritical(primask);

    if (!acquired)
    {
        s_sc8815_bus_stats.busy_count++;
    }
    return acquired;
}

static void Int_SC8815_BusRelease(void)
{
    uint32_t primask = Int_SC8815_EnterCritical();
    s_sc8815_bus_busy = false;
    Int_SC8815_ExitCritical(primask);
}

static Int_SC8815_StatusTypeDef Int_SC8815_BusRecover(void)
{
    Int_SC8815_StatusTypeDef ret;

    Int_SC8815_IicWriteSda(GPIO_PIN_SET);
    ret = Int_SC8815_IicReleaseScl();
    if (ret != INT_SC8815_OK)
    {
        return ret;
    }
    if (Int_SC8815_IicSdaRead())
    {
        return INT_SC8815_OK;
    }

    s_sc8815_bus_stats.recovery_count++;
    for (uint8_t pulse = 0u; pulse < SC8815_SW_I2C_RECOVERY_PULSES; pulse++)
    {
        Int_SC8815_IicWriteSclLow();
        ret = Int_SC8815_IicReleaseScl();
        if (ret != INT_SC8815_OK)
        {
            return ret;
        }
        if (Int_SC8815_IicSdaRead())
        {
            break;
        }
    }

    Int_SC8815_IicWriteSda(GPIO_PIN_RESET);
    ret = Int_SC8815_IicReleaseScl();
    Int_SC8815_IicWriteSda(GPIO_PIN_SET);
    if (ret != INT_SC8815_OK)
    {
        return ret;
    }
    return Int_SC8815_IicSdaRead() ? INT_SC8815_OK : INT_SC8815_ERROR_STATE;
}

static Int_SC8815_StatusTypeDef Int_SC8815_BusStart(void)
{
    Int_SC8815_StatusTypeDef ret = Int_SC8815_BusRecover();

    if (ret != INT_SC8815_OK)
    {
        return ret;
    }
    Int_SC8815_IicWriteSda(GPIO_PIN_SET);
    ret = Int_SC8815_IicReleaseScl();
    if (ret != INT_SC8815_OK)
    {
        return ret;
    }
    Int_SC8815_IicWriteSda(GPIO_PIN_RESET);
    Int_SC8815_IicWriteSclLow();
    return INT_SC8815_OK;
}

static Int_SC8815_StatusTypeDef Int_SC8815_BusRepeatedStart(void)
{
    Int_SC8815_StatusTypeDef ret;

    Int_SC8815_IicWriteSda(GPIO_PIN_SET);
    ret = Int_SC8815_IicReleaseScl();
    if (ret != INT_SC8815_OK)
    {
        return ret;
    }
    Int_SC8815_IicWriteSda(GPIO_PIN_RESET);
    Int_SC8815_IicWriteSclLow();
    return INT_SC8815_OK;
}

static void Int_SC8815_BusStop(void)
{
    Int_SC8815_IicWriteSda(GPIO_PIN_RESET);
    if (Int_SC8815_IicReleaseScl() == INT_SC8815_OK)
    {
        Int_SC8815_IicWriteSda(GPIO_PIN_SET);
    }
}

static Int_SC8815_StatusTypeDef Int_SC8815_BusWriteByte(uint8_t data)
{
    Int_SC8815_StatusTypeDef ret;

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

        ret = Int_SC8815_IicReleaseScl();
        if (ret != INT_SC8815_OK)
        {
            return ret;
        }
        Int_SC8815_IicWriteSclLow();
    }

    Int_SC8815_IicWriteSda(GPIO_PIN_SET);
    ret = Int_SC8815_IicReleaseScl();
    if (ret != INT_SC8815_OK)
    {
        return ret;
    }
    const bool ack = !Int_SC8815_IicSdaRead();
    Int_SC8815_IicWriteSclLow();

    if (!ack)
    {
        s_sc8815_bus_stats.nack_count++;
        return INT_SC8815_ERROR_ACK;
    }
    return INT_SC8815_OK;
}

static Int_SC8815_StatusTypeDef Int_SC8815_BusReadByte(bool ack, uint8_t *value)
{
    uint8_t data = 0u;
    Int_SC8815_StatusTypeDef ret;

    if (value == NULL)
    {
        return INT_SC8815_ERROR_PARAM;
    }

    Int_SC8815_IicWriteSda(GPIO_PIN_SET);

    for (uint8_t bit = 0u; bit < 8u; bit++)
    {
        data <<= 1u;
        ret = Int_SC8815_IicReleaseScl();
        if (ret != INT_SC8815_OK)
        {
            return ret;
        }
        if (Int_SC8815_IicSdaRead())
        {
            data |= 0x01u;
        }
        Int_SC8815_IicWriteSclLow();
    }

    if (ack)
    {
        Int_SC8815_IicWriteSda(GPIO_PIN_RESET);
    }
    else
    {
        Int_SC8815_IicWriteSda(GPIO_PIN_SET);
    }

    ret = Int_SC8815_IicReleaseScl();
    if (ret != INT_SC8815_OK)
    {
        return ret;
    }
    Int_SC8815_IicWriteSclLow();
    Int_SC8815_IicWriteSda(GPIO_PIN_SET);

    *value = data;
    return INT_SC8815_OK;
}

static uint32_t Int_SC8815_CurrentLimitCodeToMa(uint8_t code, uint8_t ratio, uint16_t rsense_mohm)
{
    const uint32_t numerator = ((uint32_t)code + SC8815_CURRENT_LIMIT_CODE_OFFSET) *
                               (uint32_t)ratio * SC8815_CURRENT_LIMIT_REF_RSENSE_MOHM * 1000u;
    const uint32_t denominator = SC8815_CURRENT_LIMIT_CODE_DENOMINATOR * (uint32_t)rsense_mohm;

    return numerator / denominator;
}

static uint8_t
Int_SC8815_CurrentLimitMaToCode(uint16_t current_ma, uint8_t ratio, uint16_t rsense_mohm)
{
    const uint32_t numerator =
        (uint32_t)current_ma * SC8815_CURRENT_LIMIT_CODE_DENOMINATOR * (uint32_t)rsense_mohm;
    const uint32_t denominator = (uint32_t)ratio * SC8815_CURRENT_LIMIT_REF_RSENSE_MOHM * 1000u;
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

static Int_SC8815_StatusTypeDef
Int_SC8815_GuardWrite(uint8_t reg, uint8_t old_value, uint8_t new_value)
{
    if (reg > INT_SC8815_REG_MAX)
    {
        return INT_SC8815_ERROR_PARAM;
    }

    if (((reg >= SC8815_REG_VBUS_FB_VALUE) && (reg <= SC8815_REG_STATUS)) ||
        (reg == SC8815_REG_RESERVED_18) || (reg == SC8815_REG_RESERVED_1A) ||
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
        {
            const uint32_t current_ma = Int_SC8815_CurrentLimitCodeToMa(
                new_value, SC8815_PROJECT_IBUS_RATIO_X, SC8815_PROJECT_RSNS_IBUS_MOHM);

            if (!s_sc8815_standby)
            {
                return INT_SC8815_ERROR_STATE;
            }
            if ((current_ma < SC8815_PROJECT_MIN_LIMIT_CURRENT_MA) ||
                (current_ma > SC8815_PROJECT_MAX_IBUS_LIMIT_MA))
            {
                return INT_SC8815_ERROR_RANGE;
            }
            break;
        }

        case SC8815_REG_IBAT_LIM_SET:
        {
            const uint32_t current_ma = Int_SC8815_CurrentLimitCodeToMa(
                new_value, SC8815_PROJECT_IBAT_RATIO_X, SC8815_PROJECT_RSNS_IBAT_MOHM);

            if (!s_sc8815_standby)
            {
                return INT_SC8815_ERROR_STATE;
            }
            if ((current_ma < SC8815_PROJECT_MIN_LIMIT_CURRENT_MA) ||
                (current_ma > SC8815_PROJECT_MAX_IBAT_LIMIT_MA))
            {
                return INT_SC8815_ERROR_RANGE;
            }
            break;
        }

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
                SC8815_RATIO_IBUS_RATIO_3X)
            {
                return INT_SC8815_ERROR_GUARD;
            }

            if (((new_value & SC8815_RATIO_IBAT_RATIO_MASK) >> SC8815_RATIO_IBAT_RATIO_SHIFT) !=
                SC8815_RATIO_IBAT_RATIO_6X)
            {
                return INT_SC8815_ERROR_GUARD;
            }

            if (((new_value & SC8815_RATIO_VBAT_MON_RATIO_MASK) >>
                 SC8815_RATIO_VBAT_MON_RATIO_SHIFT) != SC8815_RATIO_VBAT_MON_RATIO_12P5X)
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
                (((old_value ^ new_value) &
                  (SC8815_CTRL0_SET_FREQ_SET_MASK | SC8815_CTRL0_SET_DT_SET_MASK)) != 0u))
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
                (((old_value ^ new_value) &
                  (SC8815_CTRL1_SET_ICHAR_SEL_MASK | SC8815_CTRL1_SET_TRICKLE_SET_MASK)) != 0u))
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
                (((old_value ^ new_value) &
                  (SC8815_CTRL2_SET_EN_DITHER_MASK | SC8815_CTRL2_SET_SLEW_SET_MASK)) != 0u))
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
            if (((old_value ^ new_value) &
                 (SC8815_MASK_RESERVED_MASK & (uint8_t)~SC8815_MASK_POWER_UP_INTERNAL_SET_MASK)) !=
                0u)
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
    Int_SC8815_StatusTypeDef ret;

    if ((value == NULL) || (reg > INT_SC8815_REG_MAX))
    {
        return INT_SC8815_ERROR_PARAM;
    }

    if (!Int_SC8815_BusAcquire())
    {
        return INT_SC8815_ERROR_STATE;
    }
    s_sc8815_bus_stats.transaction_count++;

    ret = Int_SC8815_BusStart();
    if (ret == INT_SC8815_OK)
    {
        ret = Int_SC8815_BusWriteByte(SC8815_I2C_ADDR_WRITE_8BIT);
    }
    if (ret == INT_SC8815_OK)
    {
        ret = Int_SC8815_BusWriteByte(reg);
    }
    if (ret == INT_SC8815_OK)
    {
        ret = Int_SC8815_BusRepeatedStart();
    }
    if (ret == INT_SC8815_OK)
    {
        ret = Int_SC8815_BusWriteByte(SC8815_I2C_ADDR_READ_8BIT);
    }
    if (ret == INT_SC8815_OK)
    {
        ret = Int_SC8815_BusReadByte(false, value);
    }

    Int_SC8815_BusStop();
    Int_SC8815_BusRelease();
    return ret;
}

static Int_SC8815_StatusTypeDef Int_SC8815_WriteRegRawOnce(uint8_t reg, uint8_t value)
{
    Int_SC8815_StatusTypeDef ret;

    if (reg > INT_SC8815_REG_MAX)
    {
        return INT_SC8815_ERROR_PARAM;
    }

    if (!Int_SC8815_BusAcquire())
    {
        return INT_SC8815_ERROR_STATE;
    }
    s_sc8815_bus_stats.transaction_count++;

    ret = Int_SC8815_BusStart();
    if (ret == INT_SC8815_OK)
    {
        ret = Int_SC8815_BusWriteByte(SC8815_I2C_ADDR_WRITE_8BIT);
    }
    if (ret == INT_SC8815_OK)
    {
        ret = Int_SC8815_BusWriteByte(reg);
    }
    if (ret == INT_SC8815_OK)
    {
        ret = Int_SC8815_BusWriteByte(value);
    }
    Int_SC8815_BusStop();
    Int_SC8815_BusRelease();
    return ret;
}

static Int_SC8815_StatusTypeDef Int_SC8815_ReadRegRaw(uint8_t reg, uint8_t *value)
{
    return Int_SC8815_ReadRegRawOnce(reg, value);
}

static Int_SC8815_StatusTypeDef Int_SC8815_WriteRegRaw(uint8_t reg, uint8_t value)
{
    return Int_SC8815_WriteRegRawOnce(reg, value);
}

static Int_SC8815_StatusTypeDef Int_SC8815_ReadAdcPair(uint8_t high_reg, uint16_t *raw)
{
    uint8_t high_before;
    uint8_t high_after;
    uint8_t low;
    Int_SC8815_StatusTypeDef ret;

    for (uint8_t retry = 0u; retry < INT_SC8815_ADC_COHERENCY_RETRY_COUNT; retry++)
    {
        ret = Int_SC8815_ReadRegRaw(high_reg, &high_before);
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
        ret = Int_SC8815_ReadRegRaw(high_reg, &high_after);
        if (ret != INT_SC8815_OK)
        {
            return ret;
        }
        if (high_before == high_after)
        {
            *raw = (uint16_t)(((uint16_t)high_after * SC8815_ADC_HIGH_MULTIPLIER) |
                              ((low & SC8815_ADC_LOW2_MASK) >> SC8815_ADC_LOW2_SHIFT));
            return INT_SC8815_OK;
        }
    }
    return INT_SC8815_ERROR_STATE;
}

void Int_SC8815_InitSafe(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    Int_SC8815_AssertBootSafeGpio();

    /* 先写安全电平再切换输出模式，避免 GPIO 接管瞬间误启动功率级。 */
    HAL_GPIO_WritePin(SC8815_SW_I2C_SDA_GPIO_Port, SC8815_SW_I2C_SDA_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SC8815_SW_I2C_SCL_GPIO_Port, SC8815_SW_I2C_SCL_Pin, GPIO_PIN_SET);

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

    Int_SC8815_ForceStandby();
    HAL_GPIO_WritePin(SC8815_CE_N_GPIO_Port, SC8815_CE_N_Pin, GPIO_PIN_SET);

    s_sc8815_bus_busy = false;
    Int_SC8815_ResetBusStats();
    if (Int_SC8815_BusAcquire())
    {
        (void)Int_SC8815_BusRecover();
        Int_SC8815_BusRelease();
    }
}

void Int_SC8815_SetChipEnabled(bool enabled)
{
    HAL_GPIO_WritePin(
        SC8815_CE_N_GPIO_Port, SC8815_CE_N_Pin, enabled ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void Int_SC8815_SetStandby(bool standby)
{
    if (standby)
    {
        Int_SC8815_ForceStandby();
        return;
    }

    HAL_GPIO_WritePin(SC8815_PSTOP_GPIO_Port, SC8815_PSTOP_Pin, GPIO_PIN_RESET);
    s_sc8815_standby = false;
}

void Int_SC8815_ForceStandby(void)
{
    /* BSRR 单次写入，无 HAL/I2C/锁依赖，允许保护 ISR 直接调用。 */
    SC8815_PSTOP_GPIO_Port->BSRR = (uint32_t)SC8815_PSTOP_Pin;
    s_sc8815_standby = true;
}

bool Int_SC8815_IsStandbyAsserted(void)
{
    return HAL_GPIO_ReadPin(SC8815_PSTOP_GPIO_Port, SC8815_PSTOP_Pin) == GPIO_PIN_SET;
}

void Int_SC8815_NotifyInterruptFromISR(void)
{
    s_sc8815_interrupt_sequence++;
    s_sc8815_interrupt_pending = true;
}

bool Int_SC8815_TakeInterruptPending(void)
{
    bool pending;
    uint32_t primask = Int_SC8815_EnterCritical();

    pending = s_sc8815_interrupt_pending;
    s_sc8815_interrupt_pending = false;
    Int_SC8815_ExitCritical(primask);

    return pending;
}

uint32_t Int_SC8815_GetInterruptSequence(void)
{
    uint32_t sequence;
    uint32_t primask = Int_SC8815_EnterCritical();

    sequence = s_sc8815_interrupt_sequence;
    Int_SC8815_ExitCritical(primask);
    return sequence;
}

Int_SC8815_StatusTypeDef Int_SC8815_ReadReg(uint8_t reg, uint8_t *value)
{
    return Int_SC8815_ReadRegRaw(reg, value);
}

Int_SC8815_StatusTypeDef Int_SC8815_WriteReg(uint8_t reg, uint8_t value)
{
    uint8_t old_value;
    uint8_t verify_value;
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

    ret = Int_SC8815_WriteRegRaw(reg, value);
    if (ret != INT_SC8815_OK)
    {
        return ret;
    }
    ret = Int_SC8815_ReadRegRaw(reg, &verify_value);
    if (ret != INT_SC8815_OK)
    {
        return ret;
    }
    return (verify_value == value) ? INT_SC8815_OK : INT_SC8815_ERROR_STATE;
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

    ret = Int_SC8815_WriteRegRaw(reg, new_value);
    if (ret != INT_SC8815_OK)
    {
        return ret;
    }
    ret = Int_SC8815_ReadRegRaw(reg, &old_value);
    if (ret != INT_SC8815_OK)
    {
        return ret;
    }
    return (old_value == new_value) ? INT_SC8815_OK : INT_SC8815_ERROR_STATE;
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

Int_SC8815_StatusTypeDef Int_SC8815_ReadAdcCurrentRaw(Int_SC8815_CurrentChannelTypeDef channel,
                                                      uint16_t *raw)
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

Int_SC8815_StatusTypeDef Int_SC8815_ReadAdcVoltageMv(Int_SC8815_AdcChannelTypeDef channel,
                                                     uint32_t *mv)
{
    uint16_t raw;
    Int_SC8815_StatusTypeDef ret;

    if (mv == NULL)
    {
        return INT_SC8815_ERROR_PARAM;
    }

    ret = Int_SC8815_ReadAdcRaw(channel, &raw);
    if (ret != INT_SC8815_OK)
    {
        return ret;
    }

    return Int_SC8815_AdcVoltageRawToMv(channel, raw, mv);
}

Int_SC8815_StatusTypeDef Int_SC8815_ReadAdcCurrentMa(Int_SC8815_CurrentChannelTypeDef channel,
                                                     uint32_t *ma)
{
    uint16_t raw;
    Int_SC8815_StatusTypeDef ret;

    if (ma == NULL)
    {
        return INT_SC8815_ERROR_PARAM;
    }

    ret = Int_SC8815_ReadAdcCurrentRaw(channel, &raw);
    if (ret != INT_SC8815_OK)
    {
        return ret;
    }

    return Int_SC8815_AdcCurrentRawToMa(channel, raw, ma);
}

Int_SC8815_StatusTypeDef
Int_SC8815_AdcVoltageRawToMv(Int_SC8815_AdcChannelTypeDef channel, uint16_t raw, uint32_t *mv)
{
    uint32_t ratio_x10;

    if ((mv == NULL) || (raw > 1023u))
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

    *mv = (((uint32_t)raw + SC8815_ADC_VALUE_OFFSET) * SC8815_ADC_LSB_MV * ratio_x10) / 10u;
    return INT_SC8815_OK;
}

Int_SC8815_StatusTypeDef
Int_SC8815_AdcCurrentRawToMa(Int_SC8815_CurrentChannelTypeDef channel, uint16_t raw, uint32_t *ma)
{
    uint8_t ratio;
    uint16_t rsense_mohm;

    if ((ma == NULL) || (raw > 1023u))
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

    *ma = (((uint32_t)raw + SC8815_ADC_VALUE_OFFSET) * SC8815_ADC_CURRENT_NUMERATOR *
           (uint32_t)ratio * SC8815_ADC_CURRENT_REF_RSENSE_MOHM * 1000u) /
          (SC8815_ADC_CURRENT_DENOMINATOR * (uint32_t)rsense_mohm);
    return INT_SC8815_OK;
}

Int_SC8815_StatusTypeDef Int_SC8815_SetCurrentLimitMa(Int_SC8815_CurrentLimitTypeDef type,
                                                      uint16_t current_ma)
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

Int_SC8815_StatusTypeDef Int_SC8815_GetCurrentLimitMa(Int_SC8815_CurrentLimitTypeDef type,
                                                      uint32_t *current_ma)
{
    uint8_t reg;
    uint8_t ratio;
    uint16_t rsense_mohm;
    uint8_t code;
    Int_SC8815_StatusTypeDef ret;

    if (current_ma == NULL)
    {
        return INT_SC8815_ERROR_PARAM;
    }
    switch (type)
    {
        case INT_SC8815_LIMIT_IBUS:
            reg = SC8815_REG_IBUS_LIM_SET;
            ratio = SC8815_PROJECT_IBUS_RATIO_X;
            rsense_mohm = SC8815_PROJECT_RSNS_IBUS_MOHM;
            break;
        case INT_SC8815_LIMIT_IBAT:
            reg = SC8815_REG_IBAT_LIM_SET;
            ratio = SC8815_PROJECT_IBAT_RATIO_X;
            rsense_mohm = SC8815_PROJECT_RSNS_IBAT_MOHM;
            break;
        default:
            return INT_SC8815_ERROR_PARAM;
    }

    ret = Int_SC8815_ReadRegRaw(reg, &code);
    if (ret != INT_SC8815_OK)
    {
        return ret;
    }
    *current_ma = Int_SC8815_CurrentLimitCodeToMa(code, ratio, rsense_mohm);
    return INT_SC8815_OK;
}

Int_SC8815_StatusTypeDef Int_SC8815_ProbeAddress(uint8_t addr_7bit, bool swapped)
{
    Int_SC8815_StatusTypeDef ret;

    if ((addr_7bit > 0x7Fu) || (swapped != (SC8815_PROJECT_IIC_LINE_SWAPPED != 0u)))
    {
        return (addr_7bit > 0x7Fu) ? INT_SC8815_ERROR_PARAM : INT_SC8815_ERROR_GUARD;
    }
    if (!Int_SC8815_BusAcquire())
    {
        return INT_SC8815_ERROR_STATE;
    }
    s_sc8815_bus_stats.transaction_count++;
    ret = Int_SC8815_BusStart();
    if (ret == INT_SC8815_OK)
    {
        ret = Int_SC8815_BusWriteByte((uint8_t)(addr_7bit << 1u));
    }
    Int_SC8815_BusStop();
    Int_SC8815_BusRelease();

    return ret;
}

Int_SC8815_StatusTypeDef Int_SC8815_ReadRegWithLineOrder(uint8_t reg, bool swapped, uint8_t *value)
{
    if (swapped != (SC8815_PROJECT_IIC_LINE_SWAPPED != 0u))
    {
        return INT_SC8815_ERROR_GUARD;
    }
    return Int_SC8815_ReadRegRawOnce(reg, value);
}

bool Int_SC8815_IsIicLineSwapped(void)
{
    return SC8815_PROJECT_IIC_LINE_SWAPPED != 0u;
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

void Int_SC8815_GetBusStats(Int_SC8815_BusStatsTypeDef *stats)
{
    uint32_t primask;

    if (stats == NULL)
    {
        return;
    }
    primask = Int_SC8815_EnterCritical();
    *stats = s_sc8815_bus_stats;
    Int_SC8815_ExitCritical(primask);
}

void Int_SC8815_ResetBusStats(void)
{
    uint32_t primask = Int_SC8815_EnterCritical();

    s_sc8815_bus_stats.transaction_count = 0u;
    s_sc8815_bus_stats.nack_count = 0u;
    s_sc8815_bus_stats.timeout_count = 0u;
    s_sc8815_bus_stats.recovery_count = 0u;
    s_sc8815_bus_stats.busy_count = 0u;
    Int_SC8815_ExitCritical(primask);
}
