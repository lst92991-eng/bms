#ifndef __INT_BQ769_H_
#define __INT_BQ769_H_
#include "gpio.h"
#include "i2c.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stdbool.h"

#include "Int_BQ769_BSP.h"

#define BQ769_I2C_ADDRESS_WRITE 0x10
#define BQ769_I2C_ADDRESS_READ 0x11

/**
 * @brief 唤醒
 */
void Int_BQ769_WakeUp(void);

/**
 * @brief 休眠
 */
void Int_BQ769_Ship(void);

/**
 * @brief 重置
 */
void Int_BQ769_Reset(void);

/**
 * @brief 向BQ769的寄存器写入数据（单个字节)
 */
void Int_BQ769_WriteReg(uint8_t reg, uint8_t data);

/**
 * @brief 从BQ769的寄存器读取多个字节
 */
void Int_BQ769_ReadReg(uint8_t reg, uint8_t *buff, uint16_t read_len);

bool Int_BQ769_Read(uint8_t reg, uint8_t *read_buff, uint8_t read_len);

#endif // __INT_BQ769_H_
