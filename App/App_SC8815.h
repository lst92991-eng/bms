#ifndef APP_SC8815_H
#define APP_SC8815_H

#include <stdbool.h>
#include <stdint.h>

/*
 * SC8815 监控快照。
 * `standby=true` 表示 PSTOP 有效、充电环路停止。
 * `charge_requested=true` 表示状态允许时 APP 可释放 PSTOP。
 */
typedef struct
{
    bool comm_ok;
    bool charge_requested;
    bool chip_enabled;
    bool standby;
    bool ac_ok;
    bool indet;
    bool vbus_short;
    bool otp;
    bool eoc;
    uint8_t status_raw;
    uint32_t vbus_mv;
    uint32_t vbat_mv;
    uint32_t ibus_ma;
    uint32_t ibat_ma;
} App_SC8815_StateTypeDef;

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

/**
 * @brief 获取最新 SC8815 状态快照。
 * @return 只读状态指针。
 */
const App_SC8815_StateTypeDef *App_SC8815_GetState(void);

#endif /* APP_SC8815_H */
