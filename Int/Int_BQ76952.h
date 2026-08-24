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
 * - 同步当前实板已验证的 non-CRC I2C 模式；
 * - 不写任何业务寄存器。
 */
void Int_BQ76952_InitBoard(void);

/**
 * @brief 复位 BQ76952。
 *
 * 这里是“协议复位”，不是 MCU 复位。
 * 新硬件只保留硬件 wake，软件不再驱动或等待 wake 引脚。
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
uint32_t Int_BQ76952_GetLastHalError(void);

/**
 * @brief 在 BQ ALERT GPIO 回调中锁存待处理事件。
 * @note ISR 安全；只写内存，不允许在中断中发起 I2C。
 */
void Int_BQ76952_NotifyAlertFromISR(void);

/**
 * @brief 取出并清除一次 BQ ALERT 待处理事件。
 * @note 若处理期间可能再次进中断，应使用带序号的 AcknowledgeAlert。
 */
bool Int_BQ76952_TakeAlertPending(void);

/**
 * @brief 仅在 ALERT 序号未变化时确认已处理事件。
 * @param sequence 处理状态帧前取得的 ALERT 序号。
 * @return true 表示已清除对应 pending；false 表示期间又来了新事件。
 */
bool Int_BQ76952_AcknowledgeAlert(uint32_t sequence);

/**
 * @brief 查询 BQ ALERT 是否仍有待处理事件，不清除锁存。
 */
bool Int_BQ76952_IsAlertPending(void);

/**
 * @brief 返回自 MCU 启动以来锁存的 ALERT 次数，允许自然回卷。
 */
uint32_t Int_BQ76952_GetAlertSequence(void);

/**
 * @brief 建立一个跨多次公开访问的总线事务和统一绝对截止时间。
 * @note 只能在任务/调度前上下文调用；成功后必须成对调用 EndTransaction。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_BeginTransaction(uint32_t budget_ms);
void Int_BQ76952_EndTransaction(void);

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
Int_BQ76952_StatusTypeDef
Int_BQ76952_WriteDirect(uint8_t command, const uint8_t *data, uint8_t len);

/**
 * @brief 发送 command-only subcommand。
 * @note 只写 0x3E/0x3F，不强制等待 echo；读回型 subcommand 走 ReadSubcommand。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_SendSubcommand(uint16_t subcommand);

/**
 * @brief 读取带回读数据的 subcommand。
 */
Int_BQ76952_StatusTypeDef
Int_BQ76952_ReadSubcommand(uint16_t subcommand, uint8_t *data, uint8_t len);

/**
 * @brief 写带 data 的 subcommand。
 */
Int_BQ76952_StatusTypeDef
Int_BQ76952_WriteSubcommandData(uint16_t subcommand, const uint8_t *data, uint8_t len);

/**
 * @brief 读取 Data Memory。
 * @note 只做通信动作，不自动进入或退出 CONFIG_UPDATE。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_ReadDataMemory(uint16_t address, uint8_t *data, uint8_t len);

/**
 * @brief 写 Data Memory。
 * @note 调用前应由上层确认设备已进入 CONFIG_UPDATE。
 */
Int_BQ76952_StatusTypeDef
Int_BQ76952_WriteDataMemory(uint16_t address, const uint8_t *data, uint8_t len);

/**
 * @brief 发送 SET_CFGUPDATE 并轮询 Battery Status[CFGUPDATE]。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_EnterConfigUpdate(void);

/**
 * @brief 发送 EXIT_CFGUPDATE 并轮询 Battery Status[CFGUPDATE] 清零。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_ExitConfigUpdate(void);

/**
 * @brief 写 host 控制均衡 cell mask。
 *
 * mask 的 bit 与 BQ Cell1..Cell16 对应；本项目 6S 只能使用 BSP 中确认过的
 * BQ76952_CELL_MASK_6S_HW_CELLx，不能按物理 cell0..5 直接移位。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_SetBalanceMask(uint16_t mask);

/**
 * @brief 在一个完整的互斥事务中写 FET_CONTROL 并读取 FET_STATUS。
 * @param off_mask FET_CONTROL 的四路关断位完整镜像。
 * @param observed_status 成功时返回 FET_STATUS；失败时保持调用前内容。
 */
Int_BQ76952_StatusTypeDef Int_BQ76952_ApplyFetControl(uint8_t off_mask, uint8_t *observed_status);

#endif /* INT_BQ76952_H */
