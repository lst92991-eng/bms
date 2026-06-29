#ifndef INT_EEPROM_H
#define INT_EEPROM_H

#include <stdbool.h>
#include <stdint.h>

/* M24C64：8KB、32B/page，A0/A1/A2 全接地，7-bit 地址为 0x50。 */
#define INT_EEPROM_SIZE_BYTES       8192u
#define INT_EEPROM_PAGE_SIZE_BYTES  32u
#define INT_EEPROM_BASE_ADDR_7BIT   0x50u
#define INT_EEPROM_DEV_ADDR         ((uint16_t)(INT_EEPROM_BASE_ADDR_7BIT << 1u))

#define INT_EEPROM_READ_TIMEOUT_MS   100u
#define INT_EEPROM_WRITE_TIMEOUT_MS  100u
#define INT_EEPROM_WAIT_TIMEOUT_MS   10u

typedef enum
{
    INT_EEPROM_OK = 0,
    INT_EEPROM_ERROR,
    INT_EEPROM_TIMEOUT
} Int_EEPROM_StatusTypeDef;

Int_EEPROM_StatusTypeDef Int_EEPROM_Init(void);
bool Int_EEPROM_IsOnline(void);
bool Int_EEPROM_IsReady(uint32_t timeout_ms);
Int_EEPROM_StatusTypeDef Int_EEPROM_Read(uint16_t address, uint8_t *data, uint16_t len);
Int_EEPROM_StatusTypeDef Int_EEPROM_Write(uint16_t address, const uint8_t *data, uint16_t len);
Int_EEPROM_StatusTypeDef Int_EEPROM_WriteReadback(uint16_t address, const uint8_t *data, uint16_t len);

#endif
