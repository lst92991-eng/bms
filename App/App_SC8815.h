#ifndef APP_SC8815_H
#define APP_SC8815_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 安全初始化 SC8815，只进入 standby monitor，不启动充电。
 */
void App_SC8815_Init(void);

/**
 * @brief 周期读取 SC8815 状态/ADC，并执行充电请求状态机。
 * @param interval_ms 调用周期，用于调试打印节拍。
 */
void App_SC8815_Task(uint16_t interval_ms);

/**
 * @brief 请求启动或停止充电。
 * @param enable true 请求充电，false 回到 standby monitor。
 */
void App_SC8815_RequestCharge(bool enable);

bool App_SC8815_IsCommOk(void);
bool App_SC8815_IsAcOk(void);
bool App_SC8815_HasFault(void);
bool App_SC8815_IsCharging(void);
uint32_t App_SC8815_GetVbusMv(void);
uint32_t App_SC8815_GetVbatMv(void);

#endif /* APP_SC8815_H */
