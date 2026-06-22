#ifndef INT_BQ76952_H
#define INT_BQ76952_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    INT_BQ76952_OK = 0,
    INT_BQ76952_ERROR,
    INT_BQ76952_ERROR_PARAM,
    INT_BQ76952_ERROR_HAL,
    INT_BQ76952_ERROR_CRC,
    INT_BQ76952_ERROR_CHECKSUM,
    INT_BQ76952_ERROR_TIMEOUT,
    INT_BQ76952_ERROR_LENGTH
} Int_BQ76952_StatusTypeDef;

/**
 * @brief 设置 I2C CRC 校验开关。
 * @note 只改变本驱动的通信格式假设，不会写 BQ76952 Comm Type。
 */
void Int_BQ76952_SetCrcEnabled(bool enabled);

/**
 * @brief 读取当前驱动是否按 I2C CRC 模式收发。
 */
bool Int_BQ76952_IsCrcEnabled(void);

/**
 * @brief 读取 direct command。
 * @param command BQ76952 direct command 地址。
 * @param data 输出数据缓冲区。
 * @param len 读取字节数。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_ReadDirect(uint8_t command, uint8_t *data, uint8_t len);

/**
 * @brief 写 direct command。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_WriteDirect(uint8_t command, const uint8_t *data, uint8_t len);

/**
 * @brief 发送 command-only subcommand。
 * @note 只写 0x3E/0x3F，不强制等待 echo；读回型 subcommand 请用 ReadSubcommand。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_SendSubcommand(uint16_t subcommand);

/**
 * @brief 读取带返回数据的 subcommand。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_ReadSubcommand(uint16_t subcommand, uint8_t *data, uint8_t len);

/**
 * @brief 读取 Data Memory。
 * @note 只做通信动作，不自动进入或退出 CONFIG_UPDATE。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_ReadDataMemory(uint16_t address, uint8_t *data, uint8_t len);

/**
 * @brief 写 Data Memory。
 * @note 调用前应由上层确认设备已进入 CONFIG_UPDATE。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_WriteDataMemory(uint16_t address, const uint8_t *data, uint8_t len);

/**
 * @brief 读取器件编号，用于 bring-up 确认芯片响应。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_ReadDeviceNumber(uint16_t *device_number);

/**
 * @brief 读取 Battery Status direct command。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_ReadBatteryStatus(uint16_t *status);

/**
 * @brief 读取 Alarm Status 锁存状态。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_ReadAlarmStatus(uint16_t *status);

/**
 * @brief 按 mask 清除 Alarm Status 锁存位。
 * @note 写 1 清除，调用前应先记录告警原因。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_ClearAlarmStatus(uint16_t mask);

/**
 * @brief 读取 FET Status direct command。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_ReadFetStatus(uint8_t *status);

/**
 * @brief 读取指定 cell direct command 电压。
 * @param cell_index 1-16，当前 6S 实际映射由硬件规则另行确认。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_ReadCellVoltage(uint8_t cell_index, int16_t *cell_mv);

/**
 * @brief 发送 SET_CFGUPDATE 并轮询 Battery Status[CFGUPDATE]。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_EnterConfigUpdate(void);

/**
 * @brief 发送 EXIT_CFGUPDATE 并轮询 Battery Status[CFGUPDATE] 清零。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_ExitConfigUpdate(void);

#endif /* INT_BQ76952_H */
