#include "App_Power.h"

#include <stdio.h>

#include "App_BatMan.h"
#include "App_DebugCli.h"
#include "App_SC8815.h"

#define APP_POWER_DISCHARGE_CURRENT_MA          (12000)
#define APP_POWER_DISCHARGE_OVER_MARGIN_MA      (0)
#define APP_POWER_CELL_LOW_MV                   (3000u)
#define APP_POWER_CELL_RECOVER_MV               (3200u)
#define APP_POWER_CELL_FULL_STOP_MV             (4200u)
#define APP_POWER_CELL_FULL_RESUME_MV           (4180u)
#define APP_POWER_TOP_BALANCE_STOP_MV           (4180u)
#define APP_POWER_TOP_BALANCE_RESUME_MV         (4150u)
#define APP_POWER_TOP_BALANCE_START_DELTA_MV    (40u)
#define APP_POWER_TOP_BALANCE_STOP_DELTA_MV     (20u)
#define APP_POWER_CHARGE_TEMP_MIN_C             (0)
#define APP_POWER_CHARGE_TEMP_MAX_C             (45)
#define APP_POWER_DISCHARGE_TEMP_MIN_C          (-20)
#define APP_POWER_DISCHARGE_TEMP_MAX_C          (60)
#define APP_POWER_SC_INPUT_VALID_MV             (12000u)
#define APP_POWER_BQ_WAKE_TIMEOUT_MS            (60000u)
#define APP_POWER_PREDISCHARGE_TIME_MS          (5000u)
#define APP_POWER_BQ_SAFETY_A_SCD_MASK          (0x80u)
#define APP_POWER_DEBUG_PERIOD_MS               (5000u)
typedef enum
{
    APP_POWER_CHARGE_STOP_NONE = 0,
    APP_POWER_CHARGE_STOP_FULL,
    APP_POWER_CHARGE_STOP_TOP_BALANCE
} App_Power_ChargeStopReasonTypeDef;

static App_Power_StateTypeDef s_power_state;
static bool s_charge_allowed;
static bool s_discharge_allowed;
static bool s_output_charge;
static bool s_output_discharge;
static bool s_output_predischarge;
static bool s_output_sc_charge;
static bool s_output_synced;
static bool s_charge_full_latched;
static App_Power_ChargeStopReasonTypeDef s_charge_stop_reason;
static bool s_discharge_scd_latched;
static bool s_low_power_sound_played;
static uint32_t s_bq_wake_ms;
static uint16_t s_predischarge_ms;
static uint16_t s_debug_ms;

static void App_Power_SetOutput(bool charge_fet_enable,
                                bool discharge_enable,
                                bool sc_charge_enable)
{
    sc_charge_enable = charge_fet_enable && sc_charge_enable;

    if (s_output_synced &&
        (s_output_charge == charge_fet_enable) &&
        (s_output_discharge == discharge_enable) &&
        (s_output_sc_charge == sc_charge_enable) &&
        !s_output_predischarge)
    {
        return;
    }

    /*
     * BQ CHG MOS 是电池包功率路径，SC 请求是充电功率级开关。
     * 无充电器或策略停充时，BQ CHG 可保持打开，但 SC 不误启动。
     */
    if (!sc_charge_enable)
    {
        App_SC8815_RequestCharge(false);
    }

    if (!App_BatMan_SetMainFets(charge_fet_enable, discharge_enable))
    {
        App_SC8815_RequestCharge(false);
        s_charge_allowed = false;
        s_discharge_allowed = false;
        s_output_synced = false;
        s_power_state = APP_POWER_STATE_FAULT;
        return;
    }

    App_SC8815_RequestCharge(sc_charge_enable);
    s_output_charge = charge_fet_enable;
    s_output_discharge = discharge_enable;
    s_output_predischarge = false;
    s_output_sc_charge = sc_charge_enable;
    s_output_synced = true;
}

static void App_Power_SetScWakeCharge(bool charge_enable)
{
    /*
     * BQ shutdown 后 REG18 掉电，主控虽然可由 24V 输入供电，但 BQ I2C
     * 可能完全无响应。此时不能写 BQ FET，只允许 SC8815 先把 BMS+ 拉高，
     * 等 BQ 通过 LD/电压恢复被唤醒后再回到正常闭环。
     */
    App_SC8815_RequestCharge(charge_enable);
    s_output_synced = false;
}

static void App_Power_PrintSeparator(const char *title)
{
    printf("---------- %s ----------\r\n", title);
}

static void App_Power_PrintSummary(void)
{
    printf("电源 摘要 状态:%u 充:%u 放:%u 预:%u SCD锁存:%u 电芯:%u/%u 压差:%u 电流:%ldmA VBUS:%lu\r\n",
           (unsigned int)s_power_state,
           s_charge_allowed ? 1u : 0u,
           s_discharge_allowed ? 1u : 0u,
           s_output_predischarge ? 1u : 0u,
           s_discharge_scd_latched ? 1u : 0u,
           (unsigned int)cell_min_mv,
           (unsigned int)cell_max_mv,
           (unsigned int)cell_delta_mv,
           (long)current_ma,
           (unsigned long)App_SC8815_GetVbusMv());
}

static void App_Power_PrintDebug(void)
{
    App_Power_PrintSeparator("电源");
    App_Power_PrintSummary();
}

void App_Power_PrintSnapshot(void)
{
    App_Power_PrintSeparator("电源详细");
    App_Power_PrintSummary();
    printf("电源细节 输出同步:%u 输出充:%u 输出放:%u 预放输出:%u 预放计时:%u/%u ms BQ唤醒:%lu/%lu ms 充电停因:%u SCD锁存:%u 软件放电限流:%u mA\r\n",
           s_output_synced ? 1u : 0u,
           s_output_charge ? 1u : 0u,
           s_output_discharge ? 1u : 0u,
           s_output_predischarge ? 1u : 0u,
           (unsigned int)s_predischarge_ms,
           (unsigned int)APP_POWER_PREDISCHARGE_TIME_MS,
           (unsigned long)s_bq_wake_ms,
           (unsigned long)APP_POWER_BQ_WAKE_TIMEOUT_MS,
           (unsigned int)s_charge_stop_reason,
           s_discharge_scd_latched ? 1u : 0u,
           (unsigned int)APP_POWER_DISCHARGE_CURRENT_MA);
}

static void App_Power_PrintReasonFlag(bool active, const char *text)
{
    if (active)
    {
        printf("BQFAST原因 %s\r\n", text);
    }
}

void App_Power_PrintStopReason(void)
{
    const bool cell_ok = ((cell_min_mv >= APP_BATMAN_CELL_VALID_MIN_MV) &&
                          (cell_max_mv <= APP_BATMAN_CELL_VALID_MAX_MV));
    const bool discharge_over_current =
        (current_ma < -(APP_POWER_DISCHARGE_CURRENT_MA + APP_POWER_DISCHARGE_OVER_MARGIN_MA));
    const bool discharge_temp_ok = ((temp_cell_c >= APP_POWER_DISCHARGE_TEMP_MIN_C) &&
                                    (temp_cell_c <= APP_POWER_DISCHARGE_TEMP_MAX_C));

    App_Power_PrintSeparator("BQFAST停表-电源原因");
    printf("BQFAST电源 状态:%u 充:%u 放:%u 预:%u SCD锁存:%u I:%ldmA CellMin:%u RcMin:%u CellMax:%u d:%u Temp:%d 限流:%u\r\n",
           (unsigned int)s_power_state,
           s_charge_allowed ? 1u : 0u,
           s_discharge_allowed ? 1u : 0u,
           s_output_predischarge ? 1u : 0u,
           s_discharge_scd_latched ? 1u : 0u,
           (long)current_ma,
           (unsigned int)cell_min_mv,
           (unsigned int)cell_min_rc_mv,
           (unsigned int)cell_max_mv,
           (unsigned int)cell_delta_mv,
           temp_cell_c,
           (unsigned int)APP_POWER_DISCHARGE_CURRENT_MA);

    App_Power_PrintReasonFlag(!cell_ok, "电芯采样不在有效范围，电源状态机不允许继续放电");
    App_Power_PrintReasonFlag(fault_active, "BQ/APP总故障置位，电源状态机进入FAULT");
    App_Power_PrintReasonFlag(s_discharge_scd_latched, "APP已经锁存SCD，需排查后发送 fault clear");
    App_Power_PrintReasonFlag(cell_min_mv <= APP_POWER_CELL_LOW_MV, "最低单体达到低电压阈值，APP关闭放电");
    App_Power_PrintReasonFlag((s_power_state == APP_POWER_STATE_MONITOR) &&
                              (cell_min_rc_mv < APP_POWER_CELL_RECOVER_MV),
                              "RC补偿最低单体低于放电恢复阈值3200mV，APP进入MONITOR并关闭放电");
    App_Power_PrintReasonFlag(discharge_over_current, "电流超过APP软件放电限流，APP关闭放电");
    App_Power_PrintReasonFlag(!discharge_temp_ok, "放电温度不在允许范围，APP关闭放电");
    App_Power_PrintReasonFlag(!s_discharge_allowed, "电源状态机当前不允许放电");
}

bool App_Power_ClearDischargeFault(void)
{
    if ((safety_status_a & APP_POWER_BQ_SAFETY_A_SCD_MASK) != 0u)
    {
        return false;
    }

    s_discharge_scd_latched = false;
    s_predischarge_ms = 0u;
    return true;
}

void App_Power_Init(void)
{
    s_power_state = APP_POWER_STATE_OFF;
    s_charge_allowed = false;
    s_discharge_allowed = false;
    s_output_charge = false;
    s_output_discharge = false;
    s_output_predischarge = false;
    s_output_sc_charge = false;
    s_output_synced = false;
    s_charge_full_latched = false;
    s_charge_stop_reason = APP_POWER_CHARGE_STOP_NONE;
    s_discharge_scd_latched = false;
    s_low_power_sound_played = false;
    s_bq_wake_ms = 0u;
    s_predischarge_ms = 0u;
    s_debug_ms = 0u;

    App_SC8815_RequestCharge(false);
    if ((cell_min_mv >= APP_BATMAN_CELL_VALID_MIN_MV) &&
        (cell_max_mv <= APP_BATMAN_CELL_VALID_MAX_MV) &&
        !fault_active)
    {
        if (App_BatMan_SetMainFets(true, true))
        {
            s_output_charge = true;
            s_output_discharge = true;
            s_output_predischarge = false;
            s_output_sc_charge = false;
            s_output_synced = true;
        }
    }
}

void App_Power_Task(uint32_t interval_ms)
{
    bool cell_ok;
    bool input_ok;
    bool sc_charge_ok;
    bool charge_temp_ok;
    bool charge_voltage_ok;
    bool discharge_temp_ok;
    bool discharge_over_current;

    cell_ok = ((cell_min_mv >= APP_BATMAN_CELL_VALID_MIN_MV) &&
               (cell_max_mv <= APP_BATMAN_CELL_VALID_MAX_MV));
    input_ok = (App_SC8815_IsAcOk() ||
                (App_SC8815_GetVbusMv() >= APP_POWER_SC_INPUT_VALID_MV));
    sc_charge_ok = !App_SC8815_HasFault();
    charge_temp_ok = ((temp_cell_c >= APP_POWER_CHARGE_TEMP_MIN_C) &&
                      (temp_cell_c <= APP_POWER_CHARGE_TEMP_MAX_C));
    if (cell_ok)
    {
        /*
         * 顶端均衡阶段不能继续把最高串硬顶到过压保护。
         * 最高串接近满电且压差仍大时先停充，保留 BQ host 均衡去拉低最高串；
         * 真正碰到 4.20V 满停后只按电压回差恢复，避免压差刚均好又贴着过压点补电。
         */
        if (cell_max_mv >= APP_POWER_CELL_FULL_STOP_MV)
        {
            s_charge_full_latched = true;
            s_charge_stop_reason = APP_POWER_CHARGE_STOP_FULL;
        }
        else if ((s_charge_stop_reason == APP_POWER_CHARGE_STOP_FULL) &&
                 (cell_max_mv <= APP_POWER_CELL_FULL_RESUME_MV))
        {
            s_charge_full_latched = false;
            s_charge_stop_reason = APP_POWER_CHARGE_STOP_NONE;
        }
        else if ((s_charge_stop_reason == APP_POWER_CHARGE_STOP_TOP_BALANCE) &&
                 ((cell_max_mv <= APP_POWER_TOP_BALANCE_RESUME_MV) ||
                  (cell_delta_mv <= APP_POWER_TOP_BALANCE_STOP_DELTA_MV)))
        {
            s_charge_full_latched = false;
            s_charge_stop_reason = APP_POWER_CHARGE_STOP_NONE;
        }
        else if ((s_charge_stop_reason == APP_POWER_CHARGE_STOP_NONE) &&
                 (cell_max_mv >= APP_POWER_TOP_BALANCE_STOP_MV) &&
                 (cell_delta_mv >= APP_POWER_TOP_BALANCE_START_DELTA_MV))
        {
            s_charge_full_latched = true;
            s_charge_stop_reason = APP_POWER_CHARGE_STOP_TOP_BALANCE;
        }
    }
    else
    {
        s_charge_full_latched = false;
        s_charge_stop_reason = APP_POWER_CHARGE_STOP_NONE;
    }
    charge_voltage_ok = !s_charge_full_latched;
    discharge_temp_ok = ((temp_cell_c >= APP_POWER_DISCHARGE_TEMP_MIN_C) &&
                         (temp_cell_c <= APP_POWER_DISCHARGE_TEMP_MAX_C));

    /*
     * 当前约定 current_ma > 0 为充电，current_ma < 0 为放电。
     * 电子负载/SCD 测试时 APP 软件限流抬高，避免先于 BQ 硬件 SCD 关断。
     */
    discharge_over_current =
        (current_ma < -(APP_POWER_DISCHARGE_CURRENT_MA + APP_POWER_DISCHARGE_OVER_MARGIN_MA));

    if ((safety_status_a & APP_POWER_BQ_SAFETY_A_SCD_MASK) != 0u)
    {
        if (!s_discharge_scd_latched)
        {
            printf("电源 放电SCD锁存: 检测到BQ短路保护，停止自动重试，请排查后发送 fault clear\r\n");
        }
        s_discharge_scd_latched = true;
    }

    if (!cell_ok)
    {
        /*
         * BQ 可能已经因低压进入 shutdown：cell 全 0 或通信失败不能直接禁止
         * SC8815，否则插入 24V 后无法给 BMS+ 预充，也就唤不醒 BQ。
        */
        s_discharge_allowed = false;
        s_predischarge_ms = 0u;
        s_charge_allowed = input_ok && sc_charge_ok;
        if (s_charge_allowed)
        {
            if (s_power_state != APP_POWER_STATE_BQ_WAKE)
            {
                s_bq_wake_ms = 0u;
            }
            else if (s_bq_wake_ms < APP_POWER_BQ_WAKE_TIMEOUT_MS)
            {
                s_bq_wake_ms += interval_ms;
            }

            if (s_bq_wake_ms < APP_POWER_BQ_WAKE_TIMEOUT_MS)
            {
                s_power_state = APP_POWER_STATE_BQ_WAKE;
                App_Power_SetScWakeCharge(true);
            }
            else
            {
                s_power_state = APP_POWER_STATE_FAULT;
                s_charge_allowed = false;
                App_Power_SetScWakeCharge(false);
            }
        }
        else
        {
            s_power_state = APP_POWER_STATE_FAULT;
            App_Power_SetScWakeCharge(false);
        }
    }
    else if (fault_active)
    {
        s_bq_wake_ms = 0u;
        s_predischarge_ms = 0u;
        s_power_state = APP_POWER_STATE_FAULT;
        s_charge_allowed = false;
        s_discharge_allowed = false;
        s_low_power_sound_played = false;
        App_Power_SetOutput(false, false, false);
    }
    else if (s_discharge_scd_latched)
    {
        s_bq_wake_ms = 0u;
        s_predischarge_ms = 0u;
        s_power_state = APP_POWER_STATE_FAULT;
        s_charge_allowed = false;
        s_discharge_allowed = false;
        App_Power_SetOutput(false, false, false);
    }
    else if ((cell_min_mv <= APP_POWER_CELL_LOW_MV) || discharge_over_current)
    {
        s_bq_wake_ms = 0u;
        s_predischarge_ms = 0u;
        s_power_state = APP_POWER_STATE_LOW;
        if (!s_low_power_sound_played)
        {
            s_low_power_sound_played = true;
        }
        s_charge_allowed = input_ok && sc_charge_ok && charge_temp_ok && charge_voltage_ok;
        s_discharge_allowed = false;
        App_Power_SetOutput(true, s_discharge_allowed, s_charge_allowed);
    }
    else if (cell_min_rc_mv < APP_POWER_CELL_RECOVER_MV)
    {
        s_bq_wake_ms = 0u;
        s_predischarge_ms = 0u;
        s_power_state = APP_POWER_STATE_MONITOR;
        s_charge_allowed = input_ok && sc_charge_ok && charge_temp_ok && charge_voltage_ok;
        s_discharge_allowed = false;
        App_Power_SetOutput(true, s_discharge_allowed, s_charge_allowed);
    }
    else
    {
        s_bq_wake_ms = 0u;
        s_predischarge_ms = APP_POWER_PREDISCHARGE_TIME_MS;
        s_power_state = APP_POWER_STATE_RUN;
        s_low_power_sound_played = false;
        s_charge_allowed = input_ok &&
                           sc_charge_ok &&
                           charge_temp_ok &&
                           charge_voltage_ok;
        s_discharge_allowed = discharge_temp_ok;
        App_Power_SetOutput(true, s_discharge_allowed, s_charge_allowed);
    }

    if (!App_DebugCli_IsVofaStreaming() && !App_DebugCli_IsBqMonitoring())
    {
        s_debug_ms = (uint16_t)(s_debug_ms + interval_ms);
        if (s_debug_ms >= APP_POWER_DEBUG_PERIOD_MS)
        {
            s_debug_ms = 0u;
            App_Power_PrintDebug();
        }
    }
}

App_Power_StateTypeDef App_Power_GetState(void)
{
    return s_power_state;
}

bool App_Power_IsDischargeAllowed(void)
{
    return s_discharge_allowed;
}
