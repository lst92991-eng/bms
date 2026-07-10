#ifndef APP_BATMAN_H
#define APP_BATMAN_H

#include <stdbool.h>
#include <stdint.h>

/*
 * BQ APP 对外常量。
 * 只保留显示、CAN 或主机日志需要的值；内部调参项留在 App_BatMan.c。
 */
enum
{
    APP_BATMAN_CELL_COUNT = 6u,
    APP_BATMAN_CELL_FULL_MV = 4200u,
    APP_BATMAN_CELL_VALID_MIN_MV = 2500u,
    APP_BATMAN_CELL_VALID_MAX_MV = 4300u,
    APP_BATMAN_DEBUG_PERIOD_MS = 5000u,
    APP_BATMAN_CAPACITY_MAH = 5000u,
    APP_BATMAN_CC2_RAW_POLARITY = 1,
    APP_BATMAN_CC2_RAW_NUMERATOR = 1,
    APP_BATMAN_CC2_RAW_DENOMINATOR = 1,
    APP_BATMAN_STACK_RAW_TO_MV = 10u
};

#define APP_BATMAN_DEFAULT_SOC_PERCENT           (50.0f)

/**
 * @brief 初始化 BQ76952，写入基线 Data Memory，并让主 FET 进入受控默认关断态。
 */
void App_BatMan_Init(void);

/**
 * @brief BQ 周期监控任务。
 * @param interval_ms 调用周期，用于 SOC/SOH 积分。
 */
void App_BatMan_Task(uint32_t interval_ms);

/**
 * @brief 打印一帧完整 BQ 中文诊断快照。
 */
void App_BatMan_PrintSnapshot(void);

/**
 * @brief 打印一帧简短 BQ 监视摘要。
 */
void App_BatMan_PrintMonitor(void);

/**
 * @brief 打印一行 bqfast 专用的紧凑监视数据。
 */
void App_BatMan_PrintFastMonitor(void);

/**
 * @brief 判断当前 BQ/采样快照是否已经出现需要停表记录的异常。
 */
bool App_BatMan_IsMonitorFaultActive(void);

/**
 * @brief 判断最近一次 BQ 采样是否仍然在线。
 * @return true 表示本周期通信正常且电芯采样有效。
 */
bool App_BatMan_IsOnline(void);

/**
 * @brief 打印 bqfast 自动停表时的 BQ 侧原因。
 */
void App_BatMan_PrintMonitorStopReason(void);

/**
 * @brief 设置主充放电 MOS。
 * @param charge_enable true 允许 CHG/PCHG，false 强制关断充电路径。
 * @param discharge_enable true 允许 DSG/PDSG，false 强制关断放电路径。
 * @return true 写入成功。
 */
bool App_BatMan_SetMainFets(bool charge_enable, bool discharge_enable);
bool App_BatMan_RecoverAfterWake(void);

/**
 * @brief 关闭主 FET 后发送 BQ76952 SHUTDOWN 子命令。
 * @return true 表示主 FET 关断命令和 SHUTDOWN 子命令都已成功发出。
 */
bool App_BatMan_RequestShutdown(void);

/**
 * @brief 手动测试 PDSG：只允许 PDSG，强制关闭 CHG/PCHG/DSG，并释放 FET_INIT_OFF。
 * @return true 写入成功。
 */
bool App_BatMan_TestPreDischargeOnly(void);

/**
 * @brief 强制关闭 CHG/DSG/PCHG/PDSG。
 * @return true 写入成功。
 */
bool App_BatMan_AllMainFetsOff(void);

/* 最新 BQ 遥测快照，由主循环任务更新。 */
extern uint16_t cell_mv[APP_BATMAN_CELL_COUNT];
extern uint32_t stack_mv;
extern uint32_t pack_mv;
extern int32_t current_ma;
extern float current_a;
extern uint16_t cell_min_mv;
extern uint16_t cell_max_mv;
extern uint16_t cell_avg_mv;
extern uint16_t cell_delta_mv;
extern uint16_t cell_min_rc_mv;
extern uint16_t cell_avg_rc_mv;
extern int16_t cell_rc_ohmic_mv;
extern int16_t cell_rc_polar_mv;
extern int16_t cell_rc_total_mv;
extern int16_t temp_ic_c;
extern int16_t temp_ts1_c;
extern int16_t temp_ts3_c;
extern int16_t temp_cell_c;
extern int16_t temp_fet_c;
extern uint16_t alarm_status;
extern uint16_t alarm_raw;
extern uint16_t battery_status;
extern uint16_t manufacturing_status;
extern uint8_t fet_status;
extern uint8_t fet_control_request;
extern uint8_t safety_alert_a;
extern uint8_t safety_alert_b;
extern uint8_t safety_alert_c;
extern uint8_t safety_status_a;
extern uint8_t safety_status_b;
extern uint8_t safety_status_c;
extern uint8_t pf_status_a;
extern uint8_t pf_status_b;
extern uint8_t pf_status_c;
extern uint8_t pf_status_d;
extern bool fault_active;
extern float soc_percent;
extern float display_soc_percent;
extern uint8_t soc_confidence_percent;
extern float soc_residual_percent;
extern float soc_kalman_gain;
extern float soc_p;
extern float soc_active_capacity_mah;
extern uint32_t charge_throughput_mah;
extern uint32_t discharge_throughput_mah;
extern uint32_t cycle_count;
extern uint8_t soh_percent;
extern uint8_t soh_confidence_percent;
extern uint16_t balance_mask;

#endif /* APP_BATMAN_H */
