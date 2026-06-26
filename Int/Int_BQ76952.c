#include "Int_BQ76952.h"

#include <stddef.h>
#include <stdio.h>

#include "Int_BQ76952_BSP.h"
#include "i2c.h"
#include "main.h"

#ifndef INT_BQ76952_I2C_HANDLE
#define INT_BQ76952_I2C_HANDLE hi2c1
#endif

#ifndef INT_BQ76952_WAKE_GPIO_PORT
#define INT_BQ76952_WAKE_GPIO_PORT GPIOB
#endif

#ifndef INT_BQ76952_WAKE_PIN
#define INT_BQ76952_WAKE_PIN GPIO_PIN_3
#endif

#ifndef INT_BQ76952_ALERT_GPIO_PORT
#define INT_BQ76952_ALERT_GPIO_PORT GPIOB
#endif

#ifndef INT_BQ76952_ALERT_PIN
#define INT_BQ76952_ALERT_PIN GPIO_PIN_4
#endif

#ifndef INT_BQ76952_WAKE_PULSE_MS
#define INT_BQ76952_WAKE_PULSE_MS (2u)
#endif

#ifndef INT_BQ76952_WAKE_SETTLE_MS
#define INT_BQ76952_WAKE_SETTLE_MS (10u)
#endif

#define INT_BQ76952_I2C_ADDR             BQ76952_I2C_8BIT_WRITE_ADDR_DEFAULT
#define INT_BQ76952_I2C_TIMEOUT_MS       (100u)
#define INT_BQ76952_ECHO_POLL_COUNT      (100u)
#define INT_BQ76952_CFG_POLL_COUNT       (100u)
#define INT_BQ76952_POLL_DELAY_MS        (1u)
#define INT_BQ76952_SUBCMD_RESPONSE_DELAY_MS (2u)
#define INT_BQ76952_DIRECT_MAX_LEN       (34u)
#define INT_BQ76952_TRANSFER_MAX_LEN     BQ76952_TRANSFER_BUFFER_SIZE

extern I2C_HandleTypeDef INT_BQ76952_I2C_HANDLE;

static bool s_bq76952_crc_enabled = false;
static uint32_t s_bq76952_last_hal_error = 0u;

static void Int_BQ76952_RecordHalError(void)
{
    s_bq76952_last_hal_error = HAL_I2C_GetError(&INT_BQ76952_I2C_HANDLE);
}

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
        printf("bq transfer echo fail cmd:0x%04x ret:%d\r\n",
               (unsigned int)command_or_address,
               (int)ret);
        return ret;
    }

    ret = Int_BQ76952_ReadDirect(BQ76952_TRANSFER_LENGTH, &length, 1u);
    if (ret != INT_BQ76952_OK)
    {
        printf("bq transfer length fail cmd:0x%04x ret:%d\r\n",
               (unsigned int)command_or_address,
               (int)ret);
        return ret;
    }

    if (length < BQ76952_TRANSFER_LENGTH_OVERHEAD)
    {
        printf("bq transfer length short cmd:0x%04x len:%u\r\n",
               (unsigned int)command_or_address,
               (unsigned int)length);
        return INT_BQ76952_ERROR_LENGTH;
    }

    data_len = (uint8_t)(length - BQ76952_TRANSFER_LENGTH_OVERHEAD);
    if ((data_len == 0u) || (data_len > INT_BQ76952_TRANSFER_MAX_LEN) || (len > data_len))
    {
        printf("bq transfer data length bad cmd:0x%04x len:%u data_len:%u want:%u\r\n",
               (unsigned int)command_or_address,
               (unsigned int)length,
               (unsigned int)data_len,
               (unsigned int)len);
        return INT_BQ76952_ERROR_LENGTH;
    }

    ret = Int_BQ76952_ReadDirect(BQ76952_TRANSFER_BUFFER_START, raw, data_len);
    if (ret != INT_BQ76952_OK)
    {
        printf("bq transfer buffer fail cmd:0x%04x ret:%d\r\n",
               (unsigned int)command_or_address,
               (int)ret);
        return ret;
    }

    ret = Int_BQ76952_ReadDirect(BQ76952_TRANSFER_CHECKSUM, &checksum, 1u);
    if (ret != INT_BQ76952_OK)
    {
        printf("bq transfer checksum read fail cmd:0x%04x ret:%d\r\n",
               (unsigned int)command_or_address,
               (int)ret);
        return ret;
    }

    if (Int_BQ76952_BufferChecksum(command_or_address, raw, data_len) != checksum)
    {
        printf("bq transfer checksum bad cmd:0x%04x got:0x%02x exp:0x%02x len:%u\r\n",
               (unsigned int)command_or_address,
               (unsigned int)checksum,
               (unsigned int)Int_BQ76952_BufferChecksum(command_or_address, raw, data_len),
               (unsigned int)data_len);
        return INT_BQ76952_ERROR_CHECKSUM;
    }

    for (uint8_t i = 0u; i < len; i++)
    {
        data[i] = raw[i];
    }

    return INT_BQ76952_OK;
}

static Int_BQ76952_StatusTypeDef Int_BQ76952_ReadCfgUpdateBit(bool *is_set)
{
    uint8_t data[2];
    uint16_t status;
    Int_BQ76952_StatusTypeDef ret;

    if (is_set == NULL)
    {
        return INT_BQ76952_ERROR_PARAM;
    }

    ret = Int_BQ76952_ReadDirect(BQ76952_CMD_BATTERY_STATUS, data, 2u);
    if (ret != INT_BQ76952_OK)
    {
        return ret;
    }

    status = (uint16_t)(((uint16_t)data[1] << 8u) | data[0]);
    *is_set = ((status & BQ76952_BATTERY_STATUS_CFGUPDATE_MASK) != 0u);

    return INT_BQ76952_OK;
}

void Int_BQ76952_InitBoard(void)
{
    s_bq76952_crc_enabled = (BQ76952_I2C_CRC_DEFAULT_ENABLED != 0u);
}

void Int_BQ76952_WakeUp(void)
{
    /*
     * PB3 当前按硬件排查要求保持浮空输入，不主动驱动 BMS_WAKE。
     * 这里仅保留等待窗口，避免上层流程依赖唤醒函数时序时行为突变。
     */
    HAL_Delay(INT_BQ76952_WAKE_SETTLE_MS);
}

bool Int_BQ76952_IsAlertAsserted(void)
{
    return HAL_GPIO_ReadPin(INT_BQ76952_ALERT_GPIO_PORT, INT_BQ76952_ALERT_PIN) == GPIO_PIN_RESET;
}

Int_BQ76952_StatusTypeDef Int_BQ76952_Reset(void)
{
    Int_BQ76952_WakeUp();
    return Int_BQ76952_SendSubcommand(BQ76952_SUBCMD_RESET);
}

Int_BQ76952_StatusTypeDef Int_BQ76952_Shutdown(void)
{
    return Int_BQ76952_SendSubcommand(BQ76952_SUBCMD_SHUTDOWN);
}

void Int_BQ76952_SetCrcEnabled(bool enabled)
{
    s_bq76952_crc_enabled = enabled;
}

bool Int_BQ76952_IsCrcEnabled(void)
{
    return s_bq76952_crc_enabled;
}

Int_BQ76952_StatusTypeDef Int_BQ76952_ProbeDevice(uint32_t *hal_error)
{
    HAL_StatusTypeDef hal_status;

    s_bq76952_last_hal_error = 0u;
    hal_status = HAL_I2C_IsDeviceReady(&INT_BQ76952_I2C_HANDLE,
                                       INT_BQ76952_I2C_ADDR,
                                       3u,
                                       INT_BQ76952_I2C_TIMEOUT_MS);
    if (hal_status != HAL_OK)
    {
        Int_BQ76952_RecordHalError();
        if (hal_error != NULL)
        {
            *hal_error = s_bq76952_last_hal_error;
        }
        return INT_BQ76952_ERROR_HAL;
    }

    if (hal_error != NULL)
    {
        *hal_error = 0u;
    }

    return INT_BQ76952_OK;
}

uint32_t Int_BQ76952_GetLastHalError(void)
{
    return s_bq76952_last_hal_error;
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
            Int_BQ76952_RecordHalError();
            return INT_BQ76952_ERROR_HAL;
        }

        return INT_BQ76952_OK;
    }

    {
        uint8_t rx[INT_BQ76952_DIRECT_MAX_LEN * 2u];

        if (HAL_I2C_Mem_Read(&INT_BQ76952_I2C_HANDLE,
                             INT_BQ76952_I2C_ADDR,
                             command,
                             I2C_MEMADD_SIZE_8BIT,
                             rx,
                             (uint16_t)(len * 2u),
                             INT_BQ76952_I2C_TIMEOUT_MS) != HAL_OK)
        {
            Int_BQ76952_RecordHalError();
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
                uint8_t raw_len = (uint8_t)(len * 2u);
                if (raw_len > 8u)
                {
                    raw_len = 8u;
                }

                printf("bq crc fail cmd:0x%02x idx:%u data:0x%02x got:0x%02x exp:0x%02x raw:",
                       (unsigned int)command,
                       (unsigned int)i,
                       (unsigned int)rx[i * 2u],
                       (unsigned int)rx[(i * 2u) + 1u],
                       (unsigned int)crc);
                for (uint8_t j = 0u; j < raw_len; j++)
                {
                    printf("%02x", (unsigned int)rx[j]);
                }
                printf("\r\n");
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
            Int_BQ76952_RecordHalError();
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
            Int_BQ76952_RecordHalError();
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
    HAL_Delay(INT_BQ76952_SUBCMD_RESPONSE_DELAY_MS);

    return Int_BQ76952_ReadTransfer(subcommand, data, len);
}

Int_BQ76952_StatusTypeDef Int_BQ76952_WriteSubcommandData(uint16_t subcommand,
                                                          const uint8_t *data,
                                                          uint8_t len)
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

    transfer[0] = (uint8_t)(subcommand & 0xFFu);
    transfer[1] = (uint8_t)(subcommand >> 8u);

    for (uint8_t i = 0u; i < len; i++)
    {
        transfer[2u + i] = data[i];
    }

    ret = Int_BQ76952_WriteDirect(BQ76952_SUBCMD_ADDR_LSB, transfer, (uint8_t)(len + 2u));
    if (ret != INT_BQ76952_OK)
    {
        return ret;
    }

    meta[0] = Int_BQ76952_BufferChecksum(subcommand, data, len);
    meta[1] = (uint8_t)(len + BQ76952_TRANSFER_LENGTH_OVERHEAD);

    return Int_BQ76952_WriteDirect(BQ76952_TRANSFER_CHECKSUM, meta, 2u);
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
    HAL_Delay(INT_BQ76952_SUBCMD_RESPONSE_DELAY_MS);

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

    meta[0] = Int_BQ76952_BufferChecksum(address, data, len);
    meta[1] = (uint8_t)(len + BQ76952_TRANSFER_LENGTH_OVERHEAD);

    return Int_BQ76952_WriteDirect(BQ76952_TRANSFER_CHECKSUM, meta, 2u);
}

#ifdef INT_BQ76952_ENABLE_BRINGUP_API
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
#endif

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
        bool is_cfg_update;

        ret = Int_BQ76952_ReadCfgUpdateBit(&is_cfg_update);
        if (ret != INT_BQ76952_OK)
        {
            return ret;
        }

        if (is_cfg_update)
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
        bool is_cfg_update;

        ret = Int_BQ76952_ReadCfgUpdateBit(&is_cfg_update);
        if (ret != INT_BQ76952_OK)
        {
            return ret;
        }

        if (!is_cfg_update)
        {
            return INT_BQ76952_OK;
        }

        HAL_Delay(INT_BQ76952_POLL_DELAY_MS);
    }

    return INT_BQ76952_ERROR_TIMEOUT;
}
