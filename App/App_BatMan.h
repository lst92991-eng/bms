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

#define APP_BATMAN_DEFAULT_SOC_PERCENT (50.0f)

typedef enum
{
    APP_BATMAN_CONFIG_UNCHECKED = 0,
    APP_BATMAN_CONFIG_WRITING,
    APP_BATMAN_CONFIG_VERIFYING,
    APP_BATMAN_CONFIG_VALID,
    APP_BATMAN_CONFIG_RECOVERY_REQUIRED,
    APP_BATMAN_CONFIG_INVALID_LATCHED
} App_BatMan_ConfigStateTypeDef;

typedef enum
{
    APP_BATMAN_FRAME_NEVER = 0,
    APP_BATMAN_FRAME_VALID,
    APP_BATMAN_FRAME_TRANSPORT_ERROR,
    APP_BATMAN_FRAME_PROTOCOL_ERROR,
    APP_BATMAN_FRAME_DEADLINE_EXCEEDED
} App_BatMan_FrameStateTypeDef;

typedef struct
{
    App_BatMan_FrameStateTypeDef state;
    uint8_t driver_status;
    uint32_t sequence;
    uint32_t last_valid_tick_ms;
    uint32_t age_ms;
    uint32_t elapsed_ms;
} App_BatMan_FrameStatusTypeDef;

typedef struct
{
    uint8_t desired_off_mask;
    uint8_t commanded_off_mask;
    uint8_t observed_fet_status;
    bool observed_valid;
    bool request_valid;
} App_BatMan_FetControlStateTypeDef;

enum
{
    APP_BATMAN_FAULT_NONE = 0u,
    APP_BATMAN_FAULT_COMMUNICATION = (1u << 0),
    APP_BATMAN_FAULT_CELL_RANGE = (1u << 1),
    APP_BATMAN_FAULT_SAFETY = (1u << 2),
    APP_BATMAN_FAULT_PERMANENT_FAILURE = (1u << 3),
    APP_BATMAN_FAULT_CELL_TEMPERATURE_INVALID = (1u << 4),
    APP_BATMAN_FAULT_CONFIG_INVALID = (1u << 5),
    APP_BATMAN_FAULT_FET_CONTROL_INVALID = (1u << 6)
};

/**
 * @brief 系统最早阶段幂等关闭 BQ 四路 FET，并读 FET_STATUS 确认。
 * @note 无日志、无 EEPROM；失败仅返回 false，由平台保持 standby/inhibit。
 */
bool App_BatMan_EarlySafeOutputs(void);

/**
 * @brief 初始化 BQ76952，写入基线 Data Memory，并让主 FET 进入受控默认关断态。
 */
bool App_BatMan_Init(void);

/**
 * @brief BQ 周期监控任务。
 * @param interval_ms 调用周期，用于 SOC/SOH 积分。
 */
void App_BatMan_Task(uint32_t interval_ms);

/**
 * @brief 低优先级维护入口，承载 EEPROM 与串口诊断等可阻塞工作。
 * @note 不得从 BatMan/Power 关键周期中调用。
 */
void App_BatMan_MaintenanceTask(uint32_t interval_ms);

/** @brief BQ ALERT ISR 入口；只锁存 pending，不访问 I2C。 */
void App_BatMan_NotifyAlertFromISR(void);

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

/** @brief 仅在完整 DM manifest 回读一致后返回 true。 */
bool App_BatMan_IsConfigValid(void);
App_BatMan_ConfigStateTypeDef App_BatMan_GetConfigState(void);

/**
 * @brief 至少一个已启用外部温度通道通过协议范围检查时返回 true。
 * @note 仅表示运行期读数有效；NTC 型号、位置和整机误差标定仍需 HIL 证明。
 */
bool App_BatMan_IsCellTemperatureValid(void);

/** @brief 返回当前安全故障位，供 Power 等上层只读门禁。 */
uint32_t App_BatMan_GetFaultFlags(void);
void App_BatMan_GetFrameStatus(App_BatMan_FrameStatusTypeDef *status);
void App_BatMan_GetFetControlState(App_BatMan_FetControlStateTypeDef *state);

/**
 * @brief 打印 bqfast 自动停表时的 BQ 侧原因。
 */
void App_BatMan_PrintMonitorStopReason(void);

/**
 * @brief 设置主充放电 MOS。
 * @param charge_enable true 允许 CHG/PCHG，false 强制关断充电路径。
 * @param discharge_enable true 允许 DSG/PDSG，false 强制关断放电路径。
 * @param safety_authorization_epoch Safety supervisor 当前代际令牌；全关请求可传 0。
 * @return true 写入成功。
 */
bool App_BatMan_SetMainFets(bool charge_enable,
                            bool discharge_enable,
                            uint32_t safety_authorization_epoch);

/**
 * @brief BQ 复位指纹或通信证明丢失后的完整重新认证。
 * @note 固定执行全关、器件身份核对、manifest 重写/回读及 FET 全关回读；成功后仍需
 *       一帧新的完整采样，Safety 才能重新开放功率授权。
 */
bool App_BatMan_ReauthenticateAfterReset(void);

/**
 * @brief 关闭主 FET 后发送 BQ76952 SHUTDOWN 子命令。
 * @return true 表示主 FET 关断命令和 SHUTDOWN 子命令都已成功发出。
 */
bool App_BatMan_RequestShutdown(void);

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
extern uint32_t soh_learned_capacity_mah;
extern uint32_t soh_learning_discharge_mah;
extern uint16_t soh_capacity_learning_count;
extern uint8_t soh_percent;
extern uint8_t health_score_percent;
extern uint8_t soh_confidence_percent;
extern bool soh_capacity_valid;
extern bool soh_learning_active;
extern uint16_t balance_mask;

#endif /* APP_BATMAN_H */
