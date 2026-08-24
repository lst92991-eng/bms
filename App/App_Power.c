#include "App_Power.h"

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "App_BatMan.h"
#include "App_SC8815.h"
#include "App_Safety.h"
#include "Com_SOC.h"
#include "Int_BQ76952_BSP.h"
#include "Int_Log.h"

#ifndef BMS_ENGINEERING_BUILD
#define BMS_ENGINEERING_BUILD 0
#endif

#ifndef APP_POWER_BQ_WAKE_HIL_ENABLE
/* Release 固定关闭；Engineering 仅提供 500 ms one-shot 供实板时序验证。 */
#define APP_POWER_BQ_WAKE_HIL_ENABLE BMS_ENGINEERING_BUILD
#endif

#if (BMS_ENGINEERING_BUILD != 0) && (APP_POWER_BQ_WAKE_HIL_ENABLE != 0)
#define APP_POWER_BQ_WAKE_PULSE_ALLOWED 1
#else
#define APP_POWER_BQ_WAKE_PULSE_ALLOWED 0
#endif

enum
{
    APP_POWER_DISCHARGE_CURRENT_MA = 12000,
    APP_POWER_CELL_LOW_MV = 3000u,
    APP_POWER_CELL_RECOVER_MV = 3200u,
    APP_POWER_CELL_FULL_STOP_MV = 4200u,
    APP_POWER_CELL_FULL_RESUME_MV = 4180u,
    APP_POWER_TOP_BALANCE_STOP_MV = 4180u,
    APP_POWER_TOP_BALANCE_RESUME_MV = 4150u,
    APP_POWER_TOP_BALANCE_START_DELTA_MV = 40u,
    APP_POWER_TOP_BALANCE_STOP_DELTA_MV = 20u,
    APP_POWER_CHARGE_TEMP_MIN_C = 0,
    APP_POWER_CHARGE_TEMP_DERATE_END_C = 15,
    APP_POWER_CHARGE_TEMP_MAX_C = 45,
    APP_POWER_DISCHARGE_TEMP_MIN_C = -20,
    APP_POWER_DISCHARGE_TEMP_MAX_C = 60,
    APP_POWER_CHARGE_LIMIT_COLD_MA = 1000u,
    /* 25.2V 目标对应 4.20V/Cell，只允许规格书标准充电电流 1A。 */
    APP_POWER_CHARGE_LIMIT_NOMINAL_MA = 1000u,
    APP_POWER_ANCHOR_CONFIRM_CURRENT_MA = 250,
    APP_POWER_SC_INPUT_VALID_MV = 12000u,
    APP_POWER_BQ_WAKE_PULSE_DEADLINE_MS = 500u,
    APP_POWER_BQ_RECOVERY_FRAME_TIMEOUT_MS = 2500u,
    APP_POWER_SC_SAMPLE_MAX_AGE_MS = 2500u,
    APP_POWER_BQ_SAFETY_A_SCD_MASK = 0x80u,
    APP_POWER_ANCHOR_CONFIRM_WINDOW_MS = 10000u,
    APP_POWER_ANCHOR_MIN_CONFIRM_MS = 1000u,
    APP_POWER_FULL_CONFIRM_MIN_CELL_MV = 4150u,
    APP_POWER_FULL_CONFIRM_MAX_DELTA_MV = 40u,
    APP_POWER_EMPTY_CONFIRM_MAX_CELL_MV = 3100u
};

static App_Power_StateTypeDef s_power_state;
static App_Power_StopReasonTypeDef s_stop_reason;
static App_Power_ShutdownProvenanceTypeDef s_shutdown;
static bool s_charge_allowed;
static bool s_discharge_allowed;
static bool s_desired_charge_fet;
static bool s_desired_discharge_fet;
static bool s_desired_sc_charge;
static bool s_commanded_charge_fet;
static bool s_commanded_discharge_fet;
static bool s_commanded_sc_charge;
static bool s_observed_charge_fet;
static bool s_observed_discharge_fet;
static bool s_observed_sc_charge;
static bool s_charge_full_latched;
static bool s_charge_complete_latched;
static bool s_top_balance_latched;
static bool s_discharge_scd_latched;
static bool s_discharge_rearm_required;
static bool s_full_anchor_pending;
static bool s_empty_anchor_pending;
static bool s_bq_shutdown_requested;
static bool s_charge_inhibit;
static uint16_t s_charge_limit_ma;
static bool s_charge_limit_initialized;
static uint32_t s_bq_wake_ms;
static uint32_t s_bq_recovery_verify_ms;
static uint32_t s_bq_recovery_frame_sequence;
static bool s_bq_recovery_configured;
static uint32_t s_full_anchor_confirm_ms;
static uint32_t s_empty_anchor_confirm_ms;

/* 由 BQ 单所有者代理提供；无效时发布默认禁止充电、放电和均衡。 */
extern bool App_BatMan_IsConfigValid(void);
extern bool App_BatMan_IsCellTemperatureValid(void);

static uint32_t App_Power_AbsCurrentMa(int32_t value)
{
    if (value >= 0)
    {
        return (uint32_t)value;
    }
    return (uint32_t)(-(value + 1)) + 1u;
}

static void App_Power_RefreshObserved(void)
{
    App_SC8815_SnapshotTypeDef sc;

    s_observed_charge_fet = (fet_status & BQ76952_FET_STATUS_CHG_FET_MASK) != 0u;
    s_observed_discharge_fet = (fet_status & BQ76952_FET_STATUS_DSG_FET_MASK) != 0u;
    App_SC8815_GetSnapshot(&sc);
    s_observed_sc_charge = sc.commanded_charge && !sc.observed_standby;
}

static void App_Power_StopScSynchronously(void)
{
    App_SC8815_RequestCharge(false);
    s_desired_sc_charge = false;
    s_commanded_sc_charge = false;
    s_observed_sc_charge = false;
}

static bool App_Power_ForceOutputsOff(void)
{
    bool bq_off_confirmed;

    App_Power_StopScSynchronously();
    s_desired_charge_fet = false;
    s_desired_discharge_fet = false;
    s_desired_sc_charge = false;
    bq_off_confirmed = App_BatMan_AllMainFetsOff();
    if (bq_off_confirmed)
    {
        s_commanded_charge_fet = false;
        s_commanded_discharge_fet = false;
        App_Power_RefreshObserved();
    }
    return bq_off_confirmed;
}

static bool App_Power_RequestScChargeWithEpoch(uint16_t limit_ma,
                                               uint32_t safety_authorization_epoch)
{
    App_SC8815_SnapshotTypeDef sc;

    App_SC8815_GetSnapshot(&sc);
    if (!sc.comm_ok || sc.vbus_short || sc.otp)
    {
        App_Power_StopScSynchronously();
        s_charge_allowed = false;
        s_stop_reason = APP_POWER_STOP_SC_FAULT;
        return false;
    }
    if (!App_SC8815_SetChargeCurrentLimitMa(limit_ma))
    {
        App_Power_StopScSynchronously();
        s_charge_allowed = false;
        s_stop_reason = APP_POWER_STOP_COMMAND_FAILED;
        return false;
    }
    App_SC8815_GetSnapshot(&sc);
    if ((!sc.desired_charge || (sc.safety_authorization_epoch != safety_authorization_epoch)) &&
        !App_SC8815_RequestChargeAuthorized(safety_authorization_epoch))
    {
        App_Power_StopScSynchronously();
        s_charge_allowed = false;
        s_stop_reason = APP_POWER_STOP_COMMAND_FAILED;
        return false;
    }
    s_desired_sc_charge = true;
    s_commanded_sc_charge = true;
    return true;
}

static bool App_Power_RequestScWakeCharge(uint16_t limit_ma, bool input_ok, bool sc_ok)
{
    const App_SafetyWakeEvidenceTypeDef evidence = {
        .reason = APP_SAFETY_WAKE_REASON_BQ_HOST_SHUTDOWN,
        .host_requested = s_shutdown.host_requested,
        .shutdown_command_succeeded = s_shutdown.shutdown_command_succeeded,
        .expected_offline_seen = s_shutdown.expected_offline_seen,
        .input_valid = input_ok,
        .sc_ready = sc_ok,
    };
    uint32_t safety_authorization_epoch;

    if (!App_Safety_GetBqWakeAuthorization(&evidence, &safety_authorization_epoch))
    {
        App_Power_StopScSynchronously();
        s_charge_allowed = false;
        s_stop_reason = APP_POWER_STOP_SAFETY_NOT_AUTHORIZED;
        return false;
    }
    s_shutdown.wake_authorization_issued = true;
    if (!App_Power_RequestScChargeWithEpoch(limit_ma, safety_authorization_epoch))
    {
        return false;
    }
    s_shutdown.valid_input_seen = true;
    s_shutdown.wake_pulse_started = true;
    return true;
}

static bool App_Power_SetOutput(bool charge_fet_enable,
                                bool discharge_enable,
                                bool sc_charge_enable,
                                uint16_t charge_limit_ma)
{
    bool bq_command_needed;
    bool release_requested;
    uint32_t safety_authorization_epoch = 0u;

    sc_charge_enable = charge_fet_enable && sc_charge_enable;
    release_requested = charge_fet_enable || discharge_enable;
    if (!sc_charge_enable)
    {
        /* 所有禁充分支在任何 I2C/FET 操作之前先同步拉高 PSTOP。 */
        App_Power_StopScSynchronously();
    }

    if (release_requested && !App_Safety_GetPowerReleaseAuthorization(&safety_authorization_epoch))
    {
        bool off_confirmed = App_Power_ForceOutputsOff();

        s_charge_allowed = false;
        s_discharge_allowed = false;
        s_power_state = APP_POWER_STATE_FAULT;
        s_stop_reason =
            off_confirmed ? APP_POWER_STOP_SAFETY_NOT_AUTHORIZED : APP_POWER_STOP_COMMAND_FAILED;
        App_Power_RefreshObserved();
        return false;
    }

    s_desired_charge_fet = charge_fet_enable;
    s_desired_discharge_fet = discharge_enable;
    s_desired_sc_charge = sc_charge_enable;
    App_Power_RefreshObserved();

    bq_command_needed = (s_commanded_charge_fet != charge_fet_enable) ||
                        (s_commanded_discharge_fet != discharge_enable) ||
                        (s_observed_charge_fet != charge_fet_enable) ||
                        (s_observed_discharge_fet != discharge_enable);
    if (bq_command_needed)
    {
        if (!App_BatMan_SetMainFets(
                charge_fet_enable, discharge_enable, safety_authorization_epoch))
        {
            (void)App_Power_ForceOutputsOff();
            s_charge_allowed = false;
            s_discharge_allowed = false;
            s_power_state = APP_POWER_STATE_FAULT;
            s_stop_reason = APP_POWER_STOP_COMMAND_FAILED;
            return false;
        }
        s_commanded_charge_fet = charge_fet_enable;
        s_commanded_discharge_fet = discharge_enable;
    }

    if (sc_charge_enable)
    {
        if (!App_Power_RequestScChargeWithEpoch(charge_limit_ma, safety_authorization_epoch))
        {
            bool off_confirmed = App_Power_ForceOutputsOff();

            s_charge_allowed = false;
            s_discharge_allowed = false;
            s_power_state = APP_POWER_STATE_FAULT;
            if (!off_confirmed)
            {
                s_stop_reason = APP_POWER_STOP_COMMAND_FAILED;
            }
            return false;
        }
    }

    if (release_requested && !App_Safety_IsPowerReleaseAuthorized(safety_authorization_epoch))
    {
        bool off_confirmed = App_Power_ForceOutputsOff();

        s_charge_allowed = false;
        s_discharge_allowed = false;
        s_power_state = APP_POWER_STATE_FAULT;
        s_stop_reason =
            off_confirmed ? APP_POWER_STOP_SAFETY_NOT_AUTHORIZED : APP_POWER_STOP_COMMAND_FAILED;
        App_Power_RefreshObserved();
        return false;
    }
    return true;
}

static bool App_Power_SetScWakeCharge(bool enable, bool input_ok, bool sc_ok)
{
    if (!enable)
    {
        App_Power_StopScSynchronously();
        return true;
    }

    if (!s_shutdown.host_requested || !s_shutdown.shutdown_command_succeeded ||
        !s_shutdown.expected_offline_seen || !input_ok || !sc_ok)
    {
        App_Power_StopScSynchronously();
        s_power_state = APP_POWER_STATE_FAULT;
        s_stop_reason = APP_POWER_STOP_BQ_OFFLINE;
        return false;
    }
    if (s_shutdown.wake_pulse_started)
    {
        return true;
    }
    return App_Power_RequestScWakeCharge(APP_POWER_CHARGE_LIMIT_COLD_MA, input_ok, sc_ok);
}

static bool App_Power_ExecuteBqShutdown(void)
{
    App_Power_StopScSynchronously();
    s_charge_allowed = false;
    s_discharge_allowed = false;
    memset(&s_shutdown, 0, sizeof(s_shutdown));
    s_shutdown.host_requested = true;
    s_bq_wake_ms = 0u;
    s_bq_recovery_verify_ms = 0u;
    s_bq_recovery_frame_sequence = 0u;
    s_bq_recovery_configured = false;

    if (!App_BatMan_RequestShutdown())
    {
        s_power_state = APP_POWER_STATE_FAULT;
        s_stop_reason = APP_POWER_STOP_SHUTDOWN_FAILED;
        return false;
    }

    s_commanded_charge_fet = false;
    s_commanded_discharge_fet = false;
    s_shutdown.shutdown_command_succeeded = true;
    s_power_state = APP_POWER_STATE_BQ_SHUTDOWN;
    s_stop_reason = APP_POWER_STOP_HOST_SHUTDOWN;
    return true;
}

static bool App_Power_StartBqRecovery(void)
{
    App_BatMan_FrameStatusTypeDef frame;

    App_Power_StopScSynchronously();
    App_BatMan_GetFrameStatus(&frame);
    if (!App_BatMan_ReauthenticateAfterReset())
    {
        s_power_state = APP_POWER_STATE_FAULT;
        s_stop_reason = APP_POWER_STOP_COMMAND_FAILED;
        return false;
    }

    s_shutdown.recovery_config_succeeded = true;
    s_bq_recovery_configured = true;
    s_bq_recovery_frame_sequence = frame.sequence;
    s_bq_recovery_verify_ms = 0u;
    return true;
}

static bool App_Power_ConfirmBqRecoveryFrame(uint32_t interval_ms)
{
    App_BatMan_FrameStatusTypeDef frame;

    App_Power_StopScSynchronously();
    App_BatMan_GetFrameStatus(&frame);
    s_bq_recovery_verify_ms += interval_ms;
    if ((frame.state == APP_BATMAN_FRAME_VALID) &&
        (frame.sequence != s_bq_recovery_frame_sequence) && App_BatMan_IsOnline() &&
        App_BatMan_IsConfigValid())
    {
        s_shutdown.post_recovery_frame_seen = true;
        s_bq_wake_ms = 0u;
        s_power_state = APP_POWER_STATE_OFF;
        s_stop_reason = APP_POWER_STOP_BOOT;
        s_commanded_charge_fet = false;
        s_commanded_discharge_fet = false;
        return true;
    }
    if (s_bq_recovery_verify_ms >= APP_POWER_BQ_RECOVERY_FRAME_TIMEOUT_MS)
    {
        s_power_state = APP_POWER_STATE_FAULT;
        s_stop_reason = APP_POWER_STOP_WAKE_TIMEOUT;
    }
    return false;
}

static bool
App_Power_HandleShutdownState(uint32_t interval_ms, bool bq_online, bool input_ok, bool sc_ok)
{
    if ((s_power_state != APP_POWER_STATE_BQ_SHUTDOWN) &&
        (s_power_state != APP_POWER_STATE_BQ_WAKE))
    {
        return false;
    }

    s_charge_allowed = false;
    s_discharge_allowed = false;
    if (!s_shutdown.host_requested || !s_shutdown.shutdown_command_succeeded)
    {
        App_Power_StopScSynchronously();
        s_power_state = APP_POWER_STATE_FAULT;
        s_stop_reason = APP_POWER_STOP_SHUTDOWN_FAILED;
        return true;
    }

    if (s_bq_recovery_configured)
    {
        (void)App_Power_ConfirmBqRecoveryFrame(interval_ms);
        return true;
    }

    if (!bq_online)
    {
        s_shutdown.expected_offline_seen = true;
        if (input_ok && sc_ok)
        {
            s_shutdown.valid_input_seen = true;
            if (APP_POWER_BQ_WAKE_PULSE_ALLOWED == 0)
            {
                App_Power_StopScSynchronously();
                s_stop_reason = APP_POWER_STOP_WAKE_DISABLED;
            }
            else
            {
                s_power_state = APP_POWER_STATE_BQ_WAKE;
                if (!s_shutdown.wake_pulse_started)
                {
                    s_bq_wake_ms = 0u;
                    if (!App_Power_SetScWakeCharge(true, input_ok, sc_ok))
                    {
                        s_power_state = APP_POWER_STATE_FAULT;
                    }
                }
                else
                {
                    s_bq_wake_ms += interval_ms;
                    if (s_bq_wake_ms >= APP_POWER_BQ_WAKE_PULSE_DEADLINE_MS)
                    {
                        App_Power_StopScSynchronously();
                        s_power_state = APP_POWER_STATE_FAULT;
                        s_stop_reason = APP_POWER_STOP_WAKE_TIMEOUT;
                    }
                }
            }
        }
        else
        {
            App_Power_StopScSynchronously();
        }
        return true;
    }

    if (s_shutdown.expected_offline_seen && s_shutdown.valid_input_seen && input_ok && sc_ok)
    {
        (void)App_Power_StartBqRecovery();
    }
    else
    {
        /* 在线等待不是“已唤醒”；禁止用普通通信恢复伪造 shutdown 闭环。 */
        App_Power_StopScSynchronously();
    }
    return true;
}

static uint16_t App_Power_SelectChargeLimitMa(int16_t cell_temp_c)
{
    if (!s_charge_limit_initialized)
    {
        s_charge_limit_initialized = true;
        return (cell_temp_c < APP_POWER_CHARGE_TEMP_DERATE_END_C)
                   ? APP_POWER_CHARGE_LIMIT_COLD_MA
                   : APP_POWER_CHARGE_LIMIT_NOMINAL_MA;
    }
    if (s_charge_limit_ma == APP_POWER_CHARGE_LIMIT_COLD_MA)
    {
        return (cell_temp_c >= (APP_POWER_CHARGE_TEMP_DERATE_END_C + 2))
                   ? APP_POWER_CHARGE_LIMIT_NOMINAL_MA
                   : APP_POWER_CHARGE_LIMIT_COLD_MA;
    }
    /* 降额进入点不能低于规格边界；回差只放在升额方向。 */
    return (cell_temp_c < APP_POWER_CHARGE_TEMP_DERATE_END_C) ? APP_POWER_CHARGE_LIMIT_COLD_MA
                                                              : APP_POWER_CHARGE_LIMIT_NOMINAL_MA;
}

static void App_Power_ConfirmSocAnchors(uint32_t interval_ms, bool cells_valid)
{
    App_SC8815_SnapshotTypeDef sc;
    bool discharge_off;

    App_SC8815_GetSnapshot(&sc);
    discharge_off = (fet_status & BQ76952_FET_STATUS_DSG_FET_MASK) == 0u;
    if (s_full_anchor_pending)
    {
        s_full_anchor_confirm_ms += interval_ms;
        if (!cells_valid || (s_full_anchor_confirm_ms > APP_POWER_ANCHOR_CONFIRM_WINDOW_MS) ||
            (cell_min_mv < APP_POWER_FULL_CONFIRM_MIN_CELL_MV) ||
            (cell_delta_mv > APP_POWER_FULL_CONFIRM_MAX_DELTA_MV) || s_observed_sc_charge)
        {
            s_full_anchor_pending = false;
        }
        else if ((s_full_anchor_confirm_ms >= APP_POWER_ANCHOR_MIN_CONFIRM_MS) &&
                 sc.observed_standby &&
                 (App_Power_AbsCurrentMa(current_ma) <= APP_POWER_ANCHOR_CONFIRM_CURRENT_MA))
        {
            Com_SOC_NotifyAnchorEvent(COM_SOC_ANCHOR_FULL_COMPLETE);
            s_full_anchor_pending = false;
        }
    }
    if (s_empty_anchor_pending)
    {
        s_empty_anchor_confirm_ms += interval_ms;
        if (!cells_valid || (s_empty_anchor_confirm_ms > APP_POWER_ANCHOR_CONFIRM_WINDOW_MS) ||
            (cell_min_mv > APP_POWER_EMPTY_CONFIRM_MAX_CELL_MV) || s_desired_sc_charge ||
            s_observed_sc_charge || (current_ma > APP_POWER_ANCHOR_CONFIRM_CURRENT_MA))
        {
            s_empty_anchor_pending = false;
        }
        else if ((s_empty_anchor_confirm_ms >= APP_POWER_ANCHOR_MIN_CONFIRM_MS) && discharge_off &&
                 (App_Power_AbsCurrentMa(current_ma) <= APP_POWER_ANCHOR_CONFIRM_CURRENT_MA))
        {
            Com_SOC_NotifyAnchorEvent(COM_SOC_ANCHOR_EMPTY_CUTOFF);
            s_empty_anchor_pending = false;
        }
    }
}

bool App_Power_RequestBqShutdown(void)
{
    taskENTER_CRITICAL();
    s_bq_shutdown_requested = true;
    taskEXIT_CRITICAL();
    return true;
}

bool App_Power_RequestChargeInhibit(bool inhibit)
{
    if (inhibit)
    {
        App_Power_StopScSynchronously();
    }
    taskENTER_CRITICAL();
    s_charge_inhibit = inhibit;
    taskEXIT_CRITICAL();
    return true;
}

void App_Power_Init(void)
{
    s_power_state = APP_POWER_STATE_OFF;
    s_stop_reason = APP_POWER_STOP_BOOT;
    memset(&s_shutdown, 0, sizeof(s_shutdown));
    s_charge_allowed = false;
    s_discharge_allowed = false;
    s_desired_charge_fet = false;
    s_desired_discharge_fet = false;
    s_desired_sc_charge = false;
    s_commanded_charge_fet = false;
    s_commanded_discharge_fet = false;
    s_commanded_sc_charge = false;
    s_observed_charge_fet = false;
    s_observed_discharge_fet = false;
    s_observed_sc_charge = false;
    s_charge_full_latched = false;
    s_charge_complete_latched = false;
    s_top_balance_latched = false;
    s_discharge_scd_latched = false;
    s_discharge_rearm_required = false;
    s_full_anchor_pending = false;
    s_empty_anchor_pending = false;
    s_bq_shutdown_requested = false;
    s_charge_inhibit = false;
    s_charge_limit_ma = APP_POWER_CHARGE_LIMIT_NOMINAL_MA;
    s_charge_limit_initialized = false;
    s_bq_wake_ms = 0u;
    s_bq_recovery_verify_ms = 0u;
    s_bq_recovery_frame_sequence = 0u;
    s_bq_recovery_configured = false;
    s_full_anchor_confirm_ms = 0u;
    s_empty_anchor_confirm_ms = 0u;
    App_Power_StopScSynchronously();
}

void App_Power_Task(uint32_t interval_ms)
{
    App_SC8815_SnapshotTypeDef sc;
    bool shutdown_requested;
    bool bq_online;
    bool config_valid;
    bool cells_valid;
    bool temp_valid;
    bool charger_present;
    bool input_ok;
    bool sc_ok;
    bool charge_temp_ok;
    bool discharge_temp_ok;
    bool discharge_over_current;
    bool completion_now;

    taskENTER_CRITICAL();
    shutdown_requested = s_bq_shutdown_requested;
    s_bq_shutdown_requested = false;
    taskEXIT_CRITICAL();
    if (shutdown_requested)
    {
        (void)App_Power_ExecuteBqShutdown();
        return;
    }

    App_SC8815_GetSnapshot(&sc);
    App_Power_RefreshObserved();
    bq_online = App_BatMan_IsOnline();
    config_valid = App_BatMan_IsConfigValid();
    cells_valid = bq_online && (cell_min_mv >= APP_BATMAN_CELL_VALID_MIN_MV) &&
                  (cell_max_mv <= APP_BATMAN_CELL_VALID_MAX_MV);
    temp_valid = App_BatMan_IsCellTemperatureValid();
    sc_ok = sc.comm_ok && !sc.vbus_short && !sc.otp &&
            (sc.sample_age_ms <= APP_POWER_SC_SAMPLE_MAX_AGE_MS);
    charger_present = sc_ok && sc.ac_ok;
    input_ok = sc_ok && (charger_present || (sc.vbus_mv >= APP_POWER_SC_INPUT_VALID_MV));

    if (App_Power_HandleShutdownState(interval_ms, bq_online, input_ok, sc_ok))
    {
        return;
    }

    if (bq_online && ((battery_status & BQ76952_BATTERY_STATUS_SDM_MASK) != 0u))
    {
        (void)App_Power_ForceOutputsOff();
        s_power_state = APP_POWER_STATE_FAULT;
        s_charge_allowed = false;
        s_discharge_allowed = false;
        s_stop_reason = APP_POWER_STOP_BQ_OFFLINE;
        return;
    }
    if (!bq_online)
    {
        /* 通信失败不能只改软件 allowed 标志；每周期都要有界尝试 BQ 主 FET 全关。 */
        (void)App_Power_ForceOutputsOff();
        s_power_state = APP_POWER_STATE_FAULT;
        s_charge_allowed = false;
        s_discharge_allowed = false;
        s_stop_reason = APP_POWER_STOP_BQ_OFFLINE;
        return;
    }
    if (!config_valid)
    {
        (void)App_Power_ForceOutputsOff();
        s_power_state = APP_POWER_STATE_FAULT;
        s_charge_allowed = false;
        s_discharge_allowed = false;
        s_stop_reason = APP_POWER_STOP_CONFIG_INVALID;
        /* 重新认证成功后也必须等下一帧 BatMan readiness，当前周期绝不继续放行。 */
        (void)App_BatMan_ReauthenticateAfterReset();
        return;
    }
    if (!cells_valid)
    {
        App_Power_StopScSynchronously();
        s_power_state = APP_POWER_STATE_FAULT;
        s_charge_allowed = false;
        s_discharge_allowed = false;
        s_stop_reason = APP_POWER_STOP_CELL_SAMPLE_INVALID;
        (void)App_Power_SetOutput(false, false, false, s_charge_limit_ma);
        return;
    }
    if (!temp_valid)
    {
        App_Power_StopScSynchronously();
        s_power_state = APP_POWER_STATE_FAULT;
        s_charge_allowed = false;
        s_discharge_allowed = false;
        s_stop_reason = APP_POWER_STOP_CELL_TEMP_INVALID;
        (void)App_Power_SetOutput(false, false, false, s_charge_limit_ma);
        return;
    }

    charge_temp_ok =
        (temp_cell_c >= APP_POWER_CHARGE_TEMP_MIN_C) && (temp_cell_c < APP_POWER_CHARGE_TEMP_MAX_C);
    discharge_temp_ok = (temp_cell_c >= APP_POWER_DISCHARGE_TEMP_MIN_C) &&
                        (temp_cell_c <= APP_POWER_DISCHARGE_TEMP_MAX_C);
    s_charge_limit_ma = App_Power_SelectChargeLimitMa(temp_cell_c);
    discharge_over_current = current_ma < -APP_POWER_DISCHARGE_CURRENT_MA;

    if ((safety_status_a & APP_POWER_BQ_SAFETY_A_SCD_MASK) != 0u)
    {
        s_discharge_scd_latched = true;
    }

    completion_now = charger_present && s_observed_sc_charge &&
                     (cell_max_mv >= APP_POWER_CELL_FULL_STOP_MV) &&
                     (cell_min_mv >= APP_POWER_FULL_CONFIRM_MIN_CELL_MV) &&
                     (cell_delta_mv <= APP_POWER_FULL_CONFIRM_MAX_DELTA_MV) && (current_ma >= 0) &&
                     (sc.eoc || (current_ma <= APP_POWER_ANCHOR_CONFIRM_CURRENT_MA));
    if (cell_max_mv >= APP_POWER_CELL_FULL_STOP_MV)
    {
        s_charge_full_latched = true;
        s_top_balance_latched = false;
        if (completion_now && !s_charge_complete_latched)
        {
            s_charge_complete_latched = true;
            s_full_anchor_pending = true;
            s_full_anchor_confirm_ms = 0u;
        }
    }
    else if (s_charge_full_latched && (cell_max_mv <= APP_POWER_CELL_FULL_RESUME_MV))
    {
        s_charge_full_latched = false;
        s_charge_complete_latched = false;
    }
    if (!s_charge_full_latched && !s_top_balance_latched &&
        (cell_max_mv >= APP_POWER_TOP_BALANCE_STOP_MV) &&
        (cell_delta_mv >= APP_POWER_TOP_BALANCE_START_DELTA_MV))
    {
        s_top_balance_latched = true;
    }
    else if (s_top_balance_latched && ((cell_max_mv <= APP_POWER_TOP_BALANCE_RESUME_MV) ||
                                       (cell_delta_mv <= APP_POWER_TOP_BALANCE_STOP_DELTA_MV)))
    {
        s_top_balance_latched = false;
    }

    if (fault_active || !sc_ok || s_discharge_scd_latched)
    {
        s_power_state = APP_POWER_STATE_FAULT;
        s_charge_allowed = false;
        s_discharge_allowed = false;
        s_stop_reason = s_discharge_scd_latched
                            ? APP_POWER_STOP_SCD_LATCHED
                            : (!sc_ok ? APP_POWER_STOP_SC_FAULT : APP_POWER_STOP_BQ_FAULT);
        (void)App_Power_SetOutput(false, false, false, s_charge_limit_ma);
    }
    else if ((cell_min_mv <= APP_POWER_CELL_LOW_MV) || discharge_over_current)
    {
        if ((s_desired_discharge_fet || s_observed_discharge_fet) &&
            (cell_min_mv <= APP_POWER_CELL_LOW_MV))
        {
            s_empty_anchor_pending = true;
            s_empty_anchor_confirm_ms = 0u;
        }
        s_power_state = APP_POWER_STATE_LOW;
        s_charge_allowed = charger_present && charge_temp_ok && !s_charge_inhibit &&
                           !s_charge_full_latched && !s_top_balance_latched;
        s_discharge_allowed = false;
        s_stop_reason = (cell_min_mv <= APP_POWER_CELL_LOW_MV)
                            ? APP_POWER_STOP_LOW_CELL
                            : APP_POWER_STOP_DISCHARGE_OVERCURRENT;
        (void)App_Power_SetOutput(true, false, s_charge_allowed, s_charge_limit_ma);
    }
    else if (cell_min_rc_mv < APP_POWER_CELL_RECOVER_MV)
    {
        s_power_state = APP_POWER_STATE_MONITOR;
        s_charge_allowed = charger_present && charge_temp_ok && !s_charge_inhibit &&
                           !s_charge_full_latched && !s_top_balance_latched;
        s_discharge_allowed = false;
        s_stop_reason = APP_POWER_STOP_LOW_CELL;
        (void)App_Power_SetOutput(true, false, s_charge_allowed, s_charge_limit_ma);
    }
    else
    {
        s_power_state = APP_POWER_STATE_RUN;
        s_charge_allowed = charger_present && charge_temp_ok && !s_charge_inhibit &&
                           !s_charge_full_latched && !s_top_balance_latched;
        s_discharge_allowed = discharge_temp_ok;
        if (!charge_temp_ok || !discharge_temp_ok)
        {
            s_stop_reason = APP_POWER_STOP_CELL_TEMP_RANGE;
        }
        else if (s_charge_full_latched)
        {
            s_stop_reason = s_charge_complete_latched ? APP_POWER_STOP_CHARGE_COMPLETE
                                                      : APP_POWER_STOP_CHARGE_VOLTAGE_LIMIT;
        }
        else if (s_top_balance_latched)
        {
            s_stop_reason = APP_POWER_STOP_TOP_BALANCE;
        }
        else if (s_charge_inhibit)
        {
            s_stop_reason = APP_POWER_STOP_CHARGE_INHIBITED;
        }
        else if (!charger_present)
        {
            s_stop_reason = APP_POWER_STOP_NO_INPUT;
        }
        else
        {
            s_stop_reason = APP_POWER_STOP_NONE;
        }

        if (!s_discharge_allowed)
        {
            s_discharge_rearm_required = charger_present;
            (void)App_Power_SetOutput(true, false, s_charge_allowed, s_charge_limit_ma);
        }
        else if (charger_present)
        {
            s_discharge_rearm_required = true;
            (void)App_Power_SetOutput(true, true, s_charge_allowed, s_charge_limit_ma);
        }
        else if (s_discharge_rearm_required)
        {
            s_discharge_rearm_required = false;
            (void)App_Power_SetOutput(true, false, false, s_charge_limit_ma);
        }
        else
        {
            (void)App_Power_SetOutput(true, true, false, s_charge_limit_ma);
        }
    }

    App_Power_ConfirmSocAnchors(interval_ms, cells_valid);
}

void App_Power_PrintSnapshot(void)
{
    Int_Log_Printf("---------- 电源详细 ----------\r\n");
    Int_Log_Printf(
        "状态:%u 停因:%u 允许充/放:%u/%u shutdown "
        "host/cmd/offline/input/auth/pulse/cfg/frame:%u/%u/%u/%u/%u/%u/%u/%u wake:%lums\r\n",
        (unsigned int)s_power_state,
        (unsigned int)s_stop_reason,
        s_charge_allowed ? 1u : 0u,
        s_discharge_allowed ? 1u : 0u,
        s_shutdown.host_requested ? 1u : 0u,
        s_shutdown.shutdown_command_succeeded ? 1u : 0u,
        s_shutdown.expected_offline_seen ? 1u : 0u,
        s_shutdown.valid_input_seen ? 1u : 0u,
        s_shutdown.wake_authorization_issued ? 1u : 0u,
        s_shutdown.wake_pulse_started ? 1u : 0u,
        s_shutdown.recovery_config_succeeded ? 1u : 0u,
        s_shutdown.post_recovery_frame_seen ? 1u : 0u,
        (unsigned long)s_bq_wake_ms);
}

void App_Power_PrintStopReason(void)
{
    Int_Log_Printf("电源停因:%u 状态:%u Cell:%u/%u Temp:%d Config:%u TempValid:%u\r\n",
                   (unsigned int)s_stop_reason,
                   (unsigned int)s_power_state,
                   (unsigned int)cell_min_mv,
                   (unsigned int)cell_max_mv,
                   temp_cell_c,
                   App_BatMan_IsConfigValid() ? 1u : 0u,
                   App_BatMan_IsCellTemperatureValid() ? 1u : 0u);
}

bool App_Power_ClearDischargeFault(void)
{
    if ((safety_status_a & APP_POWER_BQ_SAFETY_A_SCD_MASK) != 0u)
    {
        return false;
    }
    s_discharge_scd_latched = false;
    s_discharge_rearm_required = false;
    return true;
}

App_Power_StateTypeDef App_Power_GetState(void)
{
    return s_power_state;
}
bool App_Power_IsDischargeAllowed(void)
{
    return s_discharge_allowed;
}
App_Power_StopReasonTypeDef App_Power_GetStopReason(void)
{
    return s_stop_reason;
}

void App_Power_GetShutdownProvenance(App_Power_ShutdownProvenanceTypeDef *provenance)
{
    if (provenance != NULL)
    {
        *provenance = s_shutdown;
    }
}
