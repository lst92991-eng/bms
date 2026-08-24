#ifndef APP_SC8815_H
#define APP_SC8815_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    bool comm_ok;
    bool desired_charge;
    bool commanded_charge;
    bool observed_standby;
    bool emergency_latched;
    bool chip_enabled;
    bool ac_ok;
    bool indet;
    bool vbus_short;
    bool otp;
    bool eoc;
    uint8_t status_raw;
    uint8_t last_error;
    uint32_t generation;
    uint32_t safety_authorization_epoch;
    uint32_t sample_age_ms;
    uint32_t vbus_mv;
    uint32_t vbat_mv;
    uint32_t ibus_ma;
    uint32_t ibat_ma;
    uint16_t vbus_raw;
    uint16_t vbat_raw;
    uint16_t ibus_raw;
    uint16_t ibat_raw;
    uint16_t desired_ibat_limit_ma;
    uint16_t commanded_ibat_limit_ma;
    uint16_t observed_ibat_limit_ma;
} App_SC8815_SnapshotTypeDef;

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
void App_SC8815_EmergencyStop(void);
void App_SC8815_EmergencyStopFromISR(void);
void App_SC8815_InvalidateAuthorization(void);
bool App_SC8815_RequestChargeAuthorized(uint32_t safety_authorization_epoch);
bool App_SC8815_SetChargeCurrentLimitMa(uint16_t current_ma);
void App_SC8815_GetSnapshot(App_SC8815_SnapshotTypeDef *snapshot);

bool App_SC8815_IsCommOk(void);
bool App_SC8815_IsAcOk(void);
bool App_SC8815_HasFault(void);
bool App_SC8815_IsCharging(void);
uint32_t App_SC8815_GetVbusMv(void);
uint32_t App_SC8815_GetVbatMv(void);
uint32_t App_SC8815_GetInputLimitMa(void);

#endif /* APP_SC8815_H */
