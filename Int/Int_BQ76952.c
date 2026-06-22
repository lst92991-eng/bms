#include "Int_BQ76952.h"

#include "Int_BQ76952_BSP.h"
#include "i2c.h"

#ifndef INT_BQ76952_I2C_HANDLE
#define INT_BQ76952_I2C_HANDLE hi2c1
#endif

#define INT_BQ76952_I2C_ADDR             BQ76952_I2C_8BIT_WRITE_ADDR_DEFAULT
#define INT_BQ76952_I2C_TIMEOUT_MS       (100u)
#define INT_BQ76952_ECHO_POLL_COUNT      (100u)
#define INT_BQ76952_CFG_POLL_COUNT       (100u)
#define INT_BQ76952_POLL_DELAY_MS        (1u)
#define INT_BQ76952_DIRECT_MAX_LEN       (34u)
#define INT_BQ76952_TRANSFER_MAX_LEN     BQ76952_TRANSFER_BUFFER_SIZE

extern I2C_HandleTypeDef INT_BQ76952_I2C_HANDLE;

static bool s_bq76952_crc_enabled = false;

/* I2C CRC 使用 TRM 指定的 0x07 多项式；此 CRC 只保护总线字节，不等同于 transfer buffer checksum。 */
static uint8_t Int_BQ76952_Crc8Update(uint8_t crc, uint8_t data)
{
    crc ^= data;

    for (uint8_t bit = 0u; bit < 8u; bit++)
    {
        if ((crc & 0x80u) != 0u)
        {
            crc = (uint8_t)((crc << 1u) ^ BQ76952_CRC8_POLY);
        }
        else
        {
            crc = (uint8_t)(crc << 1u);
        }
    }

    return crc;
}

static uint8_t Int_BQ76952_Crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = BQ76952_CRC8_INIT;

    for (uint8_t i = 0u; i < len; i++)
    {
        crc = Int_BQ76952_Crc8Update(crc, data[i]);
    }

    return crc;
}

static uint8_t Int_BQ76952_BufferChecksum(uint16_t command_or_address,
                                          const uint8_t *data,
                                          uint8_t len)
{
    uint16_t sum = (uint8_t)(command_or_address & 0xFFu);

    sum = (uint16_t)(sum + (uint8_t)(command_or_address >> 8u));

    for (uint8_t i = 0u; i < len; i++)
    {
        sum = (uint16_t)(sum + data[i]);
    }

    return (uint8_t)(~sum);
}

/* 只限制本驱动当前能安全处理的 block 长度；BQ76952 transfer buffer 最大为 32 byte。 */
static Int_BQ76952_StatusTypeDef Int_BQ76952_CheckLen(uint8_t len)
{
    if (len == 0u)
    {
        return INT_BQ76952_ERROR_LENGTH;
    }

    if (len > INT_BQ76952_TRANSFER_MAX_LEN)
    {
        return INT_BQ76952_ERROR_LENGTH;
    }

    return INT_BQ76952_OK;
}

/* 读回型 subcommand/Data Memory 需要等待 0x3E/0x3F echo，command-only subcommand 不走这里。 */
static Int_BQ76952_StatusTypeDef Int_BQ76952_WaitEcho(uint16_t command_or_address)
{
    uint8_t echo[2];

    for (uint16_t poll = 0u; poll < INT_BQ76952_ECHO_POLL_COUNT; poll++)
    {
        Int_BQ76952_StatusTypeDef ret;

        ret = Int_BQ76952_ReadDirect(BQ76952_SUBCMD_ADDR_LSB, echo, 2u);
        if (ret != INT_BQ76952_OK)
        {
            return ret;
        }

        if ((((uint16_t)echo[1] << 8u) | echo[0]) == command_or_address)
        {
            return INT_BQ76952_OK;
        }

        HAL_Delay(INT_BQ76952_POLL_DELAY_MS);
    }

    return INT_BQ76952_ERROR_TIMEOUT;
}

/*
 * 读取 transfer buffer 的顺序按 TRM 固定：
 * 先读 0x61 length，再读 0x40 buffer，最后读 0x60 checksum。
 * 避免在读取 buffer 前连续读 0x60/0x61 触发自动递增副作用。
 */
static Int_BQ76952_StatusTypeDef Int_BQ76952_ReadTransfer(uint16_t command_or_address,
                                                          uint8_t *data,
                                                          uint8_t len)
{
    uint8_t raw[INT_BQ76952_TRANSFER_MAX_LEN];
    Int_BQ76952_StatusTypeDef ret;
    uint8_t length;
    uint8_t data_len;
    uint8_t checksum;

    ret = Int_BQ76952_CheckLen(len);
    if (ret != INT_BQ76952_OK)
    {
        return ret;
    }

    ret = Int_BQ76952_WaitEcho(command_or_address);
    if (ret != INT_BQ76952_OK)
    {
        return ret;
    }

    ret = Int_BQ76952_ReadDirect(BQ76952_TRANSFER_LENGTH, &length, 1u);
    if (ret != INT_BQ76952_OK)
    {
        return ret;
    }

    if (length < BQ76952_TRANSFER_LENGTH_OVERHEAD)
    {
        return INT_BQ76952_ERROR_LENGTH;
    }

    data_len = (uint8_t)(length - BQ76952_TRANSFER_LENGTH_OVERHEAD);
    if ((data_len == 0u) || (data_len > INT_BQ76952_TRANSFER_MAX_LEN) || (len > data_len))
    {
        return INT_BQ76952_ERROR_LENGTH;
    }

    ret = Int_BQ76952_ReadDirect(BQ76952_TRANSFER_BUFFER_START, raw, data_len);
    if (ret != INT_BQ76952_OK)
    {
        return ret;
    }

    ret = Int_BQ76952_ReadDirect(BQ76952_TRANSFER_CHECKSUM, &checksum, 1u);
    if (ret != INT_BQ76952_OK)
    {
        return ret;
    }

    if (Int_BQ76952_BufferChecksum(command_or_address, raw, data_len) != checksum)
    {
        return INT_BQ76952_ERROR_CHECKSUM;
    }

    for (uint8_t i = 0u; i < len; i++)
    {
        data[i] = raw[i];
    }

    return INT_BQ76952_OK;
}

void Int_BQ76952_SetCrcEnabled(bool enabled)
{
    s_bq76952_crc_enabled = enabled;
}

bool Int_BQ76952_IsCrcEnabled(void)
{
    return s_bq76952_crc_enabled;
}

Int_BQ76952_StatusTypeDef Int_BQ76952_ReadDirect(uint8_t command, uint8_t *data, uint8_t len)
{
    if (data == NULL)
    {
        return INT_BQ76952_ERROR_PARAM;
    }

    if ((len == 0u) || (len > INT_BQ76952_DIRECT_MAX_LEN))
    {
        return INT_BQ76952_ERROR_LENGTH;
    }

    if (!s_bq76952_crc_enabled)
    {
        if (HAL_I2C_Mem_Read(&INT_BQ76952_I2C_HANDLE,
                             INT_BQ76952_I2C_ADDR,
                             command,
                             I2C_MEMADD_SIZE_8BIT,
                             data,
                             len,
                             INT_BQ76952_I2C_TIMEOUT_MS) != HAL_OK)
        {
            return INT_BQ76952_ERROR_HAL;
        }

        return INT_BQ76952_OK;
    }

    if (HAL_I2C_Master_Transmit(&INT_BQ76952_I2C_HANDLE,
                                INT_BQ76952_I2C_ADDR,
                                &command,
                                1u,
                                INT_BQ76952_I2C_TIMEOUT_MS) != HAL_OK)
    {
        return INT_BQ76952_ERROR_HAL;
    }

    {
        uint8_t rx[INT_BQ76952_DIRECT_MAX_LEN * 2u];

        if (HAL_I2C_Master_Receive(&INT_BQ76952_I2C_HANDLE,
                                   INT_BQ76952_I2C_ADDR,
                                   rx,
                                   (uint16_t)(len * 2u),
                                   INT_BQ76952_I2C_TIMEOUT_MS) != HAL_OK)
        {
            return INT_BQ76952_ERROR_HAL;
        }

        for (uint8_t i = 0u; i < len; i++)
        {
            uint8_t crc_input[4];
            uint8_t crc;

            if (i == 0u)
            {
                crc_input[0] = BQ76952_I2C_8BIT_WRITE_ADDR_DEFAULT;
                crc_input[1] = command;
                crc_input[2] = BQ76952_I2C_8BIT_READ_ADDR_DEFAULT;
                crc_input[3] = rx[0];
                crc = Int_BQ76952_Crc8(crc_input, 4u);
            }
            else
            {
                crc = Int_BQ76952_Crc8(&rx[i * 2u], 1u);
            }

            if (crc != rx[(i * 2u) + 1u])
            {
                return INT_BQ76952_ERROR_CRC;
            }

            data[i] = rx[i * 2u];
        }
    }

    return INT_BQ76952_OK;
}

Int_BQ76952_StatusTypeDef Int_BQ76952_WriteDirect(uint8_t command, const uint8_t *data, uint8_t len)
{
    if (data == NULL)
    {
        return INT_BQ76952_ERROR_PARAM;
    }

    if ((len == 0u) || (len > INT_BQ76952_DIRECT_MAX_LEN))
    {
        return INT_BQ76952_ERROR_LENGTH;
    }

    if (!s_bq76952_crc_enabled)
    {
        if (HAL_I2C_Mem_Write(&INT_BQ76952_I2C_HANDLE,
                              INT_BQ76952_I2C_ADDR,
                              command,
                              I2C_MEMADD_SIZE_8BIT,
                              (uint8_t *)data,
                              len,
                              INT_BQ76952_I2C_TIMEOUT_MS) != HAL_OK)
        {
            return INT_BQ76952_ERROR_HAL;
        }

        return INT_BQ76952_OK;
    }

    {
        uint8_t tx[1u + (INT_BQ76952_DIRECT_MAX_LEN * 2u)];
        uint8_t crc_input[3];

        tx[0] = command;

        for (uint8_t i = 0u; i < len; i++)
        {
            tx[1u + (i * 2u)] = data[i];

            if (i == 0u)
            {
                crc_input[0] = BQ76952_I2C_8BIT_WRITE_ADDR_DEFAULT;
                crc_input[1] = command;
                crc_input[2] = data[i];
                tx[2u + (i * 2u)] = Int_BQ76952_Crc8(crc_input, 3u);
            }
            else
            {
                tx[2u + (i * 2u)] = Int_BQ76952_Crc8(&data[i], 1u);
            }
        }

        if (HAL_I2C_Master_Transmit(&INT_BQ76952_I2C_HANDLE,
                                    INT_BQ76952_I2C_ADDR,
                                    tx,
                                    (uint16_t)(1u + (len * 2u)),
                                    INT_BQ76952_I2C_TIMEOUT_MS) != HAL_OK)
        {
            return INT_BQ76952_ERROR_HAL;
        }
    }

    return INT_BQ76952_OK;
}

Int_BQ76952_StatusTypeDef Int_BQ76952_SendSubcommand(uint16_t subcommand)
{
    uint8_t data[2];

    data[0] = (uint8_t)(subcommand & 0xFFu);
    data[1] = (uint8_t)(subcommand >> 8u);

    return Int_BQ76952_WriteDirect(BQ76952_SUBCMD_ADDR_LSB, data, 2u);
}

Int_BQ76952_StatusTypeDef Int_BQ76952_ReadSubcommand(uint16_t subcommand, uint8_t *data, uint8_t len)
{
    uint8_t command[2];
    Int_BQ76952_StatusTypeDef ret;

    if (data == NULL)
    {
        return INT_BQ76952_ERROR_PARAM;
    }

    command[0] = (uint8_t)(subcommand & 0xFFu);
    command[1] = (uint8_t)(subcommand >> 8u);

    ret = Int_BQ76952_WriteDirect(BQ76952_SUBCMD_ADDR_LSB, command, 2u);
    if (ret != INT_BQ76952_OK)
    {
        return ret;
    }

    return Int_BQ76952_ReadTransfer(subcommand, data, len);
}

Int_BQ76952_StatusTypeDef Int_BQ76952_ReadDataMemory(uint16_t address, uint8_t *data, uint8_t len)
{
    uint8_t command[2];
    Int_BQ76952_StatusTypeDef ret;

    if (data == NULL)
    {
        return INT_BQ76952_ERROR_PARAM;
    }

    command[0] = (uint8_t)(address & 0xFFu);
    command[1] = (uint8_t)(address >> 8u);

    ret = Int_BQ76952_WriteDirect(BQ76952_SUBCMD_ADDR_LSB, command, 2u);
    if (ret != INT_BQ76952_OK)
    {
        return ret;
    }

    return Int_BQ76952_ReadTransfer(address, data, len);
}

Int_BQ76952_StatusTypeDef Int_BQ76952_WriteDataMemory(uint16_t address, const uint8_t *data, uint8_t len)
{
    uint8_t transfer[2u + INT_BQ76952_TRANSFER_MAX_LEN];
    uint8_t meta[2];
    Int_BQ76952_StatusTypeDef ret;

    if (data == NULL)
    {
        return INT_BQ76952_ERROR_PARAM;
    }

    ret = Int_BQ76952_CheckLen(len);
    if (ret != INT_BQ76952_OK)
    {
        return ret;
    }

    transfer[0] = (uint8_t)(address & 0xFFu);
    transfer[1] = (uint8_t)(address >> 8u);

    for (uint8_t i = 0u; i < len; i++)
    {
        transfer[2u + i] = data[i];
    }

    ret = Int_BQ76952_WriteDirect(BQ76952_SUBCMD_ADDR_LSB, transfer, (uint8_t)(len + 2u));
    if (ret != INT_BQ76952_OK)
    {
        return ret;
    }

    /* Data Memory 写入必须在写完 0x3E/0x3F+data 后，再把 checksum/length 作为 word 写到 0x60/0x61。 */
    meta[0] = Int_BQ76952_BufferChecksum(address, data, len);
    meta[1] = (uint8_t)(len + BQ76952_TRANSFER_LENGTH_OVERHEAD);

    return Int_BQ76952_WriteDirect(BQ76952_TRANSFER_CHECKSUM, meta, 2u);
}

Int_BQ76952_StatusTypeDef Int_BQ76952_ReadDeviceNumber(uint16_t *device_number)
{
    uint8_t data[2];
    Int_BQ76952_StatusTypeDef ret;

    if (device_number == NULL)
    {
        return INT_BQ76952_ERROR_PARAM;
    }

    ret = Int_BQ76952_ReadSubcommand(BQ76952_SUBCMD_DEVICE_NUMBER, data, 2u);
    if (ret != INT_BQ76952_OK)
    {
        return ret;
    }

    *device_number = (uint16_t)(((uint16_t)data[1] << 8u) | data[0]);
    return INT_BQ76952_OK;
}

Int_BQ76952_StatusTypeDef Int_BQ76952_ReadBatteryStatus(uint16_t *status)
{
    uint8_t data[2];
    Int_BQ76952_StatusTypeDef ret;

    if (status == NULL)
    {
        return INT_BQ76952_ERROR_PARAM;
    }

    ret = Int_BQ76952_ReadDirect(BQ76952_CMD_BATTERY_STATUS, data, 2u);
    if (ret != INT_BQ76952_OK)
    {
        return ret;
    }

    *status = (uint16_t)(((uint16_t)data[1] << 8u) | data[0]);
    return INT_BQ76952_OK;
}

Int_BQ76952_StatusTypeDef Int_BQ76952_ReadAlarmStatus(uint16_t *status)
{
    uint8_t data[2];
    Int_BQ76952_StatusTypeDef ret;

    if (status == NULL)
    {
        return INT_BQ76952_ERROR_PARAM;
    }

    ret = Int_BQ76952_ReadDirect(BQ76952_CMD_ALARM_STATUS, data, 2u);
    if (ret != INT_BQ76952_OK)
    {
        return ret;
    }

    *status = (uint16_t)(((uint16_t)data[1] << 8u) | data[0]);
    return INT_BQ76952_OK;
}

Int_BQ76952_StatusTypeDef Int_BQ76952_ClearAlarmStatus(uint16_t mask)
{
    uint8_t data[2];

    data[0] = (uint8_t)(mask & 0xFFu);
    data[1] = (uint8_t)(mask >> 8u);

    return Int_BQ76952_WriteDirect(BQ76952_CMD_ALARM_STATUS, data, 2u);
}

Int_BQ76952_StatusTypeDef Int_BQ76952_ReadFetStatus(uint8_t *status)
{
    if (status == NULL)
    {
        return INT_BQ76952_ERROR_PARAM;
    }

    return Int_BQ76952_ReadDirect(BQ76952_CMD_FET_STATUS, status, 1u);
}

Int_BQ76952_StatusTypeDef Int_BQ76952_ReadCellVoltage(uint8_t cell_index, int16_t *cell_mv)
{
    uint8_t data[2];
    uint8_t command;
    Int_BQ76952_StatusTypeDef ret;

    if (cell_mv == NULL)
    {
        return INT_BQ76952_ERROR_PARAM;
    }

    if ((cell_index == 0u) || (cell_index > BQ76952_CELL_COUNT_MAX))
    {
        return INT_BQ76952_ERROR_PARAM;
    }

    command = (uint8_t)(BQ76952_CMD_CELL1_VOLTAGE + ((cell_index - 1u) * 2u));

    ret = Int_BQ76952_ReadDirect(command, data, 2u);
    if (ret != INT_BQ76952_OK)
    {
        return ret;
    }

    *cell_mv = (int16_t)(((uint16_t)data[1] << 8u) | data[0]);
    return INT_BQ76952_OK;
}

Int_BQ76952_StatusTypeDef Int_BQ76952_EnterConfigUpdate(void)
{
    Int_BQ76952_StatusTypeDef ret;

    ret = Int_BQ76952_SendSubcommand(BQ76952_SUBCMD_SET_CFGUPDATE);
    if (ret != INT_BQ76952_OK)
    {
        return ret;
    }

    for (uint16_t poll = 0u; poll < INT_BQ76952_CFG_POLL_COUNT; poll++)
    {
        uint16_t status;

        ret = Int_BQ76952_ReadBatteryStatus(&status);
        if (ret != INT_BQ76952_OK)
        {
            return ret;
        }

        if ((status & BQ76952_BATTERY_STATUS_CFGUPDATE_MASK) != 0u)
        {
            return INT_BQ76952_OK;
        }

        HAL_Delay(INT_BQ76952_POLL_DELAY_MS);
    }

    return INT_BQ76952_ERROR_TIMEOUT;
}

Int_BQ76952_StatusTypeDef Int_BQ76952_ExitConfigUpdate(void)
{
    Int_BQ76952_StatusTypeDef ret;

    ret = Int_BQ76952_SendSubcommand(BQ76952_SUBCMD_EXIT_CFGUPDATE);
    if (ret != INT_BQ76952_OK)
    {
        return ret;
    }

    for (uint16_t poll = 0u; poll < INT_BQ76952_CFG_POLL_COUNT; poll++)
    {
        uint16_t status;

        ret = Int_BQ76952_ReadBatteryStatus(&status);
        if (ret != INT_BQ76952_OK)
        {
            return ret;
        }

        if ((status & BQ76952_BATTERY_STATUS_CFGUPDATE_MASK) == 0u)
        {
            return INT_BQ76952_OK;
        }

        HAL_Delay(INT_BQ76952_POLL_DELAY_MS);
    }

    return INT_BQ76952_ERROR_TIMEOUT;
}
