#include "App_BatMan_Internal.h"

#include <stddef.h>

#include "App_Safety.h"
#include "Int_BQ76952.h"
#include "Int_BQ76952_BSP.h"
#include "Int_Log.h"
#include "main.h"

/**
 * @file App_BatMan_Config.c
 * @brief BQ76952 静态配置清单、配置回读和主 FET 闭环控制。
 *
 * 配置清单是唯一写入来源；退出 CONFIG_UPDATE 后必须逐项读回完全一致，
 * 才允许上层释放主功率路径。启动配置失败会锁存到 MCU 重启；运行期证明丢失
 * 则转入 RECOVERY_REQUIRED，只有完整重认证成功才能恢复 VALID。
 */

enum
{
    APP_BATMAN_DM_DA_CONFIGURATION_DEFAULT = 0x05u,
    APP_BATMAN_DM_PROTECTION_CONFIGURATION_DEFAULT = 0x0002u,
    APP_BATMAN_DM_ENABLED_PROTECTIONS_A_DEFAULT =
        BQ76952_ENABLED_PROTECTIONS_A_SCD_MASK | BQ76952_ENABLED_PROTECTIONS_A_OCD2_MASK |
        BQ76952_ENABLED_PROTECTIONS_A_OCD1_MASK | BQ76952_ENABLED_PROTECTIONS_A_OCC_MASK |
        BQ76952_ENABLED_PROTECTIONS_A_CUV_MASK | BQ76952_ENABLED_PROTECTIONS_A_COV_MASK,
    APP_BATMAN_DM_ENABLED_PROTECTIONS_B_DEFAULT =
        BQ76952_ENABLED_PROTECTIONS_B_OTD_MASK | BQ76952_ENABLED_PROTECTIONS_B_OTC_MASK,
    APP_BATMAN_DM_ENABLED_PROTECTIONS_C_DEFAULT = 0x00u,
    APP_BATMAN_DM_CHG_FET_PROTECTIONS_A_DEFAULT = 0x98u,
    APP_BATMAN_DM_CHG_FET_PROTECTIONS_B_DEFAULT = 0xD5u,
    APP_BATMAN_DM_CHG_FET_PROTECTIONS_C_DEFAULT = 0x56u,
    APP_BATMAN_DM_DSG_FET_PROTECTIONS_A_DEFAULT = 0xE4u,
    APP_BATMAN_DM_DSG_FET_PROTECTIONS_B_DEFAULT = 0xE6u,
    APP_BATMAN_DM_DSG_FET_PROTECTIONS_C_DEFAULT = 0xE2u,
    APP_BATMAN_DM_DEFAULT_ALARM_MASK_DEFAULT = BQ76952_ALARM_SAFETY_PIN_MASK,

    APP_BATMAN_DM_CUV_THRESHOLD_2V83 = 56u,
    APP_BATMAN_DM_CUV_DELAY_1S = 300u,
    APP_BATMAN_DM_CUV_RECOVERY_HYS_200MV = 4u,
    /*
     * EVE 50E 充电上限为 4.20V；量产误差链/HIL 标定完成前，硬件 COV 先取
     * 84 * 50.6mV = 4.250V 的保守候选值，不能用 4.30V 软件有效范围替代保护依据。
     */
    APP_BATMAN_DM_COV_THRESHOLD_4V25_HIL_PENDING = 84u,
    /* COV 延时/回差暂无项目标定值，保留 TI TRM 明确默认值 74/2。 */
    APP_BATMAN_DM_COV_DELAY_TI_DEFAULT = 74u,
    APP_BATMAN_DM_COV_RECOVERY_HYS_TI_DEFAULT = 2u,
    APP_BATMAN_DM_OCC_THRESHOLD_6A = 15u,
    APP_BATMAN_DM_OCC_DELAY_426MS = 127u,
    APP_BATMAN_DM_OCD1_THRESHOLD_14A = 35u,
    APP_BATMAN_DM_OCD1_DELAY_300MS = 89u,
    APP_BATMAN_DM_OCD2_THRESHOLD_15A2 = 38u,
    APP_BATMAN_DM_OCD2_DELAY_80MS = 22u,
    APP_BATMAN_DM_SCD_THRESHOLD_80MV = 0x04u,
    APP_BATMAN_DM_SCD_DELAY_400US = 0x1Cu,
    APP_BATMAN_DM_OCC_RECOVERY_NEG_200MA = 0xFF38u,
    APP_BATMAN_DM_OCD_RECOVERY_500MA = 500u,
    APP_BATMAN_DM_OTC_THRESHOLD_50C = 50u,
    APP_BATMAN_DM_OTC_DELAY_3S = 3u,
    APP_BATMAN_DM_OTC_RECOVERY_45C = 45u,
    APP_BATMAN_DM_OTD_THRESHOLD_60C = 60u,
    APP_BATMAN_DM_OTD_DELAY_3S = 3u,
    APP_BATMAN_DM_OTD_RECOVERY_55C = 55u,

    APP_BATMAN_DM_FET_OPTIONS_DEFAULT =
        BQ76952_FET_OPTIONS_FET_INIT_OFF_MASK | BQ76952_FET_OPTIONS_PDSG_EN_MASK |
        BQ76952_FET_OPTIONS_FET_CTRL_EN_MASK | BQ76952_FET_OPTIONS_HOST_FET_EN_MASK |
        BQ76952_FET_OPTIONS_SFET_MASK,
    APP_BATMAN_DM_CHG_PUMP_CONTROL_DEFAULT = 0x01u,
    APP_BATMAN_DM_PREDISCHARGE_TIMEOUT_2500MS = 250u,
    APP_BATMAN_DM_PREDISCHARGE_STOP_DELTA_DISABLED = 0u,
    APP_BATMAN_DM_BALANCING_CONFIGURATION_DEFAULT = 0x00u,
    APP_BATMAN_SAFE_OUTPUT_DEADLINE_MS = 50u,
    APP_BATMAN_SAFE_FET_ENABLE_DEADLINE_MS = 150u,
    /* 完整重认证占用一个跨 API 事务，且必须在 BatMan 2.5 s deadline 内收敛。 */
    APP_BATMAN_REAUTHENTICATION_DEADLINE_MS = 1500u,
    /* TRM p173/p201: PF_EN(bit6) + FET_EN(bit4)，复位后进入正常FET模式。 */
    APP_BATMAN_DM_MFG_STATUS_INIT_NORMAL = 0x0050u,
    APP_BATMAN_MAIN_FET_OFF_MASK =
        BQ76952_FET_CONTROL_PCHG_OFF_MASK | BQ76952_FET_CONTROL_CHG_OFF_MASK |
        BQ76952_FET_CONTROL_PDSG_OFF_MASK | BQ76952_FET_CONTROL_DSG_OFF_MASK,
    APP_BATMAN_MAIN_FET_STATUS_MASK =
        BQ76952_FET_STATUS_PDSG_FET_MASK | BQ76952_FET_STATUS_DSG_FET_MASK |
        BQ76952_FET_STATUS_PCHG_FET_MASK | BQ76952_FET_STATUS_CHG_FET_MASK
};

#define APP_BATMAN_DM_CC_GAIN_5_MOHM_IEEE754 (0x3FBF67F5u)
#define APP_BATMAN_DM_CAPACITY_GAIN_5_MOHM_IEEE754 (0x48D9C710u)

typedef struct
{
    uint16_t address;
    uint8_t length;
    uint8_t expected[4];
} App_BatMan_DmManifestItemTypeDef;

#define APP_BATMAN_DM_ITEM_U8(address_, value_) {(address_), 1u, {(uint8_t)(value_), 0u, 0u, 0u}}
#define APP_BATMAN_DM_ITEM_U16(address_, value_)                                                   \
    {(address_), 2u, {(uint8_t)((value_) & 0xFFu), (uint8_t)((value_) >> 8u), 0u, 0u}}
#define APP_BATMAN_DM_ITEM_U32(address_, value_)                                                   \
    {                                                                                              \
        (address_), 4u,                                                                            \
        {                                                                                          \
            (uint8_t)((value_) & 0xFFu), (uint8_t)(((value_) >> 8u) & 0xFFu),                      \
                (uint8_t)(((value_) >> 16u) & 0xFFu), (uint8_t)(((value_) >> 24u) & 0xFFu)         \
        }                                                                                          \
    }

static const App_BatMan_DmManifestItemTypeDef s_dm_manifest[] = {
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_ALERT_PIN_CONFIG, BQ76952_ALERT_PIN_ACTIVE_LOW_TRISTATE),
    /* TS1 使用 TI 默认 18k 模型；实装 NTC 型号、位置及整机误差仍须 HIL 确认。 */
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_TS1_CONFIG, BQ76952_TS_CONFIG_18K_CELL_PROTECTION),
    /* TS3 实装 NTC/位置/模型 Unknown，显式保持禁用，采样层不得把它当有效温度。 */
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_TS3_CONFIG, BQ76952_TS_CONFIG_DISABLED),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_DA_CONFIGURATION, APP_BATMAN_DM_DA_CONFIGURATION_DEFAULT),
    APP_BATMAN_DM_ITEM_U16(BQ76952_DM_VCELL_MODE, BQ76952_CELL_MASK_6S_HW_CONFIRMED),
    APP_BATMAN_DM_ITEM_U32(BQ76952_DM_CC_GAIN, APP_BATMAN_DM_CC_GAIN_5_MOHM_IEEE754),
    APP_BATMAN_DM_ITEM_U32(BQ76952_DM_CAPACITY_GAIN, APP_BATMAN_DM_CAPACITY_GAIN_5_MOHM_IEEE754),
    APP_BATMAN_DM_ITEM_U16(BQ76952_DM_PROTECTION_CONFIGURATION,
                           APP_BATMAN_DM_PROTECTION_CONFIGURATION_DEFAULT),
    APP_BATMAN_DM_ITEM_U16(BQ76952_DM_DEFAULT_ALARM_MASK, APP_BATMAN_DM_DEFAULT_ALARM_MASK_DEFAULT),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_CUV_THRESHOLD, APP_BATMAN_DM_CUV_THRESHOLD_2V83),
    APP_BATMAN_DM_ITEM_U16(BQ76952_DM_CUV_DELAY, APP_BATMAN_DM_CUV_DELAY_1S),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_CUV_RECOVERY_HYSTERESIS, APP_BATMAN_DM_CUV_RECOVERY_HYS_200MV),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_COV_THRESHOLD, APP_BATMAN_DM_COV_THRESHOLD_4V25_HIL_PENDING),
    APP_BATMAN_DM_ITEM_U16(BQ76952_DM_COV_DELAY, APP_BATMAN_DM_COV_DELAY_TI_DEFAULT),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_COV_RECOVERY_HYSTERESIS,
                          APP_BATMAN_DM_COV_RECOVERY_HYS_TI_DEFAULT),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_OCC_THRESHOLD, APP_BATMAN_DM_OCC_THRESHOLD_6A),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_OCC_DELAY, APP_BATMAN_DM_OCC_DELAY_426MS),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_OCD1_THRESHOLD, APP_BATMAN_DM_OCD1_THRESHOLD_14A),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_OCD1_DELAY, APP_BATMAN_DM_OCD1_DELAY_300MS),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_OCD2_THRESHOLD, APP_BATMAN_DM_OCD2_THRESHOLD_15A2),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_OCD2_DELAY, APP_BATMAN_DM_OCD2_DELAY_80MS),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_SCD_THRESHOLD, APP_BATMAN_DM_SCD_THRESHOLD_80MV),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_SCD_DELAY, APP_BATMAN_DM_SCD_DELAY_400US),
    APP_BATMAN_DM_ITEM_U16(BQ76952_DM_OCC_RECOVERY_THRESHOLD, APP_BATMAN_DM_OCC_RECOVERY_NEG_200MA),
    APP_BATMAN_DM_ITEM_U16(BQ76952_DM_OCD_RECOVERY_THRESHOLD, APP_BATMAN_DM_OCD_RECOVERY_500MA),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_OTC_THRESHOLD, APP_BATMAN_DM_OTC_THRESHOLD_50C),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_OTC_DELAY, APP_BATMAN_DM_OTC_DELAY_3S),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_OTC_RECOVERY, APP_BATMAN_DM_OTC_RECOVERY_45C),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_OTD_THRESHOLD, APP_BATMAN_DM_OTD_THRESHOLD_60C),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_OTD_DELAY, APP_BATMAN_DM_OTD_DELAY_3S),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_OTD_RECOVERY, APP_BATMAN_DM_OTD_RECOVERY_55C),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_FET_OPTIONS, APP_BATMAN_DM_FET_OPTIONS_DEFAULT),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_CHG_PUMP_CONTROL, APP_BATMAN_DM_CHG_PUMP_CONTROL_DEFAULT),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_PREDISCHARGE_TIMEOUT,
                          APP_BATMAN_DM_PREDISCHARGE_TIMEOUT_2500MS),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_PREDISCHARGE_STOP_DELTA,
                          APP_BATMAN_DM_PREDISCHARGE_STOP_DELTA_DISABLED),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_BALANCING_CONFIGURATION,
                          APP_BATMAN_DM_BALANCING_CONFIGURATION_DEFAULT),
    APP_BATMAN_DM_ITEM_U16(BQ76952_DM_MFG_STATUS_INIT, APP_BATMAN_DM_MFG_STATUS_INIT_NORMAL),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_ENABLED_PROTECTIONS_A,
                          APP_BATMAN_DM_ENABLED_PROTECTIONS_A_DEFAULT),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_ENABLED_PROTECTIONS_B,
                          APP_BATMAN_DM_ENABLED_PROTECTIONS_B_DEFAULT),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_ENABLED_PROTECTIONS_C,
                          APP_BATMAN_DM_ENABLED_PROTECTIONS_C_DEFAULT),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_CHG_FET_PROTECTIONS_A,
                          APP_BATMAN_DM_CHG_FET_PROTECTIONS_A_DEFAULT),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_CHG_FET_PROTECTIONS_B,
                          APP_BATMAN_DM_CHG_FET_PROTECTIONS_B_DEFAULT),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_CHG_FET_PROTECTIONS_C,
                          APP_BATMAN_DM_CHG_FET_PROTECTIONS_C_DEFAULT),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_DSG_FET_PROTECTIONS_A,
                          APP_BATMAN_DM_DSG_FET_PROTECTIONS_A_DEFAULT),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_DSG_FET_PROTECTIONS_B,
                          APP_BATMAN_DM_DSG_FET_PROTECTIONS_B_DEFAULT),
    APP_BATMAN_DM_ITEM_U8(BQ76952_DM_DSG_FET_PROTECTIONS_C,
                          APP_BATMAN_DM_DSG_FET_PROTECTIONS_C_DEFAULT)};

static App_BatMan_ConfigStateTypeDef s_config_state = APP_BATMAN_CONFIG_UNCHECKED;
static bool s_config_invalid_latched = false;
static App_BatMan_FetControlStateTypeDef s_fet_control_state;

static bool App_BatMan_BytesEqual(const uint8_t *left, const uint8_t *right, uint8_t length)
{
    uint8_t i;

    for (i = 0u; i < length; i++)
    {
        if (left[i] != right[i])
        {
            return false;
        }
    }
    return true;
}

void App_BatMan_ResetConfigState(void)
{
    s_config_state = APP_BATMAN_CONFIG_UNCHECKED;
    s_config_invalid_latched = false;
    s_fet_control_state.desired_off_mask = (uint8_t)APP_BATMAN_MAIN_FET_OFF_MASK;
    s_fet_control_state.commanded_off_mask = (uint8_t)APP_BATMAN_MAIN_FET_OFF_MASK;
    s_fet_control_state.observed_fet_status = 0u;
    s_fet_control_state.observed_valid = false;
    s_fet_control_state.request_valid = false;
}

void App_BatMan_LatchConfigInvalid(void)
{
    s_config_invalid_latched = true;
    s_config_state = APP_BATMAN_CONFIG_INVALID_LATCHED;
    s_fault_flags |= APP_BATMAN_FAULT_CONFIG_INVALID;
    fault_active = true;
}

void App_BatMan_MarkConfigRecoveryRequired(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    if (!s_config_invalid_latched)
    {
        s_config_state = APP_BATMAN_CONFIG_RECOVERY_REQUIRED;
    }
    s_fet_control_state.desired_off_mask = (uint8_t)APP_BATMAN_MAIN_FET_OFF_MASK;
    s_fet_control_state.observed_valid = false;
    s_fet_control_state.request_valid = false;
    s_fault_flags |= APP_BATMAN_FAULT_CONFIG_INVALID | APP_BATMAN_FAULT_FET_CONTROL_INVALID;
    fault_active = true;
    if (primask == 0u)
    {
        __enable_irq();
    }
}

bool App_BatMan_IsConfigValid(void)
{
    return (!s_config_invalid_latched && (s_config_state == APP_BATMAN_CONFIG_VALID));
}

App_BatMan_ConfigStateTypeDef App_BatMan_GetConfigState(void)
{
    return s_config_state;
}

bool App_BatMan_ConfigBq(void)
{
    size_t i;

    s_config_state = APP_BATMAN_CONFIG_WRITING;
    for (i = 0u; i < (sizeof(s_dm_manifest) / sizeof(s_dm_manifest[0])); i++)
    {
        const App_BatMan_DmManifestItemTypeDef *item = &s_dm_manifest[i];

        if (Int_BQ76952_WriteDataMemory(item->address, item->expected, item->length) !=
            INT_BQ76952_OK)
        {
            s_comm_fault = true;
            Int_Log_Printf("BQ配置写入失败 地址:0x%04x 长度:%u\r\n",
                           (unsigned int)item->address,
                           (unsigned int)item->length);
            App_BatMan_MarkConfigRecoveryRequired();
            return false;
        }
    }

    return true;
}

bool App_BatMan_VerifyBqConfig(void)
{
    size_t i;

    s_config_state = APP_BATMAN_CONFIG_VERIFYING;
    for (i = 0u; i < (sizeof(s_dm_manifest) / sizeof(s_dm_manifest[0])); i++)
    {
        const App_BatMan_DmManifestItemTypeDef *item = &s_dm_manifest[i];
        uint8_t actual[4] = {0u, 0u, 0u, 0u};
        Int_BQ76952_StatusTypeDef ret;

        ret = Int_BQ76952_ReadDataMemory(item->address, actual, item->length);
        if (ret != INT_BQ76952_OK)
        {
            s_comm_fault = true;
            Int_Log_Printf(
                "BQ配置回读失败 地址:0x%04x ret:%d\r\n", (unsigned int)item->address, (int)ret);
            App_BatMan_MarkConfigRecoveryRequired();
            return false;
        }
        if (!App_BatMan_BytesEqual(actual, item->expected, item->length))
        {
            Int_Log_Printf("BQ配置不一致 地址:0x%04x exp:%02x%02x%02x%02x act:%02x%02x%02x%02x\r\n",
                           (unsigned int)item->address,
                           item->expected[3],
                           item->expected[2],
                           item->expected[1],
                           item->expected[0],
                           actual[3],
                           actual[2],
                           actual[1],
                           actual[0]);
            App_BatMan_MarkConfigRecoveryRequired();
            return false;
        }
    }

    if (s_config_invalid_latched)
    {
        App_BatMan_MarkConfigRecoveryRequired();
        return false;
    }

    s_config_state = APP_BATMAN_CONFIG_VALID;
    s_fault_flags &= ~APP_BATMAN_FAULT_CONFIG_INVALID;
    return true;
}

void App_BatMan_GetFetControlState(App_BatMan_FetControlStateTypeDef *state)
{
    uint32_t primask;

    if (state == NULL)
    {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    *state = s_fet_control_state;
    if (primask == 0u)
    {
        __enable_irq();
    }
}

static uint8_t App_BatMan_ForbiddenObservedFets(uint8_t off_mask)
{
    uint8_t forbidden = 0u;

    if ((off_mask & BQ76952_FET_CONTROL_PCHG_OFF_MASK) != 0u)
    {
        forbidden |= BQ76952_FET_STATUS_PCHG_FET_MASK;
    }
    if ((off_mask & BQ76952_FET_CONTROL_CHG_OFF_MASK) != 0u)
    {
        forbidden |= BQ76952_FET_STATUS_CHG_FET_MASK;
    }
    if ((off_mask & BQ76952_FET_CONTROL_PDSG_OFF_MASK) != 0u)
    {
        forbidden |= BQ76952_FET_STATUS_PDSG_FET_MASK;
    }
    if ((off_mask & BQ76952_FET_CONTROL_DSG_OFF_MASK) != 0u)
    {
        forbidden |= BQ76952_FET_STATUS_DSG_FET_MASK;
    }
    return forbidden;
}

bool App_BatMan_ObserveFetStatus(uint8_t observed_status)
{
    uint8_t forbidden;
    uint32_t primask = __get_PRIMASK();
    bool valid;

    __disable_irq();
    forbidden = App_BatMan_ForbiddenObservedFets(s_fet_control_state.commanded_off_mask);
    s_fet_control_state.observed_fet_status = observed_status;
    s_fet_control_state.observed_valid = true;
    valid = s_fet_control_state.request_valid && ((observed_status & forbidden) == 0u);
    if (!valid)
    {
        /* 只允许显式 FET_CONTROL 成功后恢复 request_valid。 */
        s_fet_control_state.request_valid = false;
    }
    if (primask == 0u)
    {
        __enable_irq();
    }
    return valid;
}

bool App_BatMan_PreResetAllFetsOff(void)
{
    uint8_t observed = 0u;
    Int_BQ76952_StatusTypeDef ret;
    bool transaction_started = false;

    s_fet_control_state.desired_off_mask = (uint8_t)APP_BATMAN_MAIN_FET_OFF_MASK;
    s_fet_control_state.observed_valid = false;
    s_fet_control_state.request_valid = false;
    fet_control_request = (uint8_t)APP_BATMAN_MAIN_FET_OFF_MASK;

    ret = Int_BQ76952_BeginTransaction(APP_BATMAN_SAFE_OUTPUT_DEADLINE_MS);
    if (ret == INT_BQ76952_OK)
    {
        transaction_started = true;
        ret = Int_BQ76952_SendSubcommand(BQ76952_SUBCMD_ALL_FETS_OFF);
    }
    if (ret == INT_BQ76952_OK)
    {
        ret = Int_BQ76952_ReadDirect(BQ76952_CMD_FET_STATUS, &observed, 1u);
    }
    if (transaction_started)
    {
        Int_BQ76952_EndTransaction();
    }
    if (ret == INT_BQ76952_OK)
    {
        s_fet_control_state.commanded_off_mask = (uint8_t)APP_BATMAN_MAIN_FET_OFF_MASK;
        s_fet_control_state.observed_fet_status = observed;
        s_fet_control_state.observed_valid = true;
        fet_status = observed;
    }
    if ((ret != INT_BQ76952_OK) || ((observed & (uint8_t)APP_BATMAN_MAIN_FET_STATUS_MASK) != 0u))
    {
        s_fault_flags |= APP_BATMAN_FAULT_FET_CONTROL_INVALID;
        fault_active = true;
        App_BatMan_MarkConfigRecoveryRequired();
        return false;
    }

    /* 这里只确认 RESET 前物理 FET 已关；FET_CONTROL 闭环仍在配置验证后执行。 */
    s_fet_control_state.request_valid = true;
    return true;
}

static void App_BatMan_RequestAllFetsOffAfterFailure(void)
{
    uint8_t observed = 0u;
    Int_BQ76952_StatusTypeDef ret;
    bool transaction_started = false;

    s_fet_control_state.desired_off_mask = (uint8_t)APP_BATMAN_MAIN_FET_OFF_MASK;
    s_fet_control_state.observed_valid = false;
    s_fet_control_state.request_valid = false;
    fet_control_request = (uint8_t)APP_BATMAN_MAIN_FET_OFF_MASK;

    /*
     * ALL_FETS_OFF 先建立独立的锁存 blocker；即使后续 FET_CONTROL 传输失败，
     * BQ 仍不得因保护恢复而自行打开主 FET。两条命令继承调用方的外层 deadline。
     */
    ret = Int_BQ76952_BeginTransaction(APP_BATMAN_SAFE_OUTPUT_DEADLINE_MS);
    if (ret == INT_BQ76952_OK)
    {
        transaction_started = true;
        ret = Int_BQ76952_SendSubcommand(BQ76952_SUBCMD_ALL_FETS_OFF);
    }
    if (ret == INT_BQ76952_OK)
    {
        ret = Int_BQ76952_ApplyFetControl((uint8_t)APP_BATMAN_MAIN_FET_OFF_MASK, &observed);
    }
    if (transaction_started)
    {
        Int_BQ76952_EndTransaction();
    }
    if (ret == INT_BQ76952_OK)
    {
        s_fet_control_state.commanded_off_mask = (uint8_t)APP_BATMAN_MAIN_FET_OFF_MASK;
        s_fet_control_state.observed_fet_status = observed;
        s_fet_control_state.observed_valid = true;
        fet_status = observed;
    }
    s_fault_flags |= APP_BATMAN_FAULT_FET_CONTROL_INVALID;
    fault_active = true;
}

static Int_BQ76952_StatusTypeDef App_BatMan_WriteMainFetControl(uint8_t off_mask,
                                                                uint32_t safety_authorization_epoch)
{
    uint8_t data[2];
    uint8_t observed = 0u;
    uint16_t mfg_status;
    Int_BQ76952_StatusTypeDef ret;

    s_fet_control_state.desired_off_mask = off_mask;
    s_fet_control_state.request_valid = false;

    if ((off_mask != (uint8_t)APP_BATMAN_MAIN_FET_OFF_MASK) &&
        (!App_BatMan_IsConfigValid() || fault_active))
    {
        App_BatMan_RequestAllFetsOffAfterFailure();
        return INT_BQ76952_ERROR;
    }

    if ((off_mask != (uint8_t)APP_BATMAN_MAIN_FET_OFF_MASK) &&
        !App_Safety_IsPowerReleaseAuthorized(safety_authorization_epoch))
    {
        /* 旧 epoch 只能收敛到全关，绝不能把 Safety 锁存绕过到 BQ owner。 */
        (void)App_BatMan_WriteMainFetControl((uint8_t)APP_BATMAN_MAIN_FET_OFF_MASK, 0u);
        return INT_BQ76952_ERROR;
    }

    /* FET_ENABLE 只允许由初始化/唤醒的 ALL_FETS_OFF 屏障序列执行。 */
    if (off_mask != (uint8_t)APP_BATMAN_MAIN_FET_OFF_MASK)
    {
        ret = Int_BQ76952_ReadSubcommand(BQ76952_SUBCMD_MANUFACTURING_STATUS, data, 2u);
        if (ret != INT_BQ76952_OK)
        {
            App_BatMan_RequestAllFetsOffAfterFailure();
            return ret;
        }

        mfg_status = App_BatMan_ReadU16Le(data);
        if ((mfg_status & BQ76952_MFG_STATUS_FET_EN_MASK) == 0u)
        {
            /*
             * 运行期绝不临时切换 ManufacturingStatus[FET_EN]。若该位丢失，
             * 必须回到完整安全恢复序列，先建立 ALL_FETS_OFF blocker 再启用。
             */
            App_BatMan_RequestAllFetsOffAfterFailure();
            return INT_BQ76952_ERROR;
        }
    }

    if ((off_mask != (uint8_t)APP_BATMAN_MAIN_FET_OFF_MASK) &&
        !App_Safety_IsPowerReleaseAuthorized(safety_authorization_epoch))
    {
        (void)App_BatMan_WriteMainFetControl((uint8_t)APP_BATMAN_MAIN_FET_OFF_MASK, 0u);
        return INT_BQ76952_ERROR;
    }

    ret = Int_BQ76952_ApplyFetControl(off_mask, &observed);
    if (ret != INT_BQ76952_OK)
    {
        App_BatMan_RequestAllFetsOffAfterFailure();
        return ret;
    }

    s_fet_control_state.commanded_off_mask = off_mask;
    s_fet_control_state.observed_fet_status = observed;
    s_fet_control_state.observed_valid = true;
    fet_control_request = off_mask;
    fet_status = observed;

    if ((observed & App_BatMan_ForbiddenObservedFets(off_mask)) != 0u)
    {
        App_BatMan_RequestAllFetsOffAfterFailure();
        return INT_BQ76952_ERROR;
    }

    if ((off_mask != (uint8_t)APP_BATMAN_MAIN_FET_OFF_MASK) &&
        !App_Safety_IsPowerReleaseAuthorized(safety_authorization_epoch))
    {
        /* ALERT 可在 I2C 事务中到达；回读后必须复验并立即反写全关。 */
        (void)App_BatMan_WriteMainFetControl((uint8_t)APP_BATMAN_MAIN_FET_OFF_MASK, 0u);
        return INT_BQ76952_ERROR;
    }

    s_fet_control_state.request_valid = true;
    s_fault_flags &= ~APP_BATMAN_FAULT_FET_CONTROL_INVALID;
    return INT_BQ76952_OK;
}

bool App_BatMan_EnableFetControlSafely(void)
{
    uint8_t data[2];
    uint8_t observed = 0u;
    uint16_t mfg_status;
    Int_BQ76952_StatusTypeDef ret;
    bool transaction_started = false;

    /*
     * TRM p34：ALL_FETS_OFF 会建立锁存 blocker；TRM p173/p201：FET_ENABLE
     * 只切换 ManufacturingStatus[FET_EN]。因此先锁存并确认四路全关，再切换
     * FET_EN，且切换后再次确认 blocker 仍有效，最后才写完整 host off-mask。
     */
    ret = Int_BQ76952_BeginTransaction(APP_BATMAN_SAFE_FET_ENABLE_DEADLINE_MS);
    if (ret == INT_BQ76952_OK)
    {
        transaction_started = true;
        ret = Int_BQ76952_SendSubcommand(BQ76952_SUBCMD_ALL_FETS_OFF);
    }
    if (ret == INT_BQ76952_OK)
    {
        ret = Int_BQ76952_ReadDirect(BQ76952_CMD_FET_STATUS, &observed, 1u);
    }
    if ((ret == INT_BQ76952_OK) && ((observed & (uint8_t)APP_BATMAN_MAIN_FET_STATUS_MASK) != 0u))
    {
        ret = INT_BQ76952_ERROR;
    }
    if (ret == INT_BQ76952_OK)
    {
        ret = Int_BQ76952_ReadSubcommand(BQ76952_SUBCMD_MANUFACTURING_STATUS, data, 2u);
    }
    if (ret == INT_BQ76952_OK)
    {
        mfg_status = App_BatMan_ReadU16Le(data);
        if ((mfg_status & BQ76952_MFG_STATUS_FET_EN_MASK) == 0u)
        {
            ret = Int_BQ76952_SendSubcommand(BQ76952_SUBCMD_FET_ENABLE);
        }
    }
    if (ret == INT_BQ76952_OK)
    {
        ret = Int_BQ76952_ReadSubcommand(BQ76952_SUBCMD_MANUFACTURING_STATUS, data, 2u);
    }
    if (ret == INT_BQ76952_OK)
    {
        mfg_status = App_BatMan_ReadU16Le(data);
        if ((mfg_status & BQ76952_MFG_STATUS_FET_EN_MASK) == 0u)
        {
            ret = INT_BQ76952_ERROR;
        }
    }
    if (ret == INT_BQ76952_OK)
    {
        ret = Int_BQ76952_ReadDirect(BQ76952_CMD_FET_STATUS, &observed, 1u);
    }
    if ((ret == INT_BQ76952_OK) && ((observed & (uint8_t)APP_BATMAN_MAIN_FET_STATUS_MASK) != 0u))
    {
        ret = INT_BQ76952_ERROR;
    }
    if (ret == INT_BQ76952_OK)
    {
        ret = App_BatMan_WriteMainFetControl((uint8_t)APP_BATMAN_MAIN_FET_OFF_MASK, 0u);
    }

    if (ret != INT_BQ76952_OK)
    {
        if (transaction_started)
        {
            App_BatMan_RequestAllFetsOffAfterFailure();
        }
        else
        {
            s_fet_control_state.desired_off_mask = (uint8_t)APP_BATMAN_MAIN_FET_OFF_MASK;
            s_fet_control_state.request_valid = false;
            s_fault_flags |= APP_BATMAN_FAULT_FET_CONTROL_INVALID;
            fault_active = true;
        }
        s_comm_fault = true;
        App_BatMan_MarkConfigRecoveryRequired();
    }

    if (transaction_started)
    {
        Int_BQ76952_EndTransaction();
    }
    return (ret == INT_BQ76952_OK);
}

Int_BQ76952_StatusTypeDef App_BatMan_KeepMainFetsOff(void)
{
    return App_BatMan_WriteMainFetControl((uint8_t)APP_BATMAN_MAIN_FET_OFF_MASK, 0u);
}

bool App_BatMan_SetMainFets(bool charge_enable,
                            bool discharge_enable,
                            uint32_t safety_authorization_epoch)
{
    uint8_t off_mask = 0u;

    if (!charge_enable)
    {
        off_mask |= BQ76952_FET_CONTROL_PCHG_OFF_MASK | BQ76952_FET_CONTROL_CHG_OFF_MASK;
    }
    if (!discharge_enable)
    {
        off_mask |= BQ76952_FET_CONTROL_PDSG_OFF_MASK | BQ76952_FET_CONTROL_DSG_OFF_MASK;
    }

    if (App_BatMan_WriteMainFetControl(off_mask, safety_authorization_epoch) == INT_BQ76952_OK)
    {
        return true;
    }
    s_comm_fault = true;
    return false;
}

static bool App_BatMan_ReauthenticateAfterResetLocked(void)
{
    const uint16_t unsafe_raw_alarm_mask =
        BQ76952_ALARM_SAFETY_PIN_MASK | BQ76952_ALARM_SHUTV_MASK | BQ76952_ALARM_FUSE_MASK;
    uint8_t data[2];
    uint8_t fet;
    uint16_t device_number;
    uint16_t mfg_status;
    uint16_t raw_alarm;
    uint16_t status;

    if (s_config_invalid_latched)
    {
        return false;
    }
    App_BatMan_MarkConfigRecoveryRequired();
    if (!App_BatMan_PreResetAllFetsOff())
    {
        s_comm_fault = true;
        return false;
    }
    if (Int_BQ76952_ReadSubcommand(BQ76952_SUBCMD_DEVICE_NUMBER, data, 2u) != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        App_BatMan_MarkConfigRecoveryRequired();
        return false;
    }
    device_number = App_BatMan_ReadU16Le(data);
    if (device_number != BQ76952_DEVICE_NUMBER_EXPECTED)
    {
        App_BatMan_MarkConfigRecoveryRequired();
        (void)App_BatMan_KeepMainFetsOff();
        return false;
    }

    /*
     * POR 仅在退出 CONFIG_UPDATE 后清零。通信证明一旦丢失就完整重写 RAM manifest，
     * 不能用 MCU 启动期的缓存 VALID 猜测 BQ 在掉电/看门狗复位后仍保持配置。
     */
    if (Int_BQ76952_EnterConfigUpdate() != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        App_BatMan_MarkConfigRecoveryRequired();
        return false;
    }
    if (!App_BatMan_ConfigBq())
    {
        (void)Int_BQ76952_ExitConfigUpdate();
        (void)App_BatMan_KeepMainFetsOff();
        return false;
    }
    if (Int_BQ76952_ExitConfigUpdate() != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        App_BatMan_MarkConfigRecoveryRequired();
        (void)App_BatMan_KeepMainFetsOff();
        return false;
    }
    if (!App_BatMan_VerifyBqConfig())
    {
        (void)App_BatMan_KeepMainFetsOff();
        return false;
    }
    if (Int_BQ76952_SendSubcommand(BQ76952_SUBCMD_SLEEP_DISABLE) != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        App_BatMan_MarkConfigRecoveryRequired();
        (void)App_BatMan_KeepMainFetsOff();
        return false;
    }

    if (App_BatMan_ClearStartupAlarms() != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        App_BatMan_MarkConfigRecoveryRequired();
        (void)App_BatMan_KeepMainFetsOff();
        return false;
    }
    if (!App_BatMan_EnableFetControlSafely())
    {
        s_comm_fault = true;
        return false;
    }
    if (Int_BQ76952_ReadDirect(BQ76952_CMD_ALARM_RAW_STATUS, data, 2u) != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        App_BatMan_MarkConfigRecoveryRequired();
        return false;
    }
    raw_alarm = App_BatMan_ReadU16Le(data);
    if (Int_BQ76952_ReadDirect(BQ76952_CMD_BATTERY_STATUS, data, 2u) != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        App_BatMan_MarkConfigRecoveryRequired();
        return false;
    }
    status = App_BatMan_ReadU16Le(data);
    if (Int_BQ76952_ReadSubcommand(BQ76952_SUBCMD_MANUFACTURING_STATUS, data, 2u) != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        App_BatMan_MarkConfigRecoveryRequired();
        return false;
    }
    mfg_status = App_BatMan_ReadU16Le(data);
    if (Int_BQ76952_ReadDirect(BQ76952_CMD_FET_STATUS, &fet, 1u) != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        App_BatMan_MarkConfigRecoveryRequired();
        return false;
    }

    /* XCHG/XDSG 可由本函数主动全关产生，不得把观察状态误判为保护未解除。 */
    if (((status & (BQ76952_BATTERY_STATUS_SDM_MASK | BQ76952_BATTERY_STATUS_POR_MASK |
                    BQ76952_BATTERY_STATUS_CFGUPDATE_MASK)) != 0u) ||
        ((mfg_status & BQ76952_MFG_STATUS_FET_EN_MASK) == 0u) ||
        ((raw_alarm & unsafe_raw_alarm_mask) != 0u) ||
        ((fet & (BQ76952_FET_STATUS_CHG_FET_MASK | BQ76952_FET_STATUS_DSG_FET_MASK |
                 BQ76952_FET_STATUS_PCHG_FET_MASK | BQ76952_FET_STATUS_PDSG_FET_MASK)) != 0u))
    {
        App_BatMan_RequestAllFetsOffAfterFailure();
        App_BatMan_MarkConfigRecoveryRequired();
        return false;
    }

    return true;
}

bool App_BatMan_ReauthenticateAfterReset(void)
{
    bool authenticated;

    if (s_config_invalid_latched)
    {
        return false;
    }
    if (Int_BQ76952_BeginTransaction(APP_BATMAN_REAUTHENTICATION_DEADLINE_MS) != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        App_BatMan_MarkConfigRecoveryRequired();
        return false;
    }

    /* CONFIG_UPDATE 入/出与 manifest 写回读不得被其他 BQ 公开 API 插入。 */
    authenticated = App_BatMan_ReauthenticateAfterResetLocked();
    Int_BQ76952_EndTransaction();
    return authenticated;
}

bool App_BatMan_RequestShutdown(void)
{
    if (App_BatMan_KeepMainFetsOff() != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        return false;
    }
    if (!App_BatMan_NvmFlush())
    {
        Int_Log_Printf("SOH持久化关机保存失败，继续安全关机\r\n");
    }
    if (Int_BQ76952_Shutdown() != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        return false;
    }
    return true;
}

bool App_BatMan_AllMainFetsOff(void)
{
    if (App_BatMan_KeepMainFetsOff() == INT_BQ76952_OK)
    {
        return true;
    }
    s_comm_fault = true;
    return false;
}

Int_BQ76952_StatusTypeDef App_BatMan_ClearStartupAlarms(void)
{
    uint8_t data[2];
    const uint16_t mask = BQ76952_ALARM_INITSTART_MASK | BQ76952_ALARM_INITCOMP_MASK |
                          BQ76952_ALARM_FULLSCAN_MASK | BQ76952_ALARM_ADSCAN_MASK |
                          BQ76952_ALARM_WAKE_MASK;

    App_BatMan_WriteU16Le(mask, data);
    return Int_BQ76952_WriteDirect(BQ76952_CMD_ALARM_STATUS, data, 2u);
}
