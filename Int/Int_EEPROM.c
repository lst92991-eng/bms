#include "Int_EEPROM.h"

#include <string.h>

#include "i2c.h"

static bool s_eeprom_online = false;

static bool Int_EEPROM_RangeValid(uint16_t address, uint16_t len)
{
    if (len == 0u)
    {
        return address <= INT_EEPROM_SIZE_BYTES;
    }

    if (address >= INT_EEPROM_SIZE_BYTES)
    {
        return false;
    }

    return (uint32_t)len <= ((uint32_t)INT_EEPROM_SIZE_BYTES - address);
}

static uint16_t Int_EEPROM_PageRemaining(uint16_t address)
{
    return (uint16_t)(INT_EEPROM_PAGE_SIZE_BYTES -
                      (address % INT_EEPROM_PAGE_SIZE_BYTES));
}

static Int_EEPROM_StatusTypeDef Int_EEPROM_FromHalStatus(HAL_StatusTypeDef status)
{
    if (status == HAL_OK)
    {
        return INT_EEPROM_OK;
    }
    if (status == HAL_TIMEOUT)
    {
        return INT_EEPROM_TIMEOUT;
    }
    return INT_EEPROM_ERROR;
}

static Int_EEPROM_StatusTypeDef Int_EEPROM_WaitReady(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    /* 写周期内器件会 NACK，轮询 ACK 可避免固定延时过短或无谓等待。 */
    do
    {
        if (HAL_I2C_IsDeviceReady(&hi2c2,
                                  INT_EEPROM_DEV_ADDR,
                                  1u,
                                  1u) == HAL_OK)
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
    /* 启动阶段只探测 ACK，不写 EEPROM，也不重复初始化 CubeMX 的 I2C2。 */
    return Int_EEPROM_WaitReady(INT_EEPROM_READY_TIMEOUT_MS);
}

bool Int_EEPROM_IsOnline(void)
{
    return s_eeprom_online;
}

bool Int_EEPROM_IsReady(uint32_t timeout_ms)
{
    return Int_EEPROM_WaitReady(timeout_ms) == INT_EEPROM_OK;
}

Int_EEPROM_StatusTypeDef Int_EEPROM_Read(uint16_t address,
                                         uint8_t *data,
                                         uint16_t len)
{
    uint16_t offset = 0u;

    if (((data == 0) && (len > 0u)) || !Int_EEPROM_RangeValid(address, len))
    {
        return INT_EEPROM_ERROR;
    }
    if (len == 0u)
    {
        return INT_EEPROM_OK;
    }

    while (offset < len)
    {
        uint16_t current_address = (uint16_t)(address + offset);
        uint16_t chunk = Int_EEPROM_PageRemaining(current_address);
        Int_EEPROM_StatusTypeDef status;

        if (chunk > (uint16_t)(len - offset))
        {
            chunk = (uint16_t)(len - offset);
        }

        /* 分段读取可跨任意页，同时避免大块读取耗尽单次 HAL 超时窗口。 */
        status = Int_EEPROM_FromHalStatus(
            HAL_I2C_Mem_Read(&hi2c2,
                             INT_EEPROM_DEV_ADDR,
                             current_address,
                             I2C_MEMADD_SIZE_16BIT,
                             &data[offset],
                             chunk,
                             INT_EEPROM_READ_TIMEOUT_MS));
        if (status != INT_EEPROM_OK)
        {
            s_eeprom_online = false;
            return status;
        }

        offset = (uint16_t)(offset + chunk);
    }

    s_eeprom_online = true;
    return INT_EEPROM_OK;
}

Int_EEPROM_StatusTypeDef Int_EEPROM_Write(uint16_t address,
                                          const uint8_t *data,
                                          uint16_t len)
{
    uint16_t offset = 0u;

    if (((data == 0) && (len > 0u)) || !Int_EEPROM_RangeValid(address, len))
    {
        return INT_EEPROM_ERROR;
    }

    while (offset < len)
    {
        uint16_t current_address = (uint16_t)(address + offset);
        uint16_t chunk = Int_EEPROM_PageRemaining(current_address);
        Int_EEPROM_StatusTypeDef status;

        if (chunk > (uint16_t)(len - offset))
        {
            chunk = (uint16_t)(len - offset);
        }

        /* 页写越界会在当前页内回卷，因此每次事务都限制在同一页。 */
        status = Int_EEPROM_FromHalStatus(
            HAL_I2C_Mem_Write(&hi2c2,
                              INT_EEPROM_DEV_ADDR,
                              current_address,
                              I2C_MEMADD_SIZE_16BIT,
                              (uint8_t *)&data[offset],
                              chunk,
                              INT_EEPROM_WRITE_TIMEOUT_MS));
        if (status != INT_EEPROM_OK)
        {
            s_eeprom_online = false;
            return status;
        }

        status = Int_EEPROM_WaitReady(INT_EEPROM_READY_TIMEOUT_MS);
        if (status != INT_EEPROM_OK)
        {
            return status;
        }

        offset = (uint16_t)(offset + chunk);
    }

    return INT_EEPROM_OK;
}

Int_EEPROM_StatusTypeDef Int_EEPROM_WriteReadback(uint16_t address,
                                                  const uint8_t *data,
                                                  uint16_t len)
{
    uint8_t verify[INT_EEPROM_PAGE_SIZE_BYTES];
    uint16_t offset = 0u;
    Int_EEPROM_StatusTypeDef status;

    status = Int_EEPROM_Write(address, data, len);
    if (status != INT_EEPROM_OK)
    {
        return status;
    }

    while (offset < len)
    {
        uint16_t chunk = (uint16_t)(len - offset);

        if (chunk > (uint16_t)sizeof(verify))
        {
            chunk = (uint16_t)sizeof(verify);
        }

        status = Int_EEPROM_Read((uint16_t)(address + offset), verify, chunk);
        if (status != INT_EEPROM_OK)
        {
            return status;
        }
        if (memcmp(verify, &data[offset], chunk) != 0)
        {
            return INT_EEPROM_ERROR;
        }

        offset = (uint16_t)(offset + chunk);
    }

    return INT_EEPROM_OK;
}
