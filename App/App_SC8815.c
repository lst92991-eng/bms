#include "App_SC8815.h"

#include <string.h>

#include "App_Safety.h"
#include "Int_SC8815.h"
#include "Int_SC8815_BSP.h"
#include "main.h"

/**
 * @file App_SC8815.c
 * @brief SC8815 单所有者状态机：同步停机，异步且带代际校验的安全启动。
 */

enum
{
    APP_SC8815_SAMPLE_PERIOD_MS = 1000u
};

static App_SC8815_SnapshotTypeDef s_sc;
static uint16_t s_sample_ms;
static bool s_interrupt_resolution_pending;
static uint8_t s_interrupt_clean_sample_count;
static uint32_t s_interrupt_resolution_sequence;
static bool s_gpo_released;
static uint8_t s_ready_clean_sample_count;
static bool s_ready_reported;

static void App_SC8815_UpdateReadyReport(bool clean)
{
    if (!clean)
    {
        s_ready_clean_sample_count = 0u;
        s_ready_reported = false;
        App_Safety_ReportScReady(false);
        return;
    }
    if (s_ready_clean_sample_count < 2u)
    {
        s_ready_clean_sample_count++;
    }
    if ((s_ready_clean_sample_count >= 2u) && !s_ready_reported)
    {
        s_ready_reported = true;
        App_Safety_ReportScReady(true);
    }
}

static uint32_t App_SC8815_EnterCritical(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

static void App_SC8815_ExitCritical(uint32_t primask)
{
    __set_PRIMASK(primask);
}

static uint32_t App_SC8815_AddMs(uint32_t value, uint32_t add)
{
    return (value > (UINT32_MAX - add)) ? UINT32_MAX : (value + add);
}

static void App_SC8815_RecordFailure(Int_SC8815_StatusTypeDef ret)
{
    uint32_t primask = App_SC8815_EnterCritical();
    s_sc.comm_ok = false;
    s_sc.last_error = (uint8_t)ret;
    App_SC8815_ExitCritical(primask);
}

static bool App_SC8815_Check(Int_SC8815_StatusTypeDef ret)
{
    if (ret == INT_SC8815_OK)
    {
        return true;
    }
    App_SC8815_RecordFailure(ret);
    App_SC8815_UpdateReadyReport(false);
    App_SC8815_EmergencyStop();
    return false;
}

static void App_SC8815_SetStandbyMonitor(void)
{
    uint32_t primask;

    Int_SC8815_ForceStandby();
    Int_SC8815_SetChipEnabled(true);
    primask = App_SC8815_EnterCritical();
    s_sc.commanded_charge = false;
    s_sc.observed_standby = true;
    s_sc.chip_enabled = true;
    App_SC8815_ExitCritical(primask);

    /* GPO 控制本板输入 PMOS；PSTOP 已先拉高，寄存器失败也保持安全。 */
    if (App_SC8815_Check(
            Int_SC8815_UpdateReg(SC8815_REG_CTRL3_SET, SC8815_CTRL3_SET_GPO_CTRL_MASK, 0u)))
    {
        s_gpo_released = true;
    }
}

static bool App_SC8815_CommandStillCurrent(uint32_t generation, uint32_t safety_authorization_epoch)
{
    bool current;
    uint32_t primask = App_SC8815_EnterCritical();

    current = s_sc.desired_charge && !s_sc.emergency_latched && (s_sc.generation == generation) &&
              (s_sc.safety_authorization_epoch == safety_authorization_epoch);
    App_SC8815_ExitCritical(primask);
    return current && App_Safety_IsPowerReleaseAuthorized(safety_authorization_epoch);
}

static bool App_SC8815_TryReleasePstop(uint32_t generation, uint32_t safety_authorization_epoch)
{
    bool authorized;
    uint32_t primask = App_SC8815_EnterCritical();

    authorized = s_sc.desired_charge && !s_sc.emergency_latched &&
                 (s_sc.generation == generation) &&
                 (s_sc.safety_authorization_epoch == safety_authorization_epoch) &&
                 App_Safety_IsPowerReleaseAuthorized(safety_authorization_epoch);
    if (authorized)
    {
        /* 与最终授权校验同一极短临界区，退出后任何 pending trip 立即拉高。 */
        Int_SC8815_SetStandby(false);
        s_sc.chip_enabled = true;
        s_sc.commanded_charge = true;
        s_sc.observed_standby = Int_SC8815_IsStandbyAsserted();
    }
    App_SC8815_ExitCritical(primask);
    return authorized;
}

static Int_SC8815_StatusTypeDef App_SC8815_VerifyReleaseManifest(uint16_t desired_ibat_limit_ma,
                                                                 uint32_t *observed_ibat_limit_ma)
{
    uint8_t value;
    uint32_t ibus_limit_ma;
    Int_SC8815_StatusTypeDef ret;

    ret = Int_SC8815_ReadReg(SC8815_REG_VBAT_SET, &value);
    if ((ret != INT_SC8815_OK) || (value != SC8815_PROJECT_VBAT_SET_VALUE))
    {
        return (ret == INT_SC8815_OK) ? INT_SC8815_ERROR_STATE : ret;
    }
    ret = Int_SC8815_ReadReg(SC8815_REG_RATIO, &value);
    if ((ret != INT_SC8815_OK) || (value != SC8815_PROJECT_RATIO_VALUE))
    {
        return (ret == INT_SC8815_OK) ? INT_SC8815_ERROR_STATE : ret;
    }
    ret = Int_SC8815_ReadReg(SC8815_REG_CTRL0_SET, &value);
    if ((ret != INT_SC8815_OK) || ((value & SC8815_PROJECT_FORBID_CTRL0_SET_MASK) != 0u))
    {
        return (ret == INT_SC8815_OK) ? INT_SC8815_ERROR_STATE : ret;
    }
    ret = Int_SC8815_ReadReg(SC8815_REG_CTRL1_SET, &value);
    if ((ret != INT_SC8815_OK) ||
        ((value & SC8815_PROJECT_CTRL1_SAFE_SET_MASK) != SC8815_PROJECT_CTRL1_SAFE_SET_MASK) ||
        ((value & SC8815_PROJECT_FORBID_CTRL1_SET_MASK) != 0u))
    {
        return (ret == INT_SC8815_OK) ? INT_SC8815_ERROR_STATE : ret;
    }
    ret = Int_SC8815_ReadReg(SC8815_REG_CTRL2_SET, &value);
    if ((ret != INT_SC8815_OK) ||
        ((value & SC8815_PROJECT_CTRL2_SAFE_SET_MASK) != SC8815_PROJECT_CTRL2_SAFE_SET_MASK))
    {
        return (ret == INT_SC8815_OK) ? INT_SC8815_ERROR_STATE : ret;
    }
    ret = Int_SC8815_ReadReg(SC8815_REG_CTRL3_SET, &value);
    if ((ret != INT_SC8815_OK) ||
        ((value & (SC8815_CTRL3_SET_GPO_CTRL_MASK | SC8815_CTRL3_SET_AD_START_MASK)) !=
         (SC8815_CTRL3_SET_GPO_CTRL_MASK | SC8815_CTRL3_SET_AD_START_MASK)) ||
        ((value & SC8815_PROJECT_FORBID_CTRL3_SET_MASK) != 0u))
    {
        return (ret == INT_SC8815_OK) ? INT_SC8815_ERROR_STATE : ret;
    }
    ret = Int_SC8815_ReadReg(SC8815_REG_MASK, &value);
    if ((ret != INT_SC8815_OK) ||
        ((value & SC8815_PROJECT_MASK_SAFE_SET_MASK) != SC8815_PROJECT_MASK_SAFE_SET_MASK))
    {
        return (ret == INT_SC8815_OK) ? INT_SC8815_ERROR_STATE : ret;
    }
    ret = Int_SC8815_GetCurrentLimitMa(INT_SC8815_LIMIT_IBUS, &ibus_limit_ma);
    if ((ret != INT_SC8815_OK) || (ibus_limit_ma > SC8815_PROJECT_IBUS_LIMIT_MA) ||
        ((SC8815_PROJECT_IBUS_LIMIT_MA - ibus_limit_ma) > 100u))
    {
        return (ret == INT_SC8815_OK) ? INT_SC8815_ERROR_STATE : ret;
    }
    ret = Int_SC8815_GetCurrentLimitMa(INT_SC8815_LIMIT_IBAT, observed_ibat_limit_ma);
    if ((ret != INT_SC8815_OK) || (*observed_ibat_limit_ma > desired_ibat_limit_ma) ||
        ((desired_ibat_limit_ma - *observed_ibat_limit_ma) > 100u))
    {
        return (ret == INT_SC8815_OK) ? INT_SC8815_ERROR_STATE : ret;
    }
    return INT_SC8815_OK;
}

static void App_SC8815_ApplyChargeRequest(void)
{
    uint32_t generation;
    uint16_t desired_limit_ma;
    uint32_t safety_authorization_epoch;
    bool desired;
    bool emergency_latched;
    bool comm_ok;
    bool vbus_short;
    bool otp;
    bool commanded_charge;
    bool observed_standby;
    uint16_t commanded_limit_ma;
    uint32_t observed_limit_ma;
    uint32_t primask = App_SC8815_EnterCritical();

    generation = s_sc.generation;
    safety_authorization_epoch = s_sc.safety_authorization_epoch;
    desired_limit_ma = s_sc.desired_ibat_limit_ma;
    desired = s_sc.desired_charge;
    emergency_latched = s_sc.emergency_latched;
    comm_ok = s_sc.comm_ok;
    vbus_short = s_sc.vbus_short;
    otp = s_sc.otp;
    commanded_charge = s_sc.commanded_charge;
    commanded_limit_ma = s_sc.commanded_ibat_limit_ma;
    App_SC8815_ExitCritical(primask);

    observed_standby = Int_SC8815_IsStandbyAsserted();
    primask = App_SC8815_EnterCritical();
    s_sc.observed_standby = observed_standby;
    App_SC8815_ExitCritical(primask);
    if (!desired || emergency_latched)
    {
        if (!observed_standby || commanded_charge || !s_gpo_released)
        {
            App_SC8815_SetStandbyMonitor();
        }
        return;
    }

    /* active 也必须逐周期复验；TTL/trip 失效后的第一动作仍是同步 PSTOP。 */
    if (!App_Safety_IsPowerReleaseAuthorized(safety_authorization_epoch))
    {
        App_SC8815_EmergencyStop();
        return;
    }

    if (!comm_ok || vbus_short || otp)
    {
        App_SC8815_EmergencyStop();
        return;
    }

    if (commanded_charge && !observed_standby && (commanded_limit_ma == desired_limit_ma))
    {
        return;
    }

    Int_SC8815_ForceStandby();
    primask = App_SC8815_EnterCritical();
    s_sc.commanded_charge = false;
    s_sc.observed_standby = true;
    App_SC8815_ExitCritical(primask);

    if (!App_SC8815_Check(
            Int_SC8815_WriteReg(SC8815_REG_VBAT_SET, SC8815_PROJECT_VBAT_SET_VALUE)) ||
        !App_SC8815_Check(Int_SC8815_WriteReg(SC8815_REG_RATIO, SC8815_PROJECT_RATIO_VALUE)) ||
        !App_SC8815_Check(Int_SC8815_UpdateReg(SC8815_REG_CTRL0_SET,
                                               SC8815_PROJECT_CTRL0_SAFE_CLEAR_MASK,
                                               SC8815_PROJECT_CTRL0_EN_OTG_VALUE)) ||
        !App_SC8815_Check(Int_SC8815_UpdateReg(SC8815_REG_CTRL1_SET,
                                               SC8815_PROJECT_CTRL1_SAFE_CLEAR_MASK,
                                               SC8815_PROJECT_CTRL1_SAFE_SET_MASK)) ||
        !App_SC8815_Check(
            Int_SC8815_UpdateReg(SC8815_REG_CTRL2_SET, 0u, SC8815_PROJECT_CTRL2_SAFE_SET_MASK)) ||
        !App_SC8815_Check(Int_SC8815_UpdateReg(SC8815_REG_CTRL3_SET,
                                               SC8815_PROJECT_CTRL3_SAFE_CLEAR_MASK,
                                               SC8815_CTRL3_SET_GPO_CTRL_MASK)) ||
        !App_SC8815_Check(
            Int_SC8815_UpdateReg(SC8815_REG_MASK, 0u, SC8815_PROJECT_MASK_SAFE_SET_MASK)) ||
        !App_SC8815_Check(
            Int_SC8815_SetCurrentLimitMa(INT_SC8815_LIMIT_IBUS, SC8815_PROJECT_IBUS_LIMIT_MA)) ||
        !App_SC8815_Check(Int_SC8815_SetCurrentLimitMa(INT_SC8815_LIMIT_IBAT, desired_limit_ma)) ||
        !App_SC8815_Check(Int_SC8815_SetAdcEnabled(true)) ||
        !App_SC8815_Check(App_SC8815_VerifyReleaseManifest(desired_limit_ma, &observed_limit_ma)))
    {
        return;
    }

    if ((observed_limit_ma > desired_limit_ma) ||
        !App_SC8815_CommandStillCurrent(generation, safety_authorization_epoch))
    {
        App_SC8815_EmergencyStop();
        return;
    }

    primask = App_SC8815_EnterCritical();
    s_sc.commanded_ibat_limit_ma = desired_limit_ma;
    s_sc.observed_ibat_limit_ma = (uint16_t)observed_limit_ma;
    App_SC8815_ExitCritical(primask);
    s_gpo_released = false;
    Int_SC8815_SetChipEnabled(true);

    /* 释放 PSTOP 前最后一次代际校验，旧启动序列不能越过新的停机请求。 */
    if (!App_SC8815_TryReleasePstop(generation, safety_authorization_epoch))
    {
        Int_SC8815_ForceStandby();
    }
}

static bool App_SC8815_Sample(void)
{
    Int_SC8815_StatusFlagsTypeDef status;
    App_SC8815_SnapshotTypeDef sample = {0};
    Int_SC8815_StatusTypeDef ret;
    uint32_t primask;

    ret = Int_SC8815_ReadStatus(&status);
    if (ret != INT_SC8815_OK)
    {
        goto sample_failed;
    }
    sample.ac_ok = status.ac_ok;
    sample.indet = status.indet;
    sample.vbus_short = status.vbus_short;
    sample.otp = status.otp;
    sample.eoc = status.eoc;
    sample.status_raw = status.raw;

    ret = Int_SC8815_ReadAdcRaw(INT_SC8815_ADC_VBUS, &sample.vbus_raw);
    if (ret != INT_SC8815_OK)
    {
        goto sample_failed;
    }
    ret = Int_SC8815_AdcVoltageRawToMv(INT_SC8815_ADC_VBUS, sample.vbus_raw, &sample.vbus_mv);
    if (ret != INT_SC8815_OK)
    {
        goto sample_failed;
    }
    ret = Int_SC8815_ReadAdcRaw(INT_SC8815_ADC_VBAT, &sample.vbat_raw);
    if (ret != INT_SC8815_OK)
    {
        goto sample_failed;
    }
    ret = Int_SC8815_AdcVoltageRawToMv(INT_SC8815_ADC_VBAT, sample.vbat_raw, &sample.vbat_mv);
    if (ret != INT_SC8815_OK)
    {
        goto sample_failed;
    }
    ret = Int_SC8815_ReadAdcCurrentRaw(INT_SC8815_CURRENT_IBUS, &sample.ibus_raw);
    if (ret != INT_SC8815_OK)
    {
        goto sample_failed;
    }
    ret = Int_SC8815_AdcCurrentRawToMa(INT_SC8815_CURRENT_IBUS, sample.ibus_raw, &sample.ibus_ma);
    if (ret != INT_SC8815_OK)
    {
        goto sample_failed;
    }
    ret = Int_SC8815_ReadAdcCurrentRaw(INT_SC8815_CURRENT_IBAT, &sample.ibat_raw);
    if (ret != INT_SC8815_OK)
    {
        goto sample_failed;
    }
    ret = Int_SC8815_AdcCurrentRawToMa(INT_SC8815_CURRENT_IBAT, sample.ibat_raw, &sample.ibat_ma);
    if (ret != INT_SC8815_OK)
    {
        goto sample_failed;
    }

    sample.observed_standby = Int_SC8815_IsStandbyAsserted();
    primask = App_SC8815_EnterCritical();
    s_sc.comm_ok = true;
    s_sc.last_error = INT_SC8815_OK;
    s_sc.ac_ok = sample.ac_ok;
    s_sc.indet = sample.indet;
    s_sc.vbus_short = sample.vbus_short;
    s_sc.otp = sample.otp;
    s_sc.eoc = sample.eoc;
    s_sc.status_raw = sample.status_raw;
    s_sc.vbus_raw = sample.vbus_raw;
    s_sc.vbat_raw = sample.vbat_raw;
    s_sc.ibus_raw = sample.ibus_raw;
    s_sc.ibat_raw = sample.ibat_raw;
    s_sc.vbus_mv = sample.vbus_mv;
    s_sc.vbat_mv = sample.vbat_mv;
    s_sc.ibus_ma = sample.ibus_ma;
    s_sc.ibat_ma = sample.ibat_ma;
    s_sc.observed_standby = sample.observed_standby;
    s_sc.sample_age_ms = 0u;
    App_SC8815_ExitCritical(primask);

    if (sample.vbus_short || sample.otp)
    {
        App_SC8815_UpdateReadyReport(false);
        App_SC8815_EmergencyStop();
    }
    else
    {
        App_SC8815_UpdateReadyReport(true);
    }
    return true;

sample_failed:
    App_SC8815_UpdateReadyReport(false);
    App_SC8815_EmergencyStop();
    App_SC8815_RecordFailure(ret);
    return false;
}

void App_SC8815_Init(void)
{
    memset(&s_sc, 0, sizeof(s_sc));
    s_sc.desired_ibat_limit_ma = SC8815_PROJECT_IBAT_LIMIT_MA;
    s_sc.observed_standby = true;
    s_sc.emergency_latched = true;
    s_sc.generation = 1u;
    s_sc.last_error = INT_SC8815_OK;
    s_interrupt_resolution_pending = false;
    s_interrupt_clean_sample_count = 0u;
    s_interrupt_resolution_sequence = Int_SC8815_GetInterruptSequence();
    s_gpo_released = false;
    s_ready_clean_sample_count = 0u;
    s_ready_reported = false;
    App_Safety_ReportScReady(false);

    Int_SC8815_InitSafe();
    App_SC8815_SetStandbyMonitor();
    (void)App_SC8815_Check(Int_SC8815_SetAdcEnabled(true));
    (void)App_SC8815_Sample();
}

void App_SC8815_Task(uint16_t interval_ms)
{
    uint32_t elapsed_ms;
    uint32_t primask;
    bool interrupt_pending;
    bool sample_ok = false;
    bool sample_attempted = false;
    App_SC8815_SnapshotTypeDef snapshot;

    primask = App_SC8815_EnterCritical();
    s_sc.sample_age_ms = App_SC8815_AddMs(s_sc.sample_age_ms, interval_ms);
    App_SC8815_ExitCritical(primask);
    elapsed_ms = (uint32_t)s_sample_ms + interval_ms;
    s_sample_ms = (elapsed_ms >= APP_SC8815_SAMPLE_PERIOD_MS) ? APP_SC8815_SAMPLE_PERIOD_MS
                                                              : (uint16_t)elapsed_ms;

    interrupt_pending = Int_SC8815_TakeInterruptPending();
    if (interrupt_pending)
    {
        /* owner 重申门禁，防止旧 resolve 流程覆盖新 IRQ。 */
        App_Safety_SetPowerInhibit(APP_SAFETY_INHIBIT_SC_EVENT);
        App_SC8815_EmergencyStop();
        s_interrupt_resolution_pending = true;
        s_interrupt_clean_sample_count = 0u;
        s_interrupt_resolution_sequence = Int_SC8815_GetInterruptSequence();
    }
    if (interrupt_pending || (s_sample_ms >= APP_SC8815_SAMPLE_PERIOD_MS))
    {
        s_sample_ms = 0u;
        sample_attempted = true;
        sample_ok = App_SC8815_Sample();
    }
    if (s_interrupt_resolution_pending && sample_ok)
    {
        App_SC8815_GetSnapshot(&snapshot);
        if (snapshot.comm_ok && !snapshot.vbus_short && !snapshot.otp)
        {
            s_interrupt_clean_sample_count++;
            if (s_interrupt_clean_sample_count >= 2u)
            {
                uint32_t sequence;

                /* 序号复验与 clear 共用一段 IRQ 临界区，新事件不能被旧流程清除。 */
                primask = App_SC8815_EnterCritical();
                sequence = Int_SC8815_GetInterruptSequence();
                if (sequence == s_interrupt_resolution_sequence)
                {
                    App_Safety_ClearPowerInhibit(APP_SAFETY_INHIBIT_SC_EVENT);
                    s_interrupt_resolution_pending = false;
                    s_interrupt_clean_sample_count = 0u;
                }
                else
                {
                    App_Safety_SetPowerInhibit(APP_SAFETY_INHIBIT_SC_EVENT);
                    App_SC8815_EmergencyStop();
                    s_interrupt_resolution_sequence = sequence;
                    s_interrupt_clean_sample_count = 0u;
                }
                App_SC8815_ExitCritical(primask);
            }
        }
        else
        {
            s_interrupt_clean_sample_count = 0u;
        }
    }
    else if (s_interrupt_resolution_pending && sample_attempted)
    {
        s_interrupt_clean_sample_count = 0u;
    }
    App_SC8815_ApplyChargeRequest();
}

void App_SC8815_RequestCharge(bool enable)
{
    uint32_t primask;

    if (enable)
    {
        /* 无 Safety epoch 的启动请求按越权处理，并保持功率级关闭。 */
        App_SC8815_EmergencyStop();
        return;
    }
    else
    {
        /* 禁充请求的第一动作必须是不依赖调度器的硬件停机。 */
        Int_SC8815_ForceStandby();
    }

    primask = App_SC8815_EnterCritical();
    s_sc.generation++;
    /* 启动必须走携带 Safety epoch 的授权接口；旧无令牌入口只能停机。 */
    s_sc.desired_charge = false;
    s_sc.commanded_charge = false;
    s_sc.observed_standby = true;
    s_sc.emergency_latched = true;
    s_sc.safety_authorization_epoch = 0u;
    App_SC8815_ExitCritical(primask);
}

void App_SC8815_EmergencyStop(void)
{
    uint32_t primask;

    Int_SC8815_ForceStandby();
    primask = App_SC8815_EnterCritical();
    s_sc.generation++;
    s_sc.desired_charge = false;
    s_sc.commanded_charge = false;
    s_sc.observed_standby = true;
    s_sc.emergency_latched = true;
    s_sc.safety_authorization_epoch = 0u;
    App_SC8815_ExitCritical(primask);
}

void App_SC8815_EmergencyStopFromISR(void)
{
    App_SC8815_EmergencyStop();
}

void App_SC8815_InvalidateAuthorization(void)
{
    App_SC8815_EmergencyStop();
}

bool App_SC8815_RequestChargeAuthorized(uint32_t safety_authorization_epoch)
{
    bool accepted;
    uint32_t primask;

    if (!App_Safety_IsPowerReleaseAuthorized(safety_authorization_epoch))
    {
        return false;
    }
    primask = App_SC8815_EnterCritical();
    s_sc.generation++;
    accepted = true;
    if (accepted)
    {
        s_sc.safety_authorization_epoch = safety_authorization_epoch;
        s_sc.emergency_latched = false;
        s_sc.desired_charge = true;
    }
    App_SC8815_ExitCritical(primask);
    return accepted;
}

bool App_SC8815_SetChargeCurrentLimitMa(uint16_t current_ma)
{
    bool limit_changed;
    bool release_active;
    uint32_t primask;

    if ((current_ma < SC8815_PROJECT_MIN_LIMIT_CURRENT_MA) ||
        (current_ma > SC8815_PROJECT_MAX_IBAT_LIMIT_MA))
    {
        return false;
    }

    primask = App_SC8815_EnterCritical();
    limit_changed = current_ma != s_sc.desired_ibat_limit_ma;
    release_active = s_sc.desired_charge || s_sc.commanded_charge;
    App_SC8815_ExitCritical(primask);
    if (!limit_changed)
    {
        return true;
    }
    if (release_active)
    {
        Int_SC8815_ForceStandby();
    }
    primask = App_SC8815_EnterCritical();
    s_sc.generation++;
    s_sc.desired_ibat_limit_ma = current_ma;
    if (release_active)
    {
        s_sc.desired_charge = false;
        s_sc.commanded_charge = false;
        s_sc.observed_standby = true;
        s_sc.emergency_latched = true;
        s_sc.safety_authorization_epoch = 0u;
    }
    App_SC8815_ExitCritical(primask);
    return true;
}

void App_SC8815_GetSnapshot(App_SC8815_SnapshotTypeDef *snapshot)
{
    uint32_t primask;

    if (snapshot == NULL)
    {
        return;
    }
    primask = App_SC8815_EnterCritical();
    *snapshot = s_sc;
    App_SC8815_ExitCritical(primask);
}

bool App_SC8815_IsCommOk(void)
{
    return s_sc.comm_ok;
}
bool App_SC8815_IsAcOk(void)
{
    return s_sc.ac_ok;
}
bool App_SC8815_HasFault(void)
{
    return !s_sc.comm_ok || s_sc.vbus_short || s_sc.otp;
}
bool App_SC8815_IsCharging(void)
{
    return s_sc.desired_charge && s_sc.commanded_charge && !s_sc.observed_standby;
}
uint32_t App_SC8815_GetVbusMv(void)
{
    return s_sc.vbus_mv;
}
uint32_t App_SC8815_GetVbatMv(void)
{
    return s_sc.vbat_mv;
}
uint32_t App_SC8815_GetInputLimitMa(void)
{
    return SC8815_PROJECT_IBUS_LIMIT_MA;
}
