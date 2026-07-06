#include "Bms_PowerService.h"

#include "App_Power.h"

#include <stdio.h>

#include "Bms_BuzzerService.h"
#include "Bms_DebugCli.h"
#include "Bms_Config.h"
#include "Bms_ChargeService.h"
#include "Bms_DischargeService.h"
#include "Bms_Model.h"
#include "Bms_PowerPort.h"
#include "Bms_PreDischargeService.h"
#include "Bms_ProtectionService.h"

#define APP_POWER_CHARGE_CURRENT_MA             (1000)
#define APP_POWER_CHARGE_ONLY_TEST_ENABLE       0u

static App_Power_StateTypeDef s_power_state;
static bool s_charge_allowed;
static bool s_discharge_allowed;
static bool s_output_charge;
static bool s_output_discharge;
static bool s_output_predischarge;
static bool s_output_synced;
static bool s_discharge_scd_latched;
static bool s_low_power_sound_played;
static uint32_t s_bq_wake_ms;
static uint16_t s_debug_ms;
static Bms_ChargeService_StateTypeDef s_charge_state;
static Bms_PreDischargeService_StateTypeDef s_predischarge_state;

static void App_Power_SyncServiceSnapshot(void)
{
    Bms_ContextTypeDef *ctx = Bms_Model_GetMutableContext();
    Bms_ServiceSnapshotTypeDef service_snapshot;

    if (ctx == NULL)
    {
        return;
    }

    service_snapshot = ctx->service;
    service_snapshot.charge_allowed = s_charge_allowed;
    service_snapshot.discharge_allowed = s_discharge_allowed;
    service_snapshot.predischarge_active = s_output_predischarge;
    Bms_Model_SetServiceSnapshot(ctx, &service_snapshot);
}

static void App_Power_SetOutput(bool charge_enable, bool discharge_enable)
{
    if (s_output_synced &&
        (s_output_charge == charge_enable) &&
        (s_output_discharge == discharge_enable) &&
        !s_output_predischarge)
    {
        return;
    }

    if (!Bms_PowerPort_ApplyMainOutput(charge_enable, discharge_enable))
    {
        s_charge_allowed = false;
        s_discharge_allowed = false;
        s_output_synced = false;
        s_power_state = APP_POWER_STATE_FAULT;
        return;
    }

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

    if (!Bms_PowerPort_ApplyPreDischarge(charge_enable))
    {
        s_charge_allowed = false;
        s_discharge_allowed = false;
        s_output_synced = false;
        s_power_state = APP_POWER_STATE_FAULT;
        return;
    }

    s_output_charge = charge_enable;
    s_output_discharge = false;
    s_output_predischarge = true;
    s_output_synced = true;
}

static void App_Power_SetScWakeCharge(bool charge_enable)
{
    /*
     * BQ shutdown �?REG18 掉电，主控虽然可�?24V 输入供电，但 BQ I2C
     * 可能完全无响应。此时不能写 BQ FET，只允许 SC8815 先把 BMS+ 拉高�?
     * �?BQ 通过 LD/电压恢复被唤醒后再回到正常闭环�?
     */
    Bms_PowerPort_SetChargeRequest(charge_enable);
    s_output_synced = false;
}

static void App_Power_PrintSeparator(const char *title)
{
    printf("---------- %s ----------\r\n", title);
}

static void App_Power_PrintSummary(void)
{
    const Bms_ContextTypeDef *ctx = Bms_Model_GetContext();

    printf("电源 摘要 状�?%u �?%u �?%u �?%u SCD锁存:%u 电芯:%u/%u 压差:%u 电流:%ldmA VBUS:%lu\r\n",
           (unsigned int)s_power_state,
           s_charge_allowed ? 1u : 0u,
           s_discharge_allowed ? 1u : 0u,
           s_output_predischarge ? 1u : 0u,
           s_discharge_scd_latched ? 1u : 0u,
           (unsigned int)ctx->pack.min_mv,
           (unsigned int)ctx->pack.max_mv,
           (unsigned int)ctx->pack.delta_mv,
           (long)ctx->pack.current_ma,
           (unsigned long)Bms_PowerPort_GetVbusMv());
}

static void App_Power_PrintDebug(void)
{
    App_Power_PrintSeparator("电源");
    App_Power_PrintSummary();
}

void Bms_PowerService_PrintSnapshot(void)
{
    const Bms_Config_PowerTypeDef *power_config = Bms_Config_GetPowerConfig();
    const Bms_PreDischargeService_ConfigTypeDef *predischarge_config =
        Bms_Config_GetPreDischargeServiceConfig();

    App_Power_PrintSeparator("电源详细");
    App_Power_PrintSummary();
    printf("电源细节 输出同步:%u 输出�?%u 输出�?%u 预放输出:%u 预放计时:%u/%u ms BQ唤醒:%lu/%lu ms 充电停因:%u SCD锁存:%u 软件放电限流:%u mA\r\n",
           s_output_synced ? 1u : 0u,
           s_output_charge ? 1u : 0u,
           s_output_discharge ? 1u : 0u,
           s_output_predischarge ? 1u : 0u,
           (unsigned int)s_predischarge_state.elapsed_ms,
           (unsigned int)predischarge_config->duration_ms,
           (unsigned long)s_bq_wake_ms,
           (unsigned long)power_config->bq_wake_timeout_ms,
           (unsigned int)s_charge_state.stop_reason,
           s_discharge_scd_latched ? 1u : 0u,
           (unsigned int)power_config->discharge_current_ma);
}

static void App_Power_PrintReasonFlag(bool active, const char *text)
{
    if (active)
    {
        printf("BQFAST原因 %s\r\n", text);
    }
}

void Bms_PowerService_PrintStopReason(void)
{
    const Bms_ContextTypeDef *ctx = Bms_Model_GetContext();
    Bms_DischargeService_EvaluationTypeDef discharge_eval;
    const Bms_Config_PowerTypeDef *power_config = Bms_Config_GetPowerConfig();

    (void)Bms_DischargeService_Evaluate(ctx,
                                        Bms_Config_GetDischargeServiceConfig(),
                                        &discharge_eval);

    App_Power_PrintSeparator("BQFAST停表-电源原因");
    printf("BQFAST电源 状�?%u �?%u �?%u �?%u SCD锁存:%u I:%ldmA CellMin:%u RcMin:%u CellMax:%u d:%u Temp:%d 限流:%u\r\n",
           (unsigned int)s_power_state,
           s_charge_allowed ? 1u : 0u,
           s_discharge_allowed ? 1u : 0u,
           s_output_predischarge ? 1u : 0u,
           s_discharge_scd_latched ? 1u : 0u,
           (long)ctx->pack.current_ma,
           (unsigned int)ctx->pack.min_mv,
           (unsigned int)ctx->pack.min_rc_mv,
           (unsigned int)ctx->pack.max_mv,
           (unsigned int)ctx->pack.delta_mv,
           ctx->pack.temp_cell_c,
           (unsigned int)power_config->discharge_current_ma);

    App_Power_PrintReasonFlag(!discharge_eval.cell_sample_ok, "cell sample invalid, discharge blocked");
    App_Power_PrintReasonFlag(ctx->protection.fault_active, "BQ/APP总故障置位，电源状态机进入FAULT");
    App_Power_PrintReasonFlag(s_discharge_scd_latched, "APP已经锁存SCD，需排查后发�?fault clear");
    App_Power_PrintReasonFlag(discharge_eval.low_voltage, "最低单体达到低电压阈值，APP关闭放电");
    App_Power_PrintReasonFlag((s_power_state == APP_POWER_STATE_MONITOR) &&
                              discharge_eval.recover_blocked,
                              "rc recovery margin too low, discharge blocked");
    App_Power_PrintReasonFlag(discharge_eval.over_current, "电流超过APP软件放电限流，APP关闭放电");
    App_Power_PrintReasonFlag(!discharge_eval.temp_ok, "放电温度不在允许范围，APP关闭放电");
    App_Power_PrintReasonFlag(!s_discharge_allowed, "power state blocks discharge");
}

bool Bms_PowerService_ClearDischargeFault(void)
{
    if (Bms_ProtectionService_IsDischargeShortCircuit(Bms_Model_GetContext()))
    {
        return false;
    }

    s_discharge_scd_latched = false;
    Bms_PreDischargeService_Reset(&s_predischarge_state);
    App_Power_SyncServiceSnapshot();
    return true;
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
    Bms_PreDischargeService_Reset(&s_predischarge_state);
    s_low_power_sound_played = false;

    s_charge_allowed = input_ok && sc_charge_ok && charge_temp_ok && charge_voltage_ok;
    s_discharge_allowed = s_charge_allowed;

    if (s_charge_allowed)
    {
        /*
         * 临时联调�?4V 接入后同时释�?BQ CHG/DSG�?
         * 先确�?SC8815 �?VBAT 采样点能否被电池包拉到真实电压�?
         */
        s_power_state = APP_POWER_STATE_RUN;
        App_Power_SetOutput(true, true);
    }
    else
    {
        s_power_state = APP_POWER_STATE_MONITOR;
        App_Power_SetOutput(false, false);
    }

    return true;
}
#endif

void Bms_PowerService_Init(void)
{
    const Bms_ContextTypeDef *ctx = Bms_Model_GetContext();

    s_power_state = APP_POWER_STATE_OFF;
    s_charge_allowed = false;
    s_discharge_allowed = false;
    s_output_charge = false;
    s_output_discharge = false;
    s_output_predischarge = false;
    s_output_synced = false;
    Bms_ChargeService_Reset(&s_charge_state);
    s_discharge_scd_latched = false;
    s_low_power_sound_played = false;
    Bms_PreDischargeService_Reset(&s_predischarge_state);
    s_bq_wake_ms = 0u;
    s_debug_ms = 0u;
    App_Power_SyncServiceSnapshot();

    Bms_PowerPort_SetChargeRequest(false);
    if (ctx->pack.cells_valid)
    {
        (void)Bms_PowerPort_AllMainFetsOff();
    }
}

void Bms_PowerService_Task(uint32_t interval_ms)
{
    const Bms_ContextTypeDef *ctx = Bms_Model_GetContext();
    const Bms_Config_PowerTypeDef *power_config = Bms_Config_GetPowerConfig();
    const Bms_PreDischargeService_ConfigTypeDef *predischarge_config =
        Bms_Config_GetPreDischargeServiceConfig();
    Bms_ChargeService_EvaluationTypeDef charge_eval;
    Bms_DischargeService_EvaluationTypeDef discharge_eval;
    bool cell_ok;
    bool input_ok;
    bool sc_charge_ok;
    bool charge_temp_ok;
    bool charge_voltage_ok;
    bool discharge_temp_ok;
    bool discharge_over_current;

    (void)Bms_DischargeService_Evaluate(ctx,
                                        Bms_Config_GetDischargeServiceConfig(),
                                        &discharge_eval);
    cell_ok = discharge_eval.cell_sample_ok;
    input_ok = Bms_PowerPort_IsInputPresent(power_config->sc_input_valid_mv);
    sc_charge_ok = !Bms_PowerPort_HasScFault();
    (void)Bms_ChargeService_Evaluate(&s_charge_state,
                                     ctx,
                                     Bms_Config_GetChargeServiceConfig(),
                                     cell_ok,
                                     &charge_eval);
    charge_temp_ok = charge_eval.temp_ok;
    charge_voltage_ok = charge_eval.voltage_ok;
    discharge_temp_ok = discharge_eval.temp_ok;
    discharge_over_current = discharge_eval.over_current;

    if (Bms_ProtectionService_IsDischargeShortCircuit(ctx))
    {
        if (!s_discharge_scd_latched)
        {
            printf("电源 放电SCD锁存: 检测到BQ短路保护，停止自动重试，请排查后发�?fault clear\r\n");
        }
        s_discharge_scd_latched = true;
    }

#if APP_POWER_CHARGE_ONLY_TEST_ENABLE
    if (App_Power_RunChargeOnlyTest(cell_ok,
                                    input_ok,
                                    sc_charge_ok,
                                    charge_temp_ok,
                                    charge_voltage_ok))
    {
        if (!Bms_DebugCli_IsVofaStreaming() && !Bms_DebugCli_IsBqMonitoring())
        {
            s_debug_ms = (uint16_t)(s_debug_ms + interval_ms);
            if (s_debug_ms >= power_config->debug_period_ms)
            {
                s_debug_ms = 0u;
                App_Power_PrintDebug();
            }
        }
        App_Power_SyncServiceSnapshot();
        return;
    }
#endif

    if (!cell_ok)
    {
        /*
         * BQ 可能已经因低压进�?shutdown：cell �?0 或通信失败不能直接禁止
         * SC8815，否则插�?24V 后无法给 BMS+ 预充，也就唤不醒 BQ�?        */
        s_discharge_allowed = false;
        Bms_PreDischargeService_Reset(&s_predischarge_state);
        s_charge_allowed = input_ok && sc_charge_ok;
        if (s_charge_allowed)
        {
            if (s_power_state != APP_POWER_STATE_BQ_WAKE)
            {
                s_bq_wake_ms = 0u;
            }
            else if (s_bq_wake_ms < power_config->bq_wake_timeout_ms)
            {
                s_bq_wake_ms += interval_ms;
            }

            if (s_bq_wake_ms < power_config->bq_wake_timeout_ms)
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
    else if (ctx->protection.fault_active)
    {
        s_bq_wake_ms = 0u;
        Bms_PreDischargeService_Reset(&s_predischarge_state);
        s_power_state = APP_POWER_STATE_FAULT;
        s_charge_allowed = false;
        s_discharge_allowed = false;
        s_low_power_sound_played = false;
        App_Power_SetOutput(false, false);
    }
    else if (s_discharge_scd_latched)
    {
        s_bq_wake_ms = 0u;
        Bms_PreDischargeService_Reset(&s_predischarge_state);
        s_charge_allowed = input_ok && sc_charge_ok && charge_temp_ok && charge_voltage_ok;
        s_discharge_allowed = false;

        if (s_charge_allowed)
        {
            s_power_state = APP_POWER_STATE_MONITOR;
        }
        else
        {
            s_power_state = APP_POWER_STATE_FAULT;
        }

        App_Power_SetOutput(s_charge_allowed, false);
    }
    else if (discharge_eval.low_voltage || discharge_over_current)
    {
        s_bq_wake_ms = 0u;
        Bms_PreDischargeService_Reset(&s_predischarge_state);
        s_power_state = APP_POWER_STATE_LOW;
        if (!s_low_power_sound_played)
        {
            s_low_power_sound_played = true;
            if (power_config->buzzer_enable)
            {
                Bms_BuzzerService_PlayLowPower();
            }
        }
        s_charge_allowed = input_ok && sc_charge_ok && charge_temp_ok && charge_voltage_ok;
        s_discharge_allowed = false;
        App_Power_SetOutput(s_charge_allowed, s_discharge_allowed);
    }
    else if (discharge_eval.recover_blocked)
    {
        s_bq_wake_ms = 0u;
        Bms_PreDischargeService_Reset(&s_predischarge_state);
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
        if (Bms_PreDischargeService_ShouldRun(&s_predischarge_state,
                                              predischarge_config,
                                              s_discharge_allowed))
        {
            /*
             * 这里不是 MCU 单独强开 PDSG。BQ �?PDSG_EN 已经打开，只要主机允�?             * DSG，器件会自动先预放电，再�?LD/超时/压差条件切到�?DSG�?             */
            if (Bms_PreDischargeService_Update(&s_predischarge_state,
                                               predischarge_config,
                                               interval_ms))
            {
                printf("电源 预放电等待完�?%u ms\r\n",
                       (unsigned int)predischarge_config->duration_ms);
            }
            App_Power_SetPreDischarge(s_charge_allowed);
        }
        else
        {
            App_Power_SetOutput(s_charge_allowed, s_discharge_allowed);
        }
    }

    if (!Bms_DebugCli_IsVofaStreaming() && !Bms_DebugCli_IsBqMonitoring())
    {
        s_debug_ms = (uint16_t)(s_debug_ms + interval_ms);
        if (s_debug_ms >= power_config->debug_period_ms)
        {
            s_debug_ms = 0u;
            App_Power_PrintDebug();
        }
    }
    App_Power_SyncServiceSnapshot();
}

App_Power_StateTypeDef Bms_PowerService_GetState(void)
{
    return s_power_state;
}

bool Bms_PowerService_IsChargeAllowed(void)
{
    return s_charge_allowed;
}

bool Bms_PowerService_IsDischargeAllowed(void)
{
    return s_discharge_allowed;
}

void App_Power_Init(void)
{
    Bms_PowerService_Init();
}

void App_Power_Task(uint32_t interval_ms)
{
    Bms_PowerService_Task(interval_ms);
}

App_Power_StateTypeDef App_Power_GetState(void)
{
    return Bms_PowerService_GetState();
}

bool App_Power_IsChargeAllowed(void)
{
    return Bms_PowerService_IsChargeAllowed();
}

bool App_Power_IsDischargeAllowed(void)
{
    return Bms_PowerService_IsDischargeAllowed();
}

void App_Power_PrintSnapshot(void)
{
    Bms_PowerService_PrintSnapshot();
}

void App_Power_PrintStopReason(void)
{
    Bms_PowerService_PrintStopReason();
}

bool App_Power_ClearDischargeFault(void)
{
    return Bms_PowerService_ClearDischargeFault();
}
