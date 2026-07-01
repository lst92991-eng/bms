#include "App_SC8815.h"

#include <stdio.h>

#include "FreeRTOS.h"
#include "Int_SC8815.h"
#include "Int_SC8815_BSP.h"
#include "gpio.h"
#include "queue.h"

/**
 * @file App_SC8815.c
 * @brief SC8815 充电芯片 APP 层。
 *
 * 本层只负责安全启停和状态监控，不直接实现充电环路算法。
 * SC8815 自己管理功率环路；MCU 只控制 CE_N/PSTOP、写入经过 INT 层
 * guard 校验的寄存器，并周期读取 STATUS/ADC。
 *
 * 默认启动后状态：
 * - `CE_N = 0`：芯片使能，允许寄存器和 ADC 监控。
 * - `PSTOP = 1`：保持 standby，充电功率级停止。
 * - `charge_requested = false`：bring-up 阶段不会自动启动充电。
 */

#define APP_SC8815_BIT(value, mask)                (((value) & (mask)) != 0u ? 1u : 0u)
#define APP_SC8815_DEBUG_PERIOD_MS              (1000u)
#define APP_SC8815_CHARGE_QUEUE_LEN             (1u)

/*
 * SC8815 状态快照。
 * 当前只给本文件状态机使用，外部通过串口日志观察，不再暴露只读指针。
 */
typedef struct
{
    bool comm_ok;
    bool charge_requested;
    bool chip_enabled;
    bool standby;
    bool ac_ok;
    bool indet;
    bool vbus_short;
    bool otp;
    bool eoc;
    uint8_t status_raw;
    uint32_t vbus_mv;
    uint32_t vbat_mv;
    uint32_t ibus_ma;
    uint32_t ibat_ma;
    uint16_t vbus_raw;
    uint16_t vbat_raw;
    uint16_t ibus_raw;
    uint16_t ibat_raw;
    uint8_t last_error;
    uint8_t ce_n_pin;
    uint8_t pstop_pin;
    uint8_t vbat_set_reg;
    uint8_t ratio_reg;
    uint8_t ibus_lim_reg;
    uint8_t ibat_lim_reg;
    uint8_t vinreg_reg;
    uint8_t ctrl0_reg;
    uint8_t ctrl1_reg;
    uint8_t ctrl2_reg;
    uint8_t ctrl3_reg;
    uint8_t mask_reg;
} App_SC8815_StateTypeDef;

static App_SC8815_StateTypeDef s_sc;
static QueueHandle_t s_charge_queue = NULL;
static uint16_t s_debug_ms = 0u;

/**
 * @brief 进入 standby monitor 安全态。
 *
 * 该状态下 SC8815 已使能，便于读寄存器/ADC；同时 PSTOP 保持高电平，
 * 不启动充电功率环路。
 */
static void App_SC8815_SetStandbyMonitor(void)
{
    (void)Int_SC8815_SetStandby(true);
    (void)Int_SC8815_SetChipEnabled(true);
    /*
     * 本板 24V 输入 PMOS 不是接 PGATE，而是由 GPO->R1->Q4 gate 控制。
     * 回到待机时释放 GPO，避免 SC 停止后输入开关仍被拉低导通。
     */
    (void)Int_SC8815_UpdateReg(SC8815_REG_CTRL3_SET,
                               SC8815_CTRL3_SET_GPO_CTRL_MASK,
                               0u);
    s_sc.standby = true;
    s_sc.chip_enabled = true;
}

/**
 * @brief 将 INT 层返回值折叠为 APP 通信状态。
 *
 * APP 不在单个寄存器失败后做复杂重试；当前周期标记 `comm_ok=false`，
 * 下一次任务重新采样。
 */
static bool App_SC8815_Check(Int_SC8815_StatusTypeDef ret)
{
    if (ret == INT_SC8815_OK)
    {
        return true;
    }

    s_sc.comm_ok = false;
    s_sc.last_error = (uint8_t)ret;
    return false;
}

static void App_SC8815_TaskInitQueue(void)
{
    if (s_charge_queue == NULL)
    {
        s_charge_queue = xQueueCreate(APP_SC8815_CHARGE_QUEUE_LEN, sizeof(uint8_t));
    }
}

static void App_SC8815_LoadChargeRequest(void)
{
    uint8_t request;

    if (s_charge_queue == NULL)
    {
        return;
    }

    if (xQueueReceive(s_charge_queue, &request, 0u) == pdPASS)
    {
        s_sc.charge_requested = (request != 0u);
    }
}

/**
 * @brief 根据充电请求更新 SC8815 状态。
 *
 * 状态机约束：
 * - 无请求：保持 standby monitor。
 * - 通信异常、VBUS short、OTP：撤销请求并回到 standby。
 * - 已运行：不重复写配置，避免扰动硬件环路。
 * - 新请求：先在 PSTOP 高电平下写配置，再释放 PSTOP。
 */
static void App_SC8815_ApplyChargeRequest(void)
{
    if (!s_sc.charge_requested)
    {
        if (!s_sc.chip_enabled || !s_sc.standby)
        {
            App_SC8815_SetStandbyMonitor();
        }
        return;
    }

    /*
     * 最近一次状态读取失败，或 SC 已报告短路/过温时，不允许进入功率环路。
     */
    if (!s_sc.comm_ok || s_sc.vbus_short || s_sc.otp)
    {
        s_sc.charge_requested = false;
        App_SC8815_SetStandbyMonitor();
        return;
    }

    /*
     * 已进入工作态后不重复写 RATIO/VBAT/限流/CTRL 位。
     * 周期性重写这些寄存器可能扰动 SC8815 的模拟控制环路。
     */
    if (s_sc.chip_enabled && !s_sc.standby)
    {
        return;
    }

    /*
     * 关键寄存器必须在 PSTOP 高电平下配置。INT 层 guard 会拒绝项目禁止位，
     * 例如 OTG/反向输出、关闭关键保护等危险配置。
     */
    if (!App_SC8815_Check(Int_SC8815_SetStandby(true)) ||
        !App_SC8815_Check(Int_SC8815_WriteReg(SC8815_REG_VBAT_SET,
                                              SC8815_PROJECT_VBAT_SET_VALUE)) ||
        !App_SC8815_Check(Int_SC8815_WriteReg(SC8815_REG_RATIO,
                                              SC8815_PROJECT_RATIO_VALUE)) ||
        !App_SC8815_Check(Int_SC8815_UpdateReg(SC8815_REG_CTRL0_SET,
                                               SC8815_PROJECT_CTRL0_SAFE_CLEAR_MASK,
                                               SC8815_PROJECT_CTRL0_EN_OTG_VALUE)) ||
        !App_SC8815_Check(Int_SC8815_UpdateReg(SC8815_REG_CTRL1_SET,
                                               SC8815_PROJECT_CTRL1_SAFE_CLEAR_MASK,
                                               SC8815_PROJECT_CTRL1_SAFE_SET_MASK)) ||
        !App_SC8815_Check(Int_SC8815_UpdateReg(SC8815_REG_CTRL2_SET,
                                               0u,
                                               SC8815_PROJECT_CTRL2_SAFE_SET_MASK)) ||
        /*
         * 原理图中 PGATE/DITHER 未接；实际 24V_IN->VBUS 前级 PMOS 由 GPO 拉低开启。
         */
        !App_SC8815_Check(Int_SC8815_UpdateReg(SC8815_REG_CTRL3_SET,
                                               SC8815_PROJECT_CTRL3_SAFE_CLEAR_MASK,
                                               SC8815_CTRL3_SET_GPO_CTRL_MASK)) ||
        !App_SC8815_Check(Int_SC8815_UpdateReg(SC8815_REG_MASK,
                                               0u,
                                               SC8815_PROJECT_MASK_SAFE_SET_MASK)) ||
        !App_SC8815_Check(Int_SC8815_SetCurrentLimitMa(INT_SC8815_LIMIT_IBUS,
                                                       SC8815_PROJECT_BRINGUP_IBUS_LIMIT_MA)) ||
        !App_SC8815_Check(Int_SC8815_SetCurrentLimitMa(INT_SC8815_LIMIT_IBAT,
                                                       SC8815_PROJECT_BRINGUP_IBAT_LIMIT_MA)) ||
        !App_SC8815_Check(Int_SC8815_SetAdcEnabled(true)))
    {
        s_sc.charge_requested = false;
        App_SC8815_SetStandbyMonitor();
        return;
    }

    /*
     * 这是唯一释放充电环路的动作。只有配置写入和 bring-up 限流全部成功后，
     * 才允许 PSTOP 拉低。
     */
    (void)Int_SC8815_SetChipEnabled(true);
    (void)Int_SC8815_SetStandby(false);
    s_sc.chip_enabled = true;
    s_sc.standby = false;
}

/**
 * @brief 读取 SC8815 STATUS 和 ADC 快照。
 *
 * standby monitor 和充电态都允许读取 ADC；任何读失败都会让本周期
 * `comm_ok=false`。
 */
static void App_SC8815_Sample(void)
{
    Int_SC8815_StatusFlagsTypeDef status;

    /* 每个采样周期重新评估通信状态，避免一次失败永久保持故障。 */
    s_sc.comm_ok = true;
    s_sc.last_error = INT_SC8815_OK;
    if (App_SC8815_Check(Int_SC8815_ReadStatus(&status)))
    {
        s_sc.ac_ok = status.ac_ok;
        s_sc.indet = status.indet;
        s_sc.vbus_short = status.vbus_short;
        s_sc.otp = status.otp;
        s_sc.eoc = status.eoc;
        s_sc.status_raw = status.raw;
    }

    (void)App_SC8815_Check(Int_SC8815_ReadAdcVoltageMv(INT_SC8815_ADC_VBUS,
                                                       &s_sc.vbus_mv));
    (void)App_SC8815_Check(Int_SC8815_ReadAdcVoltageMv(INT_SC8815_ADC_VBAT,
                                                       &s_sc.vbat_mv));
    (void)App_SC8815_Check(Int_SC8815_ReadAdcCurrentMa(INT_SC8815_CURRENT_IBUS,
                                                       &s_sc.ibus_ma));
    (void)App_SC8815_Check(Int_SC8815_ReadAdcCurrentMa(INT_SC8815_CURRENT_IBAT,
                                                       &s_sc.ibat_ma));
    (void)App_SC8815_Check(Int_SC8815_ReadAdcRaw(INT_SC8815_ADC_VBUS,
                                                 &s_sc.vbus_raw));
    (void)App_SC8815_Check(Int_SC8815_ReadAdcRaw(INT_SC8815_ADC_VBAT,
                                                 &s_sc.vbat_raw));
    (void)App_SC8815_Check(Int_SC8815_ReadAdcCurrentRaw(INT_SC8815_CURRENT_IBUS,
                                                        &s_sc.ibus_raw));
    (void)App_SC8815_Check(Int_SC8815_ReadAdcCurrentRaw(INT_SC8815_CURRENT_IBAT,
                                                        &s_sc.ibat_raw));
}

static uint8_t App_SC8815_ReadPinLevel(GPIO_TypeDef *port, uint16_t pin)
{
    return (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET) ? 1u : 0u;
}

static void App_SC8815_UpdateDebugRegs(void)
{
    s_sc.ce_n_pin = App_SC8815_ReadPinLevel(SC8815_CE_N_GPIO_Port,
                                            SC8815_CE_N_Pin);
    s_sc.pstop_pin = App_SC8815_ReadPinLevel(SC8815_PSTOP_GPIO_Port,
                                             SC8815_PSTOP_Pin);

    (void)App_SC8815_Check(Int_SC8815_ReadReg(SC8815_REG_VBAT_SET,
                                              &s_sc.vbat_set_reg));
    (void)App_SC8815_Check(Int_SC8815_ReadReg(SC8815_REG_RATIO,
                                              &s_sc.ratio_reg));
    (void)App_SC8815_Check(Int_SC8815_ReadReg(SC8815_REG_IBUS_LIM_SET,
                                              &s_sc.ibus_lim_reg));
    (void)App_SC8815_Check(Int_SC8815_ReadReg(SC8815_REG_IBAT_LIM_SET,
                                              &s_sc.ibat_lim_reg));
    (void)App_SC8815_Check(Int_SC8815_ReadReg(SC8815_REG_VINREG_SET,
                                              &s_sc.vinreg_reg));
    (void)App_SC8815_Check(Int_SC8815_ReadReg(SC8815_REG_CTRL0_SET,
                                              &s_sc.ctrl0_reg));
    (void)App_SC8815_Check(Int_SC8815_ReadReg(SC8815_REG_CTRL1_SET,
                                              &s_sc.ctrl1_reg));
    (void)App_SC8815_Check(Int_SC8815_ReadReg(SC8815_REG_CTRL2_SET,
                                              &s_sc.ctrl2_reg));
    (void)App_SC8815_Check(Int_SC8815_ReadReg(SC8815_REG_CTRL3_SET,
                                              &s_sc.ctrl3_reg));
    (void)App_SC8815_Check(Int_SC8815_ReadReg(SC8815_REG_MASK,
                                              &s_sc.mask_reg));
}

/**
 * @brief 输出 SC8815 紧凑调试信息。
 *
 * 安全审查重点看 `stby`：`stby=1` 表示功率环路停止，`stby=0` 表示
 * 已经明确请求并进入充电工作态。
 */
static void App_SC8815_PrintDebug(void)
{
    uint32_t ibus_limit_ma;
    uint32_t ibat_limit_ma;

    App_SC8815_UpdateDebugRegs();
    ibus_limit_ma = (((uint32_t)s_sc.ibus_lim_reg + SC8815_CURRENT_LIMIT_CODE_OFFSET) *
                     SC8815_PROJECT_IBUS_RATIO_X *
                     SC8815_CURRENT_LIMIT_REF_RSENSE_MOHM *
                     1000u) /
                    (SC8815_CURRENT_LIMIT_CODE_DENOMINATOR *
                     SC8815_PROJECT_RSNS_IBUS_MOHM);
    ibat_limit_ma = (((uint32_t)s_sc.ibat_lim_reg + SC8815_CURRENT_LIMIT_CODE_OFFSET) *
                     SC8815_PROJECT_IBAT_RATIO_X *
                     SC8815_CURRENT_LIMIT_REF_RSENSE_MOHM *
                     1000u) /
                    (SC8815_CURRENT_LIMIT_CODE_DENOMINATOR *
                     SC8815_PROJECT_RSNS_IBAT_MOHM);

    printf("SC 通信:%u 错:%u 交换:%u 总线:%02x 请求:%u 使能:%u 待机:%u 状态:%02x AC:%u 短路:%u 过温:%u 充满:%u VBUS:%lu VBAT:%lu IBUS:%lu IBAT:%lu\r\n",
           s_sc.comm_ok ? 1u : 0u,
           (unsigned int)s_sc.last_error,
           Int_SC8815_IsIicLineSwapped() ? 1u : 0u,
           (unsigned int)Int_SC8815_GetBusLevels(),
           s_sc.charge_requested ? 1u : 0u,
           s_sc.chip_enabled ? 1u : 0u,
           s_sc.standby ? 1u : 0u,
           (unsigned int)s_sc.status_raw,
           s_sc.ac_ok ? 1u : 0u,
           s_sc.vbus_short ? 1u : 0u,
           s_sc.otp ? 1u : 0u,
           s_sc.eoc ? 1u : 0u,
           (unsigned long)s_sc.vbus_mv,
           (unsigned long)s_sc.vbat_mv,
           (unsigned long)s_sc.ibus_ma,
           (unsigned long)s_sc.ibat_ma);
    printf("SC硬件 CE_N:%u PSTOP:%u REG VBAT:%02x RATIO:%02x LIM:%02x/%02x CTRL:%02x/%02x/%02x/%02x MASK:%02x\r\n",
           (unsigned int)s_sc.ce_n_pin,
           (unsigned int)s_sc.pstop_pin,
           (unsigned int)s_sc.vbat_set_reg,
           (unsigned int)s_sc.ratio_reg,
           (unsigned int)s_sc.ibus_lim_reg,
           (unsigned int)s_sc.ibat_lim_reg,
           (unsigned int)s_sc.ctrl0_reg,
           (unsigned int)s_sc.ctrl1_reg,
           (unsigned int)s_sc.ctrl2_reg,
           (unsigned int)s_sc.ctrl3_reg,
           (unsigned int)s_sc.mask_reg);
    printf("SC原始ADC VBUS:%u VBAT:%u IBUS:%u IBAT:%u 限流换算:%lu/%lu mA\r\n",
           (unsigned int)s_sc.vbus_raw,
           (unsigned int)s_sc.vbat_raw,
           (unsigned int)s_sc.ibus_raw,
           (unsigned int)s_sc.ibat_raw,
           (unsigned long)ibus_limit_ma,
           (unsigned long)ibat_limit_ma);
    printf("SC配置 VINREG:%02x 状态位 AC_OK:%u INDET:%u VBUS_SHORT:%u OTP:%u EOC:%u RSV:%02x\r\n",
           (unsigned int)s_sc.vinreg_reg,
           APP_SC8815_BIT(s_sc.status_raw, SC8815_STATUS_AC_OK_MASK),
           APP_SC8815_BIT(s_sc.status_raw, SC8815_STATUS_INDET_MASK),
           APP_SC8815_BIT(s_sc.status_raw, SC8815_STATUS_VBUS_SHORT_MASK),
           APP_SC8815_BIT(s_sc.status_raw, SC8815_STATUS_OTP_MASK),
           APP_SC8815_BIT(s_sc.status_raw, SC8815_STATUS_EOC_MASK),
           (unsigned int)(s_sc.status_raw & SC8815_STATUS_RESERVED_MASK));
    printf("SC控制位 OTG:%u PGATE开:%u GPO低:%u ADC:%u FACTORY:%u IBAT基准:%u 禁涓流:%u 禁终止:%u 禁短路折返:%u MASK:%02x\r\n",
           APP_SC8815_BIT(s_sc.ctrl0_reg, SC8815_CTRL0_SET_EN_OTG_MASK),
           APP_SC8815_BIT(s_sc.ctrl3_reg, SC8815_CTRL3_SET_EN_PGATE_MASK),
           APP_SC8815_BIT(s_sc.ctrl3_reg, SC8815_CTRL3_SET_GPO_CTRL_MASK),
           APP_SC8815_BIT(s_sc.ctrl3_reg, SC8815_CTRL3_SET_AD_START_MASK),
           APP_SC8815_BIT(s_sc.ctrl2_reg, SC8815_CTRL2_SET_FACTORY_MASK),
           APP_SC8815_BIT(s_sc.ctrl1_reg, SC8815_CTRL1_SET_ICHAR_SEL_MASK),
           APP_SC8815_BIT(s_sc.ctrl1_reg, SC8815_CTRL1_SET_DIS_TRICKLE_MASK),
           APP_SC8815_BIT(s_sc.ctrl1_reg, SC8815_CTRL1_SET_DIS_TERM_MASK),
           APP_SC8815_BIT(s_sc.ctrl3_reg, SC8815_CTRL3_SET_DIS_SHORT_FOLDBACK_MASK),
           (unsigned int)s_sc.mask_reg);
}

/**
 * @brief 初始化 SC8815 APP 层。
 *
 * 初始化只进入 standby monitor，不自动启动充电。若寄存器通信失败，
 * 串口中的 `ok:0` 会提示排查软 I2C、供电或芯片状态。
 */
void App_SC8815_Init(void)
{
    /*
     * 先复位本地快照，再执行硬件安全初始化。即使首次寄存器读失败，
     * 上层也能看到确定状态。
     */
    s_sc.comm_ok = true;
    s_sc.charge_requested = false;
    s_sc.chip_enabled = true;
    s_sc.standby = true;
    s_sc.ac_ok = false;
    s_sc.indet = false;
    s_sc.vbus_short = false;
    s_sc.otp = false;
    s_sc.eoc = false;
    s_sc.status_raw = 0u;
    s_sc.vbus_mv = 0u;
    s_sc.vbat_mv = 0u;
    s_sc.ibus_ma = 0u;
    s_sc.ibat_ma = 0u;
    s_sc.vbus_raw = 0u;
    s_sc.vbat_raw = 0u;
    s_sc.ibus_raw = 0u;
    s_sc.ibat_raw = 0u;
    s_sc.last_error = INT_SC8815_OK;
    s_sc.ce_n_pin = 1u;
    s_sc.pstop_pin = 1u;
    s_sc.vbat_set_reg = 0u;
    s_sc.ratio_reg = 0u;
    s_sc.ibus_lim_reg = 0u;
    s_sc.ibat_lim_reg = 0u;
    s_sc.vinreg_reg = 0u;
    s_sc.ctrl0_reg = 0u;
    s_sc.ctrl1_reg = 0u;
    s_sc.ctrl2_reg = 0u;
    s_sc.ctrl3_reg = 0u;
    s_sc.mask_reg = 0u;
    s_charge_queue = NULL;
    s_debug_ms = 0u;

    /*
     * InitSafe 先输出 PSTOP=1、CE_N=1，随后只使能芯片并保持 PSTOP=1，
     * 形成“可通信但不充电”的监控态。
     */
    (void)Int_SC8815_InitSafe();
    App_SC8815_SetStandbyMonitor();

    /*
     * standby monitor 下启用 ADC，用于串口观察 VBUS/VBAT/IBUS/IBAT；
     * 该动作不等价于启动充电。
     */
    (void)App_SC8815_Check(Int_SC8815_SetAdcEnabled(true));
    App_SC8815_Sample();
    printf("SC初始化待机监控 通信:%u\r\n", s_sc.comm_ok ? 1u : 0u);
}

/**
 * @brief SC8815 周期任务。
 *
 * 先采样再应用状态机，保证最新 short/OTP/通信故障能阻止充电请求。
 */
void App_SC8815_Task(uint16_t interval_ms)
{
    App_SC8815_TaskInitQueue();
    App_SC8815_Sample();
    App_SC8815_LoadChargeRequest();
    App_SC8815_ApplyChargeRequest();

    s_debug_ms = (uint16_t)(s_debug_ms + interval_ms);
    if (s_debug_ms >= APP_SC8815_DEBUG_PERIOD_MS)
    {
        s_debug_ms = 0u;
        App_SC8815_PrintDebug();
    }
}

/**
 * @brief 请求启动或停止 SC8815 充电。
 *
 * 这是唯一可能让 SC8815 离开 standby 的公开入口；其他模块不应直接写
 * CE_N/PSTOP 或 SC8815 寄存器来启动功率环路。
 */
void App_SC8815_RequestCharge(bool enable)
{
    uint8_t request = enable ? 1u : 0u;

    if (s_charge_queue == NULL)
    {
        s_sc.charge_requested = enable;
        return;
    }

    if (xQueueOverwrite(s_charge_queue, &request) != pdPASS)
    {
        s_sc.charge_requested = enable;
    }
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
    return (!s_sc.comm_ok || s_sc.vbus_short || s_sc.otp);
}

bool App_SC8815_IsCharging(void)
{
    return (s_sc.charge_requested && s_sc.chip_enabled && !s_sc.standby);
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
    return SC8815_PROJECT_BRINGUP_IBUS_LIMIT_MA;
}
