#include "Int_EEPROM.h"

#include <string.h>

#include "i2c.h"

static bool s_eeprom_online = false;

static bool Int_EEPROM_AddressOk(uint16_t address, uint16_t len)
{
    if (len == 0u)
    {
        return true;
    }

    if (address >= INT_EEPROM_SIZE_BYTES)
    {
        return false;
    }

    return ((uint32_t)address + len) <= INT_EEPROM_SIZE_BYTES;
}

static uint16_t Int_EEPROM_PageLeft(uint16_t address)
{
    return (uint16_t)(INT_EEPROM_PAGE_SIZE_BYTES -
                      (address % INT_EEPROM_PAGE_SIZE_BYTES));
}

static Int_EEPROM_StatusTypeDef Int_EEPROM_WaitReady(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    do
    {
        if (HAL_I2C_IsDeviceReady(&hi2c2, INT_EEPROM_DEV_ADDR, 1u, 1u) == HAL_OK)
        {
            s_eeprom_online = true;
            return INT_EEPROM_OK;
        }
    } while ((uint32_t)(HAL_GetTick() - start) < timeout_ms);

    s_eeprom_online = false;
    return INT_EEPROM_TIMEOUT;
}

Int_EEPROM_StatusTypeDef Int_EEPROM_Init(void)
{
    /* 教学版初始化只探测 ACK，不写 EEPROM，避免误改持久化数据。 */
    return Int_EEPROM_WaitReady(INT_EEPROM_WAIT_TIMEOUT_MS);
}

bool Int_EEPROM_IsOnline(void)
{
    return s_eeprom_online;
}

bool Int_EEPROM_IsReady(uint32_t timeout_ms)
{
    return Int_EEPROM_WaitReady(timeout_ms) == INT_EEPROM_OK;
}

Int_EEPROM_StatusTypeDef Int_EEPROM_Read(uint16_t address, uint8_t *data, uint16_t len)
{
    if (((data == 0) && (len > 0u)) || !Int_EEPROM_AddressOk(address, len))
    {
        return INT_EEPROM_ERROR;
    }

    if (len == 0u)
    {
        return INT_EEPROM_OK;
    }

    if (HAL_I2C_Mem_Read(&hi2c2,
                         INT_EEPROM_DEV_ADDR,
                         address,
                         I2C_MEMADD_SIZE_16BIT,
                         data,
                         len,
                         INT_EEPROM_READ_TIMEOUT_MS) != HAL_OK)
    {
        s_eeprom_online = false;
        return INT_EEPROM_ERROR;
    }

    s_eeprom_online = true;
    return INT_EEPROM_OK;
}

Int_EEPROM_StatusTypeDef Int_EEPROM_Write(uint16_t address, const uint8_t *data, uint16_t len)
{
    uint16_t offset = 0u;

    if (((data == 0) && (len > 0u)) || !Int_EEPROM_AddressOk(address, len))
    {
        return INT_EEPROM_ERROR;
    }

    /*
     * M24C64 跨页写入会在页内回卷，所以这里按 32B 页边界拆开。
     * 先把规则写直观，后续如果要做磨损均衡，再放到 APP/Com 层。
     */
    while (offset < len)
    {
        uint16_t chunk = Int_EEPROM_PageLeft((uint16_t)(address + offset));
        if (chunk > (uint16_t)(len - offset))
        {
            chunk = (uint16_t)(len - offset);
        }

        if (HAL_I2C_Mem_Write(&hi2c2,
                              INT_EEPROM_DEV_ADDR,
                              (uint16_t)(address + offset),
                              I2C_MEMADD_SIZE_16BIT,
                              (uint8_t *)&data[offset],
                              chunk,
                              INT_EEPROM_WRITE_TIMEOUT_MS) != HAL_OK)
        {
            s_eeprom_online = false;
            return INT_EEPROM_ERROR;
        }

        if (Int_EEPROM_WaitReady(INT_EEPROM_WAIT_TIMEOUT_MS) != INT_EEPROM_OK)
        {
            return INT_EEPROM_TIMEOUT;
        }

        offset = (uint16_t)(offset + chunk);
    }

    return INT_EEPROM_OK;
}

Int_EEPROM_StatusTypeDef Int_EEPROM_WriteReadback(uint16_t address, const uint8_t *data, uint16_t len)
{
    uint8_t read_buf[INT_EEPROM_PAGE_SIZE_BYTES];
    uint16_t offset = 0u;

    if (Int_EEPROM_Write(address, data, len) != INT_EEPROM_OK)
    {
        return INT_EEPROM_ERROR;
    }

    while (offset < len)
    {
        uint16_t chunk = (uint16_t)(len - offset);
        if (chunk > sizeof(read_buf))
        {
            chunk = sizeof(read_buf);
        }

        if (Int_EEPROM_Read((uint16_t)(address + offset), read_buf, chunk) != INT_EEPROM_OK)
        {
            return INT_EEPROM_ERROR;
        }

        if (memcmp(read_buf, &data[offset], chunk) != 0)
        {
            return INT_EEPROM_ERROR;
        }

        offset = (uint16_t)(offset + chunk);
    }

    return INT_EEPROM_OK;
}
