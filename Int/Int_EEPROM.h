#ifndef INT_EEPROM_H
#define INT_EEPROM_H

#include <stdbool.h>
#include <stdint.h>

/* M24C64：8 KiB、32 B/page，A0/A1/A2 接地，7-bit 地址为 0x50。 */
#define INT_EEPROM_SIZE_BYTES 8192u
#define INT_EEPROM_PAGE_SIZE_BYTES 32u
#define INT_EEPROM_BASE_ADDR_7BIT 0x50u
#define INT_EEPROM_DEV_ADDR ((uint16_t)(INT_EEPROM_BASE_ADDR_7BIT << 1u))

#define INT_EEPROM_READ_TIMEOUT_MS 100u
#define INT_EEPROM_WRITE_TIMEOUT_MS 100u
#define INT_EEPROM_READY_TIMEOUT_MS 10u

typedef enum
{
    INT_EEPROM_OK = 0,
    INT_EEPROM_ERROR,
    INT_EEPROM_TIMEOUT
} Int_EEPROM_StatusTypeDef;

/* I2C2 由 CubeMX 初始化；此接口只探测器件，不重复初始化总线。 */
Int_EEPROM_StatusTypeDef Int_EEPROM_Init(void);
bool Int_EEPROM_IsOnline(void);
bool Int_EEPROM_IsReady(uint32_t timeout_ms);

/* 支持 EEPROM 全地址空间内的任意字节块，长度为 0 时不访问总线。 */
Int_EEPROM_StatusTypeDef Int_EEPROM_Read(uint16_t address, uint8_t *data, uint16_t len);
Int_EEPROM_StatusTypeDef Int_EEPROM_Write(uint16_t address, const uint8_t *data, uint16_t len);
Int_EEPROM_StatusTypeDef
Int_EEPROM_WriteReadback(uint16_t address, const uint8_t *data, uint16_t len);

#endif /* INT_EEPROM_H */
