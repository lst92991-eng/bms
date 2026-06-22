#ifndef __COM_BQ769_H_
#define __COM_BQ769_H_
#include "stdint.h"
#include "stdio.h"

/**
 * @brief OCV 电压-SOC 查表（每节电芯，0~100% 对应的 OCV(mV)）
 *
 * 说明：该表定义在 `Com_Bq769.c`，这里用 extern 暴露给 SOC 算法做斜率/权重自适应。
 */
extern int voltage_soc_ocv_table[101];
/**
 * @brief 通过查表法把电阻换算为温度
 */
int8_t Com_BQ769_getTemperByResist(uint32_t resistance);

/**
 * @brief 通过查表法把电压换算为电量百分比
 */
uint8_t Com_BQ769_getPercentByVoltage(uint16_t voltage);

#endif // __COM_BQ769_H_
