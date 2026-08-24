#ifndef INT_WATCHDOG_H
#define INT_WATCHDOG_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 启动独立看门狗。
 *
 * 该接口直接配置 CMSIS 寄存器，不依赖 CubeMX 生成 IWDG 句柄。
 * @param timeout_ms 基于 32 kHz 标称 LSI 计算的超时时间。
 * @return true 配置成功；false 参数越界或寄存器更新超时。
 */
bool Int_Watchdog_Start(uint32_t timeout_ms);

/** @brief 仅由安全监督任务调用，重新装载 IWDG 计数器。 */
void Int_Watchdog_Refresh(void);

bool Int_Watchdog_IsStarted(void);
uint32_t Int_Watchdog_GetConfiguredTimeoutMs(void);

#endif /* INT_WATCHDOG_H */
