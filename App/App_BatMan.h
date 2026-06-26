#ifndef APP_BATMAN_H
#define APP_BATMAN_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @file App_BatMan.h
 * @brief BQ76952 业务层主入口。
 *
 * 这一层故意保持和旧项目一样“薄而直白”：
 * - 负责把 BQ76952 的 direct command / subcommand / Data Memory 串起来；
 * - 负责把单体电压、电流、温度、告警、均衡、FET 策略组织成流程；
 * - 不再通过厚 COM wrapper 传递业务语义。
 *
 * 代码风格目标：
 * - 打开文件就能看懂整个 BQ bring-up 顺序；
 * - 每个步骤都保留清晰的中文注释；
 * - 业务逻辑尽量放在一个源文件里，方便教学和排障。
 */

#define APP_BATMAN_CELL_COUNT              (6u)
#define APP_BATMAN_CELL_CAP_TYP_MAH        (5000u)
#define APP_BATMAN_CELL_CAP_MIN_MAH        (4900u)
#define APP_BATMAN_CELL_NOMINAL_MV         (3650u)
#define APP_BATMAN_CELL_FULL_MV            (4200u)
#define APP_BATMAN_PACK_FULL_MV            (25200u)
#define APP_BATMAN_CELL_CHG_5A_MV          (4100u)
#define APP_BATMAN_PACK_CHG_5A_MV          (24600u)
#define APP_BATMAN_CELL_DSG_CUTOFF_MV      (2500u)
#define APP_BATMAN_CELL_DSG_LIFE_MV        (3000u)
#define APP_BATMAN_CELL_VALID_MIN_MV       (2500u)
#define APP_BATMAN_CELL_VALID_MAX_MV       (4300u)
#define APP_BATMAN_BALANCE_RESISTOR_OHM    (62u)
#define APP_BATMAN_BALANCE_MIN_CELL_MV     (3900u)
#define APP_BATMAN_BALANCE_START_DELTA_MV  (30u)
#define APP_BATMAN_BALANCE_STOP_DELTA_MV   (15u)
#define APP_BATMAN_BALANCE_MIN_TEMP_C      (0)
#define APP_BATMAN_BALANCE_MAX_TEMP_C      (45)
#define APP_BATMAN_BALANCE_MAX_INTERNAL_TEMP_C (70)
#define APP_BATMAN_CURRENT_LOW_CURRENT_A   (0.10f)
#define APP_BATMAN_DEBUG_PERIOD_MS         (1000u)
#define APP_BATMAN_DEFAULT_SOC_PERCENT      (50.0f)
#define APP_BATMAN_CAPACITY_MAH             (5000u)

/*
 * CC2 当前值的原始符号，默认保持“正数=充电、负数=放电”的 APP 约定。
 * 如果上板后发现方向相反，只需要把这里改成 -1，而不用动业务流程。
 */
#define APP_BATMAN_CC2_RAW_POLARITY         (1)

/* TS1 / TS3 热敏参数，来自用户补充的实物信息。 */
#define APP_BATMAN_NTC_R25_OHM              (10000u)
#define APP_BATMAN_NTC_BETA_K               (3950u)
#define APP_BATMAN_NTC_TOLERANCE_PERCENT    (1u)

void App_BatMan_Init(void);
void App_BatMan_Task(uint16_t interval_ms);

void App_BatMan_LoadCellsVoltage(void);
void App_BatMan_LoadBatVoltage(void);
void App_BatMan_LoadCurrent(void);
void App_BatMan_LoadTemperature(void);
void App_BatMan_LoadBqStatus(void);
void App_BatMan_UpdateFaultState(void);
void App_BatMan_CalcCoulomb(uint16_t delta_ms);
void App_BatMan_CalcSoc(uint16_t interval_ms);
void App_BatMan_CellBalance(void);
void App_BatMan_ApplyPolicy(void);
void App_BatMan_PrintDebug(void);

void App_BatMan_SetChargeState(uint8_t charge_state);
void App_BatMan_SetDischargeState(uint8_t discharge_state);
void App_BatMan_SetChargeAllowed(bool allowed);
void App_BatMan_SetDischargeAllowed(bool allowed);

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
extern uint16_t balance_mask;
extern uint8_t fet_status;
extern uint8_t safety_status_a;
extern uint8_t safety_status_b;
extern uint8_t safety_status_c;
extern bool fault_active;
extern bool charge_allowed;
extern bool discharge_allowed;
extern float soc_percent;
extern float display_soc_percent;

#endif /* APP_BATMAN_H */
