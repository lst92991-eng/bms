/**
 * @file Int_BQ76952.c
 * @brief BQ76952 的 I2C 协议访问层。
 *
 * 本文件负责把 BQ76952 direct command、subcommand 和 Data Memory 间接访问
 * 封装为统一状态码，并处理可选的 I2C CRC、transfer buffer checksum、长度和
 * 超时校验。驱动固定使用 hi2c1 和 BSP 中的默认地址，不负责配置保护阈值、
 * FET 策略或自动切换 BQ 的 Comm Type。
 *
 * 所有访问均为阻塞式 HAL I2C 调用，部分流程还包含 HAL_Delay 和 printf，不能
 * 在 ISR 中调用。模块内部没有互斥保护；多个任务使用时必须由上层串行化，避免
 * 同时占用 I2C1 或交叉覆盖 BQ 的 0x3E~0x61 间接访问窗口。
 */
#include "Int_BQ76952.h"

#include <stddef.h>
#include <stdio.h>

#include "Int_BQ76952_BSP.h"
#include "i2c.h"

enum
{
    /* 单次 HAL I2C 事务的最长阻塞时间，不包含下方协议轮询时间。 */
    INT_BQ76952_I2C_TIMEOUT_MS = 100u,
    /* Echo/ConfigUpdate 最多轮询约 100 ms，防止 BQ 异常时永久阻塞任务。 */
    INT_BQ76952_ECHO_POLL_COUNT = 100u,
    INT_BQ76952_CFG_POLL_COUNT = 100u,
    INT_BQ76952_POLL_DELAY_MS = 1u,
    /* 覆盖 TRM 给出的典型命令执行时间，并给下一次 0x3E 访问留出裕量。 */
    INT_BQ76952_SUBCMD_RESPONSE_DELAY_MS = 2u,
    /* direct command 与 32-byte transfer window 使用不同的协议长度上限。 */
    INT_BQ76952_DIRECT_MAX_LEN = 34u,
    INT_BQ76952_TRANSFER_MAX_LEN = BQ76952_TRANSFER_BUFFER_SIZE
};

/*
 * 这是 MCU 侧的协议镜像，不会改写 BQ 的 Comm Type。主从设置不一致时，
 * non-CRC 与 CRC 帧长度不同，后续所有访问都会失败。
 */
static bool s_bq76952_crc_enabled = false;
/* 仅保存最近一次 HAL I2C 失败原因，成功事务不会主动清零。 */
static uint32_t s_bq76952_last_hal_error = 0u;

/**
 * @brief 快照保存 I2C1 最近一次 HAL 错误位。
 *
 * 必须紧跟在失败的 HAL 调用后执行，避免后续 I2C 操作改变句柄中的 ErrorCode。
 */
static void Int_BQ76952_RecordHalError(void)
{
    s_bq76952_last_hal_error = HAL_I2C_GetError(&hi2c1);
}

/**
 * @brief 将一个字节并入 BQ76952 CRC-8 累计值。
 *
 * 算法按 MSB-first 处理，使用 BSP 中定义的 0x07 多项式；不做最终异或。
 *
 * @param crc 前一字节处理后的 CRC 值。
 * @param data 本次并入的协议字节。
 * @return 更新后的 CRC 值。
 */
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

/**
 * @brief 从 BQ76952 规定的初值开始计算一段数据的 CRC-8。
 *
 * @param data 参与 CRC 的连续字节。
 * @param len 字节数。
 * @return CRC-8 校验值。
 */
static uint8_t Int_BQ76952_Crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = BQ76952_CRC8_INIT;

    for (uint8_t i = 0u; i < len; i++)
    {
        crc = Int_BQ76952_Crc8Update(crc, data[i]);
    }

    return crc;
}

/**
 * @brief 计算 0x3E~0x61 间接访问窗口使用的反码和校验。
 *
 * 该 checksum 覆盖 16-bit subcommand/Data Memory 地址和 payload，和每个
 * I2C 数据字节后附带的 CRC-8 是两套独立机制，即使 non-CRC 通信也必须计算。
 * 地址按 BQ 协议规定以低字节在前参与累加。
 *
 * @param command_or_address subcommand 或 Data Memory 的 16-bit 地址。
 * @param data transfer buffer 中的有效 payload。
 * @param len payload 字节数。
 * @return 写入/比对 0x60 的 checksum。
 */
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

/**
 * @brief 校验间接访问 payload 是否能放入 BQ 的 32-byte transfer buffer。
 */
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

/**
 * @brief 等待 BQ 在 0x3E/0x3F 回显本次 subcommand 或 Data Memory 地址。
 *
 * 回显采用 little-endian。只有地址完全匹配才允许继续读取 length、buffer 和
 * checksum，防止把上一条尚未完成的间接访问结果误当成本次响应。
 *
 * @return 回显匹配、底层 I2C 错误或有限轮询超时。
 */
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

/**
 * @brief 从 BQ 间接访问窗口读取并校验 subcommand/Data Memory 响应。
 *
 * 顺序必须保持为：确认 0x3E echo -> 读取 0x61 length -> 读取 0x40 buffer ->
 * 读取 0x60 checksum。驱动先把完整响应读入局部缓冲并完成长度、checksum 校验，
 * 成功后才复制给调用者，避免失败时返回半帧数据。
 *
 * @param command_or_address 本次请求的 subcommand 或 Data Memory 地址。
 * @param data 调用者输出缓冲区；公开入口已负责非空检查。
 * @param len 调用者需要的 payload 字节数。
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

    /* 0x61 包含地址、checksum 和 length 自身的 4-byte 协议开销。 */
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

/**
 * @brief 读取 Battery Status[CFGUPDATE] 的稳定状态。
 *
 * Battery Status 为 little-endian 16-bit direct command；进入/退出配置模式必须
 * 以该状态位为准，不能只依赖发送 SET/EXIT_CFGUPDATE 的 I2C ACK。
 */
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

/**
 * @brief 通过 0x3E~0x61 窗口提交带 payload 的 subcommand/Data Memory 写入。
 *
 * 第一笔事务把 16-bit 地址和 payload 写入 0x3E 起始窗口；第二笔事务向 0x60
 * 写 checksum 和总长度，后者才触发 BQ 校验并提交整帧。两笔事务之间不能被
 * 其他任务的 BQ 间接访问插入。
 */
static Int_BQ76952_StatusTypeDef Int_BQ76952_WriteTransfer(uint16_t command_or_address,
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

    transfer[0] = (uint8_t)(command_or_address & 0xFFu);
    transfer[1] = (uint8_t)(command_or_address >> 8u);

    for (uint8_t i = 0u; i < len; i++)
    {
        transfer[2u + i] = data[i];
    }

    ret = Int_BQ76952_WriteDirect(BQ76952_SUBCMD_ADDR_LSB,
                                  transfer,
                                  (uint8_t)(len + 2u));
    if (ret != INT_BQ76952_OK)
    {
        return ret;
    }

    meta[0] = Int_BQ76952_BufferChecksum(command_or_address, data, len);
    meta[1] = (uint8_t)(len + BQ76952_TRANSFER_LENGTH_OVERHEAD);

    return Int_BQ76952_WriteDirect(BQ76952_TRANSFER_CHECKSUM, meta, 2u);
}

/**
 * @brief 初始化驱动侧的板级通信假设。
 *
 * 这里只同步 BSP 默认 CRC 状态，不访问总线，也不会修改 BQ 的 Comm Type、
 * I2C 地址、保护参数或 FET 状态。
 */
void Int_BQ76952_InitBoard(void)
{
    s_bq76952_crc_enabled = (BQ76952_I2C_CRC_DEFAULT_ENABLED != 0u);
}

/**
 * @brief 发送 BQ76952 RESET 子命令。
 *
 * RESET 后的稳定等待和重新配置由 APP 层负责；本函数不隐式重写 Data Memory。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_Reset(void)
{
    return Int_BQ76952_SendSubcommand(BQ76952_SUBCMD_RESET);
}

/**
 * @brief 发送 BQ76952 SHUTDOWN 子命令。
 *
 * 本层只完成协议发送。调用前关闭主功率 FET、保存状态以及关机失败处置均属于
 * APP 电源状态机的安全责任。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_Shutdown(void)
{
    return Int_BQ76952_SendSubcommand(BQ76952_SUBCMD_SHUTDOWN);
}

/**
 * @brief 切换 MCU 驱动解析 I2C CRC 帧的方式。
 *
 * 该接口不会同步写入 BQ 的 Comm Type，必须在第一条 BQ 命令前调用，并确保
 * 芯片侧设置相同，否则帧长度和CRC字节位置会失配。
 */
void Int_BQ76952_SetCrcEnabled(bool enabled)
{
    s_bq76952_crc_enabled = enabled;
}

/**
 * @brief 返回 MCU 当前采用的 BQ I2C CRC 解析模式。
 */
bool Int_BQ76952_IsCrcEnabled(void)
{
    return s_bq76952_crc_enabled;
}

/**
 * @brief 返回最近一次失败的 HAL I2C ErrorCode 快照。
 *
 * 该值用于故障诊断，不代表当前通信仍然失败；成功事务不会清零历史错误。
 */
uint32_t Int_BQ76952_GetLastHalError(void)
{
    return s_bq76952_last_hal_error;
}

/**
 * @brief 读取 BQ76952 的 8-bit direct command 地址空间。
 *
 * non-CRC 模式直接使用 HAL Mem_Read。CRC 模式下，每个数据字节后紧跟一个
 * CRC 字节；首字节 CRC 覆盖写地址、command、读地址和数据，后续 CRC 仅覆盖
 * 对应数据字节。HAL 的设备地址参数使用已左移的 8-bit 写地址 0x10。
 *
 * @param command direct command 地址。
 * @param data 输出数据缓冲区。
 * @param len 期望读取的数据字节数，不包含 CRC 字节。
 */
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
        if (HAL_I2C_Mem_Read(&hi2c1,
                             BQ76952_I2C_8BIT_WRITE_ADDR_DEFAULT,
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

        if (HAL_I2C_Mem_Read(&hi2c1,
                             BQ76952_I2C_8BIT_WRITE_ADDR_DEFAULT,
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

            /*
             * 首字节 CRC 绑定完整的寻址阶段；连续读取的后续字节各自独立校验，
             * 因此不能对整段 rx 一次性计算 CRC。
             */
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
                /* 故障日志只保留前 8 个原始字节，避免通信异常时刷屏。 */
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

/**
 * @brief 写入 BQ76952 的 8-bit direct command 地址空间。
 *
 * non-CRC 模式使用 HAL Mem_Write。CRC 模式必须用 Master_Transmit 手工构造
 * command + data/CRC 交错帧；首个 CRC 覆盖设备写地址、command 和首字节数据，
 * 后续 CRC 分别只覆盖对应数据字节。
 *
 * @param command direct command 地址。
 * @param data 待写数据。
 * @param len 数据字节数，不包含 command 和 CRC 字节。
 */
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
        if (HAL_I2C_Mem_Write(&hi2c1,
                              BQ76952_I2C_8BIT_WRITE_ADDR_DEFAULT,
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

        if (HAL_I2C_Master_Transmit(&hi2c1,
                                    BQ76952_I2C_8BIT_WRITE_ADDR_DEFAULT,
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

/**
 * @brief 发送不带 payload 的 command-only subcommand。
 *
 * subcommand 以 little-endian 写入 0x3E/0x3F。写成功后等待 2 ms，避免 BQ 尚在
 * 执行上一命令时又收到新的间接窗口访问；需要返回数据的命令应走 ReadSubcommand。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_SendSubcommand(uint16_t subcommand)
{
    uint8_t data[2];
    Int_BQ76952_StatusTypeDef ret;

    data[0] = (uint8_t)(subcommand & 0xFFu);
    data[1] = (uint8_t)(subcommand >> 8u);

    ret = Int_BQ76952_WriteDirect(BQ76952_SUBCMD_ADDR_LSB, data, 2u);
    if (ret == INT_BQ76952_OK)
    {
        /* TRM 命令执行时间约 0.5 ms；完成前不能紧接下一条 0x3E/0x3F 命令。 */
        HAL_Delay(INT_BQ76952_SUBCMD_RESPONSE_DELAY_MS);
    }

    return ret;
}

/**
 * @brief 发起 subcommand 并从 transfer buffer 读取返回数据。
 *
 * 先向 0x3E/0x3F 写 little-endian subcommand，再等待 BQ 生成响应；ReadTransfer
 * 会继续确认 echo、长度和 checksum，任一环节失败都不会返回未经校验的数据。
 */
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

/**
 * @brief 提交带 payload 的 subcommand。
 *
 * WriteTransfer 完成 checksum/length 提交后仍需留出命令执行时间，确保返回给
 * 调用者时可以安全开始下一笔 BQ 命令。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_WriteSubcommandData(uint16_t subcommand,
                                                          const uint8_t *data,
                                                          uint8_t len)
{
    Int_BQ76952_StatusTypeDef ret = Int_BQ76952_WriteTransfer(subcommand, data, len);

    if (ret == INT_BQ76952_OK)
    {
        /* 等待 BQ 提交带数据子命令，保证调用者返回后可安全发下一条命令。 */
        HAL_Delay(INT_BQ76952_SUBCMD_RESPONSE_DELAY_MS);
    }

    return ret;
}

/**
 * @brief 读取 BQ76952 Data Memory。
 *
 * Data Memory 地址以 little-endian 写入 0x3E/0x3F，再从间接窗口读取校验后的
 * payload。读取不要求进入 ConfigUpdate，但调用者仍应避免和其他窗口访问并发。
 */
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

/**
 * @brief 写入 BQ76952 Data Memory。
 *
 * 本函数只执行协议写入，不自动进入或退出 ConfigUpdate。需要 ConfigUpdate 的
 * 配置项必须由上层先确认进入成功，并在全部写入结束后显式退出。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_WriteDataMemory(uint16_t address, const uint8_t *data, uint8_t len)
{
    return Int_BQ76952_WriteTransfer(address, data, len);
}

/**
 * @brief 写入 host 控制的主动均衡 cell mask。
 *
 * mask 位号对应 BQ 的 Cell1~Cell16，而不是物理电池包中的连续序号。本项目 6S
 * 为稀疏接线，调用者必须使用 BSP 已确认的映射，错误位可能均衡到非目标通道。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_SetBalanceMask(uint16_t mask)
{
    uint8_t data[2];

    data[0] = (uint8_t)(mask & 0xFFu);
    data[1] = (uint8_t)(mask >> 8u);

    return Int_BQ76952_WriteSubcommandData(BQ76952_SUBCMD_CB_ACTIVE_CELLS,
                                           data,
                                           2u);
}

/**
 * @brief 进入 ConfigUpdate 并确认 Battery Status[CFGUPDATE] 已置位。
 *
 * SET_CFGUPDATE 的 I2C ACK 只表示命令已接收；轮询状态位成功后，上层才可以开始
 * 写需要配置模式的 Data Memory。有限超时用于避免 BQ 异常时卡死电池管理任务。
 */
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

/**
 * @brief 退出 ConfigUpdate 并确认 Battery Status[CFGUPDATE] 已清零。
 *
 * 只有状态位清零后才认为 BQ 已恢复正常运行模式；若 I2C 失败或超时，调用者
 * 必须把配置结果视为不确定状态，不能继续默认保护和 FET 配置已经生效。
 */
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
