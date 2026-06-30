#ifndef INT_SC8815_H
#define INT_SC8815_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    INT_SC8815_OK = 0,
    INT_SC8815_ERROR,
    INT_SC8815_ERROR_PARAM,
    INT_SC8815_ERROR_ACK,
    INT_SC8815_ERROR_GUARD,
    INT_SC8815_ERROR_TIMEOUT,
    INT_SC8815_ERROR_STATE,
    INT_SC8815_ERROR_RANGE
} Int_SC8815_StatusTypeDef;

typedef enum
{
    INT_SC8815_ADC_VBUS = 0,
    INT_SC8815_ADC_VBAT,
    INT_SC8815_ADC_ADIN
} Int_SC8815_AdcChannelTypeDef;

typedef enum
{
    INT_SC8815_CURRENT_IBUS = 0,
    INT_SC8815_CURRENT_IBAT
} Int_SC8815_CurrentChannelTypeDef;

typedef enum
{
    INT_SC8815_LIMIT_IBUS = 0,
    INT_SC8815_LIMIT_IBAT
} Int_SC8815_CurrentLimitTypeDef;

typedef struct
{
    bool ac_ok;
    bool indet;
    bool vbus_short;
    bool otp;
    bool eoc;
    uint8_t raw;
} Int_SC8815_StatusFlagsTypeDef;

/**
 * @brief 进入 SC8815 安全态。
 * @note 只配置 MCU 控制脚到安全电平：PSTOP=standby，#CE=disable；不启动充电。
 */
Int_SC8815_StatusTypeDef Int_SC8815_InitSafe(void);

/**
 * @brief 控制 #CE，低有效。
 */
Int_SC8815_StatusTypeDef Int_SC8815_SetChipEnabled(bool enabled);

/**
 * @brief 控制 PSTOP，高电平 standby。
 */
Int_SC8815_StatusTypeDef Int_SC8815_SetStandby(bool standby);

/**
 * @brief 读取 SC8815 单字节寄存器。
 */
Int_SC8815_StatusTypeDef Int_SC8815_ReadReg(uint8_t reg, uint8_t *value);

/**
 * @brief 写 SC8815 单字节寄存器。
 * @note 所有写入都会经过项目 guard，禁止 OTG、反向输出、关闭关键保护和非法限流。
 */
Int_SC8815_StatusTypeDef Int_SC8815_WriteReg(uint8_t reg, uint8_t value);

/**
 * @brief 读改写 SC8815 单字节寄存器。
 */
Int_SC8815_StatusTypeDef Int_SC8815_UpdateReg(uint8_t reg, uint8_t clear_mask, uint8_t set_mask);

/**
 * @brief 读取 STATUS 并解析常用标志。
 */
Int_SC8815_StatusTypeDef Int_SC8815_ReadStatus(Int_SC8815_StatusFlagsTypeDef *status);

/**
 * @brief 启动或停止 SC8815 ADC。
 */
Int_SC8815_StatusTypeDef Int_SC8815_SetAdcEnabled(bool enabled);

/**
 * @brief 读取 ADC 原始 10-bit 值。
 */
Int_SC8815_StatusTypeDef Int_SC8815_ReadAdcRaw(Int_SC8815_AdcChannelTypeDef channel, uint16_t *raw);

/**
 * @brief 读取电压 ADC 并换算为 mV。
 */
Int_SC8815_StatusTypeDef Int_SC8815_ReadAdcVoltageMv(Int_SC8815_AdcChannelTypeDef channel, uint32_t *mv);

/**
 * @brief 读取电流 ADC 并换算为 mA。
 */
Int_SC8815_StatusTypeDef Int_SC8815_ReadAdcCurrentMa(Int_SC8815_CurrentChannelTypeDef channel, uint32_t *ma);

/**
 * @brief 设置输入或电池侧限流，单位 mA。
 * @note 小于 300mA 或超过项目上限会被拒绝。
 */
Int_SC8815_StatusTypeDef Int_SC8815_SetCurrentLimitMa(Int_SC8815_CurrentLimitTypeDef type, uint16_t current_ma);

bool Int_SC8815_IsIicLineSwapped(void);
uint8_t Int_SC8815_GetBusLevels(void);

#endif /* INT_SC8815_H */
