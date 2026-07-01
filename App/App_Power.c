#include "App_Power.h"

#include <stdio.h>

#include "App_BatMan.h"
#include "App_Buzzer.h"
#include "App_SC8815.h"

#define APP_POWER_CHARGE_CURRENT_MA             (1000)
#define APP_POWER_DISCHARGE_CURRENT_MA          (1000)
#define APP_POWER_DISCHARGE_OVER_MARGIN_MA      (0)
#define APP_POWER_CELL_LOW_MV                   (3000u)
#define APP_POWER_CELL_RECOVER_MV               (3200u)
#define APP_POWER_CELL_FULL_STOP_MV             (4200u)
#define APP_POWER_CELL_FULL_RESUME_MV           (4180u)
#define APP_POWER_CHARGE_TEMP_MIN_C             (0)
#define APP_POWER_CHARGE_TEMP_MAX_C             (45)
#define APP_POWER_DISCHARGE_TEMP_MIN_C          (-20)
#define APP_POWER_DISCHARGE_TEMP_MAX_C          (60)
#define APP_POWER_SC_INPUT_VALID_MV             (12000u)
#define APP_POWER_BQ_WAKE_TIMEOUT_MS            (60000u)
#define APP_POWER_PREDISCHARGE_TIME_MS          (5000u)
#define APP_POWER_BQ_SAFETY_A_SCD_MASK          (0x80u)
#define APP_POWER_BUZZER_ENABLE                 0u
#define APP_POWER_DEBUG_PERIOD_MS               (1000u)
#define APP_POWER_CHARGE_ONLY_TEST_ENABLE       1u

static App_Power_StateTypeDef s_power_state;
static bool s_charge_allowed;
static bool s_discharge_allowed;
static bool s_output_charge;
static bool s_output_discharge;
static bool s_output_predischarge;
static bool s_output_synced;
static bool s_charge_full_latched;
static bool s_low_power_sound_played;
static bool s_predischarge_done_printed;
static uint32_t s_bq_wake_ms;
static uint16_t s_predischarge_ms;
static uint16_t s_debug_ms;

static void App_Power_SetOutput(bool charge_enable, bool discharge_enable)
{
    if (s_output_synced &&
        (s_output_charge == charge_enable) &&
        (s_output_discharge == discharge_enable) &&
        !s_output_predischarge)
    {
        return;
    }

    /*
     * 关闭充电请求先于 BQ FET 动作，保证故障/低电时 SC8815 不继续释放功率环路。
     */
    if (!charge_enable)
    {
        App_SC8815_RequestCharge(false);
    }

    if (!App_BatMan_SetMainFets(charge_enable, discharge_enable))
    {
        App_SC8815_RequestCharge(false);
        s_charge_allowed = false;
        s_discharge_allowed = false;
        s_output_synced = false;
        s_power_state = APP_POWER_STATE_FAULT;
        return;
    }

    App_SC8815_RequestCharge(charge_enable);
    s_output_charge = charge_enable;
    s_output_discharge = discharge_enable;
    s_output_predischarge = false;
    s_output_synced = true;
}

static void App_Power_SetPreDischarge(bool charge_enable)
{
    if (s_output_synced &&
        (s_output_charge == charge_enable) &&
        !s_output_discharge &&
        s_output_predischarge)
    {
        return;
    }

    if (!charge_enable)
    {
        App_SC8815_RequestCharge(false);
    }

    if (!App_BatMan_SetPreDischargeFet(charge_enable))
    {
        App_SC8815_RequestCharge(false);
        s_charge_allowed = false;
        s_discharge_allowed = false;
        s_output_synced = false;
        s_power_state = APP_POWER_STATE_FAULT;
        return;
    }

    App_SC8815_RequestCharge(charge_enable);
    s_output_charge = charge_enable;
    s_output_discharge = false;
    s_output_predischarge = true;
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

static void App_Power_PrintDebug(void)
{
    printf("电源 状态:%u 充:%u 放:%u 预:%u 满停:%u 限流:%u/%u mA 电芯:%u/%u 电流:%ld SC_AC:%u SC_VBUS:%lu\r\n",
           (unsigned int)s_power_state,
           s_charge_allowed ? 1u : 0u,
           s_discharge_allowed ? 1u : 0u,
           s_output_predischarge ? 1u : 0u,
           s_charge_full_latched ? 1u : 0u,
           (unsigned int)APP_POWER_CHARGE_CURRENT_MA,
           (unsigned int)APP_POWER_DISCHARGE_CURRENT_MA,
           (unsigned int)cell_min_mv,
           (unsigned int)cell_max_mv,
           (long)current_ma,
           App_SC8815_IsAcOk() ? 1u : 0u,
           (unsigned long)App_SC8815_GetVbusMv());
}

#if APP_POWER_CHARGE_ONLY_TEST_ENABLE
static bool App_Power_RunChargeOnlyTest(bool cell_ok,
                                        bool input_ok,
                                        bool sc_charge_ok,
                                        bool charge_temp_ok,
                                        bool charge_voltage_ok)
{
    if (!cell_ok)
    {
        return false;
    }

    s_bq_wake_ms = 0u;
    s_predischarge_ms = 0u;
    s_predischarge_done_printed = false;
    s_low_power_sound_played = false;

    s_charge_allowed = input_ok && sc_charge_ok && charge_temp_ok && charge_voltage_ok;
    s_discharge_allowed = false;

    if (s_charge_allowed)
    {
        /*
         * 临时联调：只允许 BQ 充电 FET，强制关断 BQ 放电 FET；
         * SC8815 同时发充电请求，用来分辨 BQ 与 SC 哪一侧影响 IBAT。
         */
        s_power_state = APP_POWER_STATE_RUN;
        App_Power_SetOutput(true, false);
    }
    else
    {
        s_power_state = APP_POWER_STATE_MONITOR;
        App_Power_SetOutput(false, false);
    }

    return true;
}
#endif

void App_Power_Init(void)
{
    s_power_state = APP_POWER_STATE_OFF;
    s_charge_allowed = false;
    s_discharge_allowed = false;
    s_output_charge = false;
    s_output_discharge = false;
    s_output_predischarge = false;
    s_output_synced = false;
    s_charge_full_latched = false;
    s_low_power_sound_played = false;
    s_predischarge_done_printed = false;
    s_bq_wake_ms = 0u;
    s_predischarge_ms = 0u;
    s_debug_ms = 0u;

    App_SC8815_RequestCharge(false);
    if ((cell_min_mv >= APP_BATMAN_CELL_VALID_MIN_MV) &&
        (cell_max_mv <= APP_BATMAN_CELL_VALID_MAX_MV))
    {
        (void)App_BatMan_AllMainFetsOff();
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
        if (cell_max_mv >= APP_POWER_CELL_FULL_STOP_MV)
        {
            s_charge_full_latched = true;
        }
        else if (cell_max_mv <= APP_POWER_CELL_FULL_RESUME_MV)
        {
            s_charge_full_latched = false;
        }
    }
    else
    {
        s_charge_full_latched = false;
    }
    charge_voltage_ok = !s_charge_full_latched;
    discharge_temp_ok = ((temp_cell_c >= APP_POWER_DISCHARGE_TEMP_MIN_C) &&
                         (temp_cell_c <= APP_POWER_DISCHARGE_TEMP_MAX_C));

    /*
     * 当前约定 current_ma > 0 为充电，current_ma < 0 为放电。
     * BQ 主放电 MOS 不是线性限流器，1A 放电只能靠采样后超限关断。
     */
    discharge_over_current =
        (current_ma < -(APP_POWER_DISCHARGE_CURRENT_MA + APP_POWER_DISCHARGE_OVER_MARGIN_MA));

#if APP_POWER_CHARGE_ONLY_TEST_ENABLE
    if (App_Power_RunChargeOnlyTest(cell_ok,
                                    input_ok,
                                    sc_charge_ok,
                                    charge_temp_ok,
                                    charge_voltage_ok))
    {
        s_debug_ms = (uint16_t)(s_debug_ms + interval_ms);
        if (s_debug_ms >= APP_POWER_DEBUG_PERIOD_MS)
        {
            s_debug_ms = 0u;
            App_Power_PrintDebug();
        }
        return;
    }
#endif

    if (!cell_ok)
    {
        /*
         * BQ 可能已经因低压进入 shutdown：cell 全 0 或通信失败不能直接禁止
         * SC8815，否则插入 24V 后无法给 BMS+ 预充，也就唤不醒 BQ。
        */
        s_discharge_allowed = false;
        s_predischarge_ms = 0u;
        s_predischarge_done_printed = false;
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
        s_predischarge_done_printed = false;
        s_power_state = APP_POWER_STATE_FAULT;
        s_charge_allowed = false;
        s_discharge_allowed = false;
        s_low_power_sound_played = false;
        App_Power_SetOutput(false, false);
    }
    else if ((cell_min_mv <= APP_POWER_CELL_LOW_MV) || discharge_over_current)
    {
        s_bq_wake_ms = 0u;
        s_predischarge_ms = 0u;
        s_predischarge_done_printed = false;
        s_power_state = APP_POWER_STATE_LOW;
        if (!s_low_power_sound_played)
        {
            s_low_power_sound_played = true;
#if APP_POWER_BUZZER_ENABLE
            App_Buzzer_PlayLowPower();
#endif
        }
        s_charge_allowed = input_ok && sc_charge_ok && charge_temp_ok && charge_voltage_ok;
        s_discharge_allowed = false;
        App_Power_SetOutput(s_charge_allowed, s_discharge_allowed);
    }
    else if (cell_min_mv < APP_POWER_CELL_RECOVER_MV)
    {
        s_bq_wake_ms = 0u;
        s_predischarge_ms = 0u;
        s_predischarge_done_printed = false;
        s_power_state = APP_POWER_STATE_MONITOR;
        s_charge_allowed = input_ok && sc_charge_ok && charge_temp_ok && charge_voltage_ok;
        s_discharge_allowed = false;
        App_Power_SetOutput(s_charge_allowed, s_discharge_allowed);
    }
    else
    {
        s_bq_wake_ms = 0u;
        s_power_state = APP_POWER_STATE_RUN;
        s_low_power_sound_played = false;
        s_charge_allowed = input_ok &&
                           sc_charge_ok &&
                           charge_temp_ok &&
                           charge_voltage_ok;
        s_discharge_allowed = discharge_temp_ok;
        if (s_discharge_allowed && (s_predischarge_ms < APP_POWER_PREDISCHARGE_TIME_MS))
        {
            /*
             * 这里不是 MCU 单独强开 PDSG。BQ 的 PDSG_EN 已经打开，只要主机允许
             * DSG，器件会自动先预放电，再按 LD/超时/压差条件切到主 DSG。
             */
            s_predischarge_ms = (uint16_t)(s_predischarge_ms + interval_ms);
            if (s_predischarge_ms >= APP_POWER_PREDISCHARGE_TIME_MS)
            {
                s_predischarge_ms = APP_POWER_PREDISCHARGE_TIME_MS;
                if (!s_predischarge_done_printed)
                {
                    s_predischarge_done_printed = true;
                    printf("电源 预放电等待完成:%u ms\r\n",
                           (unsigned int)APP_POWER_PREDISCHARGE_TIME_MS);
                }
            }
            App_Power_SetPreDischarge(s_charge_allowed);
        }
        else
        {
            App_Power_SetOutput(s_charge_allowed, s_discharge_allowed);
        }
    }

    s_debug_ms = (uint16_t)(s_debug_ms + interval_ms);
    if (s_debug_ms >= APP_POWER_DEBUG_PERIOD_MS)
    {
        s_debug_ms = 0u;
        App_Power_PrintDebug();
    }
}

App_Power_StateTypeDef App_Power_GetState(void)
{
    return s_power_state;
}

bool App_Power_IsChargeAllowed(void)
{
    return s_charge_allowed;
}

bool App_Power_IsDischargeAllowed(void)
{
    return s_discharge_allowed;
}
