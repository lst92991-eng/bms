#ifndef BMS_SAFETY_H
#define BMS_SAFETY_H

/**
 * @file bms_safety.h
 * @brief BMS V2 统一安全许可与功率目标计算接口。
 */

#include <stdbool.h>
#include <stdint.h>

#include "bms_action.h"
#include "bms_snapshot.h"
#include "bms_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    BMS_SAFETY_MODE_BOOT_SAFE = 0,
    BMS_SAFETY_MODE_STANDBY,
    BMS_SAFETY_MODE_ACTIVE,
    BMS_SAFETY_MODE_SHUTDOWN_PENDING,
    BMS_SAFETY_MODE_SHUTDOWN,
    BMS_SAFETY_MODE_WAKE,
    BMS_SAFETY_MODE_FAULT,
    BMS_SAFETY_MODE_FAULT_LATCHED
} BmsSafety_Mode;

#define BMS_SAFETY_REASON_NONE                  (UINT32_C(0))
#define BMS_SAFETY_REASON_SNAPSHOT_INVALID      (UINT32_C(1) << 0u)
#define BMS_SAFETY_REASON_CELLS_INVALID         (UINT32_C(1) << 1u)
#define BMS_SAFETY_REASON_CURRENT_INVALID       (UINT32_C(1) << 2u)
#define BMS_SAFETY_REASON_THERMAL_INVALID       (UINT32_C(1) << 3u)
#define BMS_SAFETY_REASON_BQ_OFFLINE            (UINT32_C(1) << 4u)
#define BMS_SAFETY_REASON_BQ_CONFIG_INVALID     (UINT32_C(1) << 5u)
#define BMS_SAFETY_REASON_BQ_FAULT              (UINT32_C(1) << 6u)
#define BMS_SAFETY_REASON_SC_INVALID            (UINT32_C(1) << 7u)
#define BMS_SAFETY_REASON_SC_INPUT_INVALID      (UINT32_C(1) << 8u)
#define BMS_SAFETY_REASON_SC_FAULT              (UINT32_C(1) << 9u)
#define BMS_SAFETY_REASON_CELL_HIGH             (UINT32_C(1) << 10u)
#define BMS_SAFETY_REASON_CELL_LOW              (UINT32_C(1) << 11u)
#define BMS_SAFETY_REASON_CHARGE_TEMPERATURE    (UINT32_C(1) << 12u)
#define BMS_SAFETY_REASON_DISCHARGE_TEMPERATURE (UINT32_C(1) << 13u)
#define BMS_SAFETY_REASON_IC_TEMPERATURE        (UINT32_C(1) << 14u)
#define BMS_SAFETY_REASON_CHARGE_OVERCURRENT    (UINT32_C(1) << 15u)
#define BMS_SAFETY_REASON_DISCHARGE_OVERCURRENT (UINT32_C(1) << 16u)
#define BMS_SAFETY_REASON_FAULT_ACTION          (UINT32_C(1) << 17u)
#define BMS_SAFETY_REASON_SHUTDOWN_REQUESTED    (UINT32_C(1) << 18u)
#define BMS_SAFETY_REASON_WAKE_NOT_AUTHORIZED   (UINT32_C(1) << 19u)
#define BMS_SAFETY_REASON_BALANCE_CONDITION     (UINT32_C(1) << 20u)
#define BMS_SAFETY_REASON_PACK_VOLTAGE_INVALID  (UINT32_C(1) << 21u)

typedef struct
{
    uint32_t snapshot_max_age_ms;
    uint32_t cells_max_age_ms;
    uint32_t pack_voltage_max_age_ms;
    uint32_t current_max_age_ms;
    uint32_t thermal_max_age_ms;
    uint32_t bq_status_max_age_ms;
    uint32_t sc_status_max_age_ms;
    uint32_t permit_validity_ms;
    uint32_t wake_timeout_ms;

    uint16_t cell_valid_min_mv;
    uint16_t cell_valid_max_mv;
    uint16_t charge_stop_cell_mv;
    uint16_t charge_resume_cell_mv;
    uint16_t discharge_stop_cell_mv;
    uint16_t discharge_resume_cell_mv;
    uint16_t balance_min_cell_mv;
    uint16_t balance_max_cell_mv;
    uint16_t balance_start_delta_mv;
    uint16_t balance_stop_delta_mv;
    uint16_t stack_cell_sum_tolerance_mv;

    int16_t charge_temp_min_deg_c;
    int16_t charge_temp_max_deg_c;
    int16_t discharge_temp_min_deg_c;
    int16_t discharge_temp_max_deg_c;
    int16_t balance_temp_min_deg_c;
    int16_t balance_temp_max_deg_c;
    int16_t ic_temp_max_deg_c;

    int32_t max_charge_current_ma;
    int32_t max_discharge_current_ma;
    int32_t balance_max_abs_current_ma;
    uint8_t required_charge_temp_sensor_count;
    uint8_t required_discharge_temp_sensor_count;
    uint8_t required_balance_temp_sensor_count;
} BmsSafety_Config;

typedef struct
{
    bool host_requested;
    bool command_confirmed;
    bool bq_shutdown_seen;
    bool bq_offline_seen;
    uint32_t wake_started_at_ms;
} BmsShutdownEvidence;

typedef struct
{
    uint32_t now_ms;
    const BmsSnapshot *snapshot;
    uint32_t effective_fault_action_mask;
    bool request_charge_path;
    bool request_discharge_path;
    bool request_sc_charge;
    bool request_balancing;
    bool request_shutdown;
    BmsShutdownEvidence shutdown_evidence;
} BmsSafety_Input;

typedef struct
{
    BmsSafety_Mode mode;
    bool charge_voltage_latched;
    bool discharge_voltage_latched;
    bool balancing_active;
    uint8_t last_target_mask;
    uint32_t permit_generation;
    bool initialized;
} BmsSafety_State;

typedef struct
{
    bool allow_charge_path;
    bool allow_discharge_path;
    bool request_sc_charge;
    bool allow_balancing;
    bool require_pstop_asserted;
    uint32_t reason_mask;
    uint32_t snapshot_sequence;
    uint32_t valid_until_ms;
    uint32_t permit_generation;
} BmsActuator_Target;

typedef struct
{
    BmsSafety_Mode mode;
    bool snapshot_usable;
    bool wake_charge_authorized;
    uint8_t valid_cell_temperature_count;
    uint16_t minimum_cell_mv;
    uint16_t maximum_cell_mv;
    uint16_t cell_delta_mv;
    BmsActuator_Target target;
} BmsSafety_Decision;

/**
 * @brief 初始化安全监督器状态并验证产品安全配置。
 *
 * @post 成功后状态为 BOOT_SAFE，所有历史许可均失效。
 * @concurrency 仅由 BmsSafetyTask 调用。
 */
BmsStatus BmsSafety_Init(BmsSafety_State *state, const BmsSafety_Config *config);

/**
 * @brief 根据完整快照、故障动作和 shutdown 证据计算唯一功率目标。
 *
 * @param[in] config 产品安全配置。
 * @param[in,out] state 安全状态；仅保存回差和 permit generation。
 * @param[in] input 本周期输入。
 * @param[out] decision 完整决策；函数先写入全安全默认值，成功后才按证据释放许可。
 * @return BMS_STATUS_OK 或参数/配置/状态错误。
 *
 * @pre state 已通过 BmsSafety_Init() 初始化。
 * @post 任何输入无效、未知模式或内部错误均不会产生充电/放电/均衡许可。
 * @safety 普通 BQ offline 不能触发 wake-charge；只有完整 shutdown evidence 才允许。
 */
BmsStatus BmsSafety_Evaluate(const BmsSafety_Config *config,
                             BmsSafety_State *state,
                             const BmsSafety_Input *input,
                             BmsSafety_Decision *decision);

#ifdef __cplusplus
}
#endif

#endif /* BMS_SAFETY_H */
