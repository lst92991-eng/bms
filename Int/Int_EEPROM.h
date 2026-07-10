#ifndef INT_EEPROM_H
#define INT_EEPROM_H

typedef enum
{
    INT_EEPROM_OK = 0,
    INT_EEPROM_TIMEOUT
} Int_EEPROM_StatusTypeDef;

/* 当前业务未使用持久化数据；这里只保留启动 ACK 探测。 */
Int_EEPROM_StatusTypeDef Int_EEPROM_Init(void);

#endif /* INT_EEPROM_H */
