#ifndef APP_BATMAN_H
#define APP_BATMAN_H

#include <stdbool.h>
#include <stdint.h>

/*
 * BQ APP 对外常量。
 * 只保留显示、CAN 或主机日志需要的值；内部调参项留在 App_BatMan.c。
 */
#define APP_BATMAN_CELL_COUNT                    (6u)
#define APP_BATMAN_CELL_CAP_TYP_MAH              (5000u)
#define APP_BATMAN_CELL_FULL_MV                  (4200u)
#define APP_BATMAN_PACK_FULL_MV                  (25200u)
#define APP_BATMAN_CELL_VALID_MIN_MV             (2500u)
#define APP_BATMAN_CELL_VALID_MAX_MV             (4300u)
#define APP_BATMAN_CURRENT_LOW_CURRENT_A         (0.10f)
#define APP_BATMAN_DEBUG_PERIOD_MS               (1000u)
#define APP_BATMAN_DEFAULT_SOC_PERCENT           (50.0f)
#define APP_BATMAN_CAPACITY_MAH                  (5000u)
#define APP_BATMAN_CC2_RAW_POLARITY              (1)

/**
 * @brief 初始化 BQ76952，写入基线 Data Memory，并把 FET 控制权交给 BQ。
 */
void App_BatMan_Init(void);

/**
 * @brief BQ 周期监控任务。
 * @param interval_ms 调用周期，用于 SOC/SOH 积分。
 */
void App_BatMan_Task(uint32_t interval_ms);

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
extern int16_t temp_ic_c;
extern int16_t temp_ts1_c;
extern int16_t temp_ts3_c;
extern int16_t temp_cell_c;
extern int16_t temp_fet_c;
extern uint16_t alarm_status;
extern uint16_t alarm_raw;
extern uint16_t battery_status;
extern uint8_t fet_status;
extern uint8_t safety_status_a;
extern uint8_t safety_status_b;
extern uint8_t safety_status_c;
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
