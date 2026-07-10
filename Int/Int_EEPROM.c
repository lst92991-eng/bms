#include "Int_EEPROM.h"

#include "i2c.h"

enum
{
    INT_EEPROM_DEVICE_ADDR = 0xA0u,
    INT_EEPROM_READY_TIMEOUT_MS = 10u
};

Int_EEPROM_StatusTypeDef Int_EEPROM_Init(void)
{
    uint32_t start = HAL_GetTick();

    /* 启动阶段只探测 ACK，不写 EEPROM，避免清洗/测试过程误改持久化数据。 */
    do
    {
        if (HAL_I2C_IsDeviceReady(&hi2c2, INT_EEPROM_DEVICE_ADDR, 1u, 1u) == HAL_OK)
        {
            return INT_EEPROM_OK;
        }
    } while ((uint32_t)(HAL_GetTick() - start) < INT_EEPROM_READY_TIMEOUT_MS);

    return INT_EEPROM_TIMEOUT;
}
