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
 * @brief BQ76952 板级/通信初始化。
 *
 * 只做本项目默认的硬件假设初始化：
 * - 打开 BQ I2C 的 CRC 模式；
 * - 释放唤醒脚到安全初值；
 * - 不写任何业务寄存器。
 */
void Int_BQ76952_InitBoard(void);

/**
 * @brief 拉起 BQ76952 唤醒脚。
 *
 * 当前 bring-up 阶段 PB3 保持浮空输入，不主动驱动 BMS_WAKE。
 * 如后续恢复 MCU 唤醒控制，再在 BSP/GPIO 中重新打开驱动。
 */
void Int_BQ76952_WakeUp(void);

/**
 * @brief 读取 BMS_INT / ALERT 现状。
 * @return true 表示告警脚被拉低，也就是中断/告警处于有效态。
 */
bool Int_BQ76952_IsAlertAsserted(void);

/**
 * @brief 复位 BQ76952。
 *
 * 这里是“协议复位”，不是 MCU 复位。
 * 先按板级时序唤醒，再发送 RESET 子命令。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_Reset(void);

/**
 * @brief 发送 BQ76952 SHUTDOWN 子命令。
 *
 * 这是危险命令，只保留给明确需要进入关断序列的场景。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_Shutdown(void);

/**
 * @brief 设置 I2C CRC 镜像开关。
 * @note 只改变本驱动的通信假设，不会改写 BQ76952 Comm Type。
 */
void Int_BQ76952_SetCrcEnabled(bool enabled);

/**
 * @brief 读取当前驱动是否按 I2C CRC 模式收发。
 */
bool Int_BQ76952_IsCrcEnabled(void);
Int_BQ76952_StatusTypeDef Int_BQ76952_ProbeDevice(uint32_t *hal_error);
uint32_t Int_BQ76952_GetLastHalError(void);

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
 * @note 只写 0x3E/0x3F，不强制等待 echo；读回型 subcommand 走 ReadSubcommand。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_SendSubcommand(uint16_t subcommand);

/**
 * @brief 读取带回读数据的 subcommand。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_ReadSubcommand(uint16_t subcommand, uint8_t *data, uint8_t len);

/**
 * @brief 写带 data 的 subcommand。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_WriteSubcommandData(uint16_t subcommand,
                                                          const uint8_t *data,
                                                          uint8_t len);

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
 * @brief 发送 SET_CFGUPDATE 并轮询 Battery Status[CFGUPDATE]。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_EnterConfigUpdate(void);

/**
 * @brief 发送 EXIT_CFGUPDATE 并轮询 Battery Status[CFGUPDATE] 清零。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_ExitConfigUpdate(void);

#ifdef INT_BQ76952_ENABLE_BRINGUP_API
/**
 * @brief 读取器件编号，用于 bring-up 确认芯片响应。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_ReadDeviceNumber(uint16_t *device_number);
#endif

#endif /* INT_BQ76952_H */
