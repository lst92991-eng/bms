#include "App_BatMan_Internal.h"

#include "Int_BQ76952.h"
#include "Int_BQ76952_BSP.h"

/*
 * 项目 Data Memory 基线值。
 * 这里只保留已经和硬件/bring-up 目标绑定的最小集合：6S 映射、主 FET
 * 默认关断、告警掩码、保护路由和均衡模式。最终阈值必须来自实验标定，
 * 不在 APP 层凭经验补齐。
 */
#define APP_BATMAN_DM_DA_CONFIGURATION_DEFAULT          (0x05u)
#define APP_BATMAN_DM_PROTECTION_CONFIGURATION_DEFAULT  (0x0002u)
#define APP_BATMAN_DM_ENABLED_PROTECTIONS_A_DEFAULT     (BQ76952_ENABLED_PROTECTIONS_A_SCD_MASK | \
                                                         BQ76952_ENABLED_PROTECTIONS_A_OCD2_MASK | \
                                                         BQ76952_ENABLED_PROTECTIONS_A_OCD1_MASK | \
                                                         BQ76952_ENABLED_PROTECTIONS_A_OCC_MASK | \
                                                         BQ76952_ENABLED_PROTECTIONS_A_CUV_MASK | \
                                                         BQ76952_ENABLED_PROTECTIONS_A_COV_MASK)
#define APP_BATMAN_DM_ENABLED_PROTECTIONS_B_DEFAULT     (BQ76952_ENABLED_PROTECTIONS_B_OTD_MASK | \
                                                         BQ76952_ENABLED_PROTECTIONS_B_OTC_MASK)
#define APP_BATMAN_DM_ENABLED_PROTECTIONS_C_DEFAULT     (0x00u)
#define APP_BATMAN_DM_CHG_FET_PROTECTIONS_A_DEFAULT     (0x98u)
#define APP_BATMAN_DM_CHG_FET_PROTECTIONS_B_DEFAULT     (0xD5u)
#define APP_BATMAN_DM_CHG_FET_PROTECTIONS_C_DEFAULT     (0x56u)
#define APP_BATMAN_DM_DSG_FET_PROTECTIONS_A_DEFAULT     (0xE4u)
#define APP_BATMAN_DM_DSG_FET_PROTECTIONS_B_DEFAULT     (0xE6u)
#define APP_BATMAN_DM_DSG_FET_PROTECTIONS_C_DEFAULT     (0xE2u)
#define APP_BATMAN_DM_DEFAULT_ALARM_MASK_DEFAULT        (0xF800u)
#define APP_BATMAN_DM_CC_GAIN_5_MOHM_IEEE754            (0x3FBF67F5u) /* 1.49536f = 7.4768 / 5mΩ。 */
#define APP_BATMAN_DM_CAPACITY_GAIN_5_MOHM_IEEE754      (0x48D9C710u) /* 446008.49f = CC Gain * 298261.6178。 */
#define APP_BATMAN_DM_CUV_THRESHOLD_2V83                (56u)   /* 56*50.6mV≈2.83V，低于 APP 3.0V 实测硬底线。 */
#define APP_BATMAN_DM_CUV_DELAY_1S                      (300u)  /* 约 6.6ms + 300*3.3ms = 996.6ms。 */
#define APP_BATMAN_DM_CUV_RECOVERY_HYS_200MV            (4u)    /* 4*50.6mV≈202mV，避免低压打嗝。 */
#define APP_BATMAN_DM_OCC_THRESHOLD_6A                  (15u)   /* 15*2mV/5mΩ=6A，作为 SC8815 充电限流后备。 */
#define APP_BATMAN_DM_OCC_DELAY_426MS                   (127u)  /* U1 最大档附近，约 6.6ms + 127*3.3ms = 425.7ms。 */
#define APP_BATMAN_DM_OCD1_THRESHOLD_14A                (35u)   /* 35*2mV/5mΩ=14A，位于 APP 12A 和 SCD 16A 之间。 */
#define APP_BATMAN_DM_OCD1_DELAY_300MS                  (89u)   /* 约 6.6ms + 89*3.3ms = 300.3ms。 */
#define APP_BATMAN_DM_OCD2_THRESHOLD_15A2               (38u)   /* 38*2mV/5mΩ=15.2A，SCD 前的快速过流层。 */
#define APP_BATMAN_DM_OCD2_DELAY_80MS                   (22u)   /* 约 79.2ms。 */
#define APP_BATMAN_DM_SCD_THRESHOLD_80MV                (0x04u) /* 80mV/5mΩ≈16A，给12A放电测试留余量。 */
#define APP_BATMAN_DM_SCD_DELAY_400US                   (0x1Cu) /* 近似 400us，实际约 405us。 */
#define APP_BATMAN_DM_OCC_RECOVERY_NEG_200MA            ((uint16_t)0xFF38u) /* I2 -200mA，回到轻微放电/零电流再恢复。 */
#define APP_BATMAN_DM_OCD_RECOVERY_500MA                (500u)  /* I2 500mA，负载明显降下来后再恢复。 */
#define APP_BATMAN_DM_OTC_THRESHOLD_50C                 (50u)   /* APP 45C 先停充，BQ 50C 作为硬后备。 */
#define APP_BATMAN_DM_OTC_DELAY_3S                      (3u)
#define APP_BATMAN_DM_OTC_RECOVERY_45C                  (45u)
#define APP_BATMAN_DM_OTD_THRESHOLD_60C                 (60u)   /* 与放电温度上限一致，BQ 直接兜底关 DSG。 */
#define APP_BATMAN_DM_OTD_DELAY_3S                      (3u)
#define APP_BATMAN_DM_OTD_RECOVERY_55C                  (55u)
#define APP_BATMAN_DM_FET_OPTIONS_DEFAULT               (BQ76952_FET_OPTIONS_FET_INIT_OFF_MASK | \
                                                         BQ76952_FET_OPTIONS_PDSG_EN_MASK | \
                                                         BQ76952_FET_OPTIONS_FET_CTRL_EN_MASK | \
                                                         BQ76952_FET_OPTIONS_HOST_FET_EN_MASK | \
                                                         BQ76952_FET_OPTIONS_SFET_MASK)
#define APP_BATMAN_DM_CHG_PUMP_CONTROL_DEFAULT          (0x01u)
#define APP_BATMAN_DM_BALANCING_CONFIGURATION_DEFAULT   (0x00u)
#define APP_BATMAN_MAIN_FET_OFF_MASK                    (BQ76952_FET_CONTROL_PCHG_OFF_MASK | \
                                                         BQ76952_FET_CONTROL_CHG_OFF_MASK | \
                                                         BQ76952_FET_CONTROL_PDSG_OFF_MASK | \
                                                         BQ76952_FET_CONTROL_DSG_OFF_MASK)

static Int_BQ76952_StatusTypeDef App_BatMan_WriteMainFetControl(uint8_t off_mask)
{
    uint8_t data[2];
    uint16_t mfg_status;
    Int_BQ76952_StatusTypeDef ret;

    ret = Int_BQ76952_ReadSubcommand(BQ76952_SUBCMD_MANUFACTURING_STATUS, data, 2u);
    if (ret != INT_BQ76952_OK)
    {
        return ret;
    }

    mfg_status = App_BatMan_ReadU16Le(data);
    if ((mfg_status & BQ76952_MFG_STATUS_FET_EN_MASK) == 0u)
    {
        ret = Int_BQ76952_SendSubcommand(BQ76952_SUBCMD_FET_ENABLE);
        if (ret != INT_BQ76952_OK)
        {
            return ret;
        }
    }

    data[0] = off_mask;
    ret = Int_BQ76952_WriteSubcommandData(BQ76952_SUBCMD_FET_CONTROL, data, 1u);
    if (ret == INT_BQ76952_OK)
    {
        fet_control_request = off_mask;
        if (off_mask != (uint8_t)APP_BATMAN_MAIN_FET_OFF_MASK)
        {
            /*
             * FET_CONTROL 只清 host off bit；FET_INIT_OFF/host latch 仍可能压住输出。
             * 释放任一主功率 MOS 时补发 ALL_FETS_ON，让 BQ 在无保护条件下真正打开通道。
             */
            ret = Int_BQ76952_SendSubcommand(BQ76952_SUBCMD_ALL_FETS_ON);
        }
    }
    return ret;
}

/**
 * @brief 写入 1 字节 Data Memory 配置。
 *
 * APP 层只表达业务配置项，I2C、Data Memory 校验和及时序细节由 INT 层负责。
 */
static bool App_BatMan_WriteConfigU8(uint16_t address, uint8_t value)
{
    if (Int_BQ76952_WriteDataMemory(address, &value, 1u) == INT_BQ76952_OK)
    {
        return true;
    }

    s_comm_fault = true;
    App_BatMan_PrintDmWrite8Fail(address);
    return false;
}

/**
 * @brief 写入 2 字节 Data Memory 配置。
 */
static bool App_BatMan_WriteConfigU16(uint16_t address, uint16_t value)
{
    uint8_t data[2];

    App_BatMan_WriteU16Le(value, data);
    if (Int_BQ76952_WriteDataMemory(address, data, 2u) == INT_BQ76952_OK)
    {
        return true;
    }

    s_comm_fault = true;
    App_BatMan_PrintDmWrite16Fail(address);
    return false;
}

/**
 * @brief 写入 4 字节 Data Memory 配置。
 */
static bool App_BatMan_WriteConfigU32(uint16_t address, uint32_t value)
{
    uint8_t data[4];

    App_BatMan_WriteU32Le(value, data);
    if (Int_BQ76952_WriteDataMemory(address, data, 4u) == INT_BQ76952_OK)
    {
        return true;
    }

    s_comm_fault = true;
    App_BatMan_PrintDmWrite32Fail(address);
    return false;
}

/**
 * @brief 写入 BQ76952 bring-up 阶段的项目基线配置。
 *
 * 与旧 APP 的关键区别：这里配置 BQ 的保护和均衡能力，但主充放电
 * MOS 默认保持关断，后续必须由明确的业务入口释放。函数保持短而直，
 * 是为了让每个 Data Memory 写入在审阅时都能被快速定位。
 */
bool App_BatMan_ConfigBq(void)
{
    /*
     * 真实 6S 采样并不是连续接在 BQ Cell1..Cell6：
     * 物理 cell0..5 对应 BQ Cell1/2/6/9/12/16。
     */
    if (!App_BatMan_WriteConfigU8(BQ76952_DM_DA_CONFIGURATION,
                                  APP_BATMAN_DM_DA_CONFIGURATION_DEFAULT) ||
        !App_BatMan_WriteConfigU16(BQ76952_DM_VCELL_MODE,
                                   BQ76952_CELL_MASK_6S_HW_CONFIRMED))
    {
        return false;
    }

    /*
     * 实板低边采样电阻为 5mΩ。把 BQ 自身的 CC Gain/Capacity Gain
     * 配准到硬件值后，APP 层电流读数保持 1:1，避免 SOC 和日志使用临时比例。
     */
    if (!App_BatMan_WriteConfigU32(BQ76952_DM_CC_GAIN,
                                   APP_BATMAN_DM_CC_GAIN_5_MOHM_IEEE754) ||
        !App_BatMan_WriteConfigU32(BQ76952_DM_CAPACITY_GAIN,
                                   APP_BATMAN_DM_CAPACITY_GAIN_5_MOHM_IEEE754))
    {
        return false;
    }

    /*
     * 保护配置与告警掩码决定 BQ 上报哪些事件，以及哪些事件进入
     * BQ 自己的保护/FET 判定路径。
     */
    if (!App_BatMan_WriteConfigU16(BQ76952_DM_PROTECTION_CONFIGURATION,
                                   APP_BATMAN_DM_PROTECTION_CONFIGURATION_DEFAULT) ||
        !App_BatMan_WriteConfigU16(BQ76952_DM_DEFAULT_ALARM_MASK,
                                   APP_BATMAN_DM_DEFAULT_ALARM_MASK_DEFAULT) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_CUV_THRESHOLD,
                                  APP_BATMAN_DM_CUV_THRESHOLD_2V83) ||
        !App_BatMan_WriteConfigU16(BQ76952_DM_CUV_DELAY,
                                   APP_BATMAN_DM_CUV_DELAY_1S) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_CUV_RECOVERY_HYSTERESIS,
                                  APP_BATMAN_DM_CUV_RECOVERY_HYS_200MV) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_OCC_THRESHOLD,
                                  APP_BATMAN_DM_OCC_THRESHOLD_6A) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_OCC_DELAY,
                                  APP_BATMAN_DM_OCC_DELAY_426MS) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_OCD1_THRESHOLD,
                                  APP_BATMAN_DM_OCD1_THRESHOLD_14A) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_OCD1_DELAY,
                                  APP_BATMAN_DM_OCD1_DELAY_300MS) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_OCD2_THRESHOLD,
                                  APP_BATMAN_DM_OCD2_THRESHOLD_15A2) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_OCD2_DELAY,
                                  APP_BATMAN_DM_OCD2_DELAY_80MS) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_SCD_THRESHOLD,
                                  APP_BATMAN_DM_SCD_THRESHOLD_80MV) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_SCD_DELAY,
                                  APP_BATMAN_DM_SCD_DELAY_400US) ||
        !App_BatMan_WriteConfigU16(BQ76952_DM_OCC_RECOVERY_THRESHOLD,
                                   APP_BATMAN_DM_OCC_RECOVERY_NEG_200MA) ||
        !App_BatMan_WriteConfigU16(BQ76952_DM_OCD_RECOVERY_THRESHOLD,
                                   APP_BATMAN_DM_OCD_RECOVERY_500MA) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_OTC_THRESHOLD,
                                  APP_BATMAN_DM_OTC_THRESHOLD_50C) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_OTC_DELAY,
                                  APP_BATMAN_DM_OTC_DELAY_3S) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_OTC_RECOVERY,
                                  APP_BATMAN_DM_OTC_RECOVERY_45C) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_OTD_THRESHOLD,
                                  APP_BATMAN_DM_OTD_THRESHOLD_60C) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_OTD_DELAY,
                                  APP_BATMAN_DM_OTD_DELAY_3S) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_OTD_RECOVERY,
                                  APP_BATMAN_DM_OTD_RECOVERY_55C))
    {
        return false;
    }

    /*
     * FET_INIT_OFF + HOST_FET_EN 让主充放电 MOS 上电后保持关断，
     * 直到 MCU 明确写 host FET control 释放。当前 bring-up 阶段只关不断开，
     * 不自动释放 CHG/DSG。
     */
    if (!App_BatMan_WriteConfigU8(BQ76952_DM_FET_OPTIONS,
                                  APP_BATMAN_DM_FET_OPTIONS_DEFAULT) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_CHG_PUMP_CONTROL,
                                  APP_BATMAN_DM_CHG_PUMP_CONTROL_DEFAULT))
    {
        return false;
    }

    /*
     * 关闭 BQ 自主均衡，允许 MCU 后续用 CB_ACTIVE_CELLS 明确指定均衡串。
     */
    if (!App_BatMan_WriteConfigU8(BQ76952_DM_BALANCING_CONFIGURATION,
                                  APP_BATMAN_DM_BALANCING_CONFIGURATION_DEFAULT))
    {
        return false;
    }

    /*
     * FET protection routing 决定哪些安全事件会关断 CHG/DSG。
     * 这些值和 FET_OPTIONS 放在同一段，便于上板前整体审查 FET 托管。
     */
    if (!App_BatMan_WriteConfigU8(BQ76952_DM_ENABLED_PROTECTIONS_A,
                                  APP_BATMAN_DM_ENABLED_PROTECTIONS_A_DEFAULT) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_ENABLED_PROTECTIONS_B,
                                  APP_BATMAN_DM_ENABLED_PROTECTIONS_B_DEFAULT) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_ENABLED_PROTECTIONS_C,
                                  APP_BATMAN_DM_ENABLED_PROTECTIONS_C_DEFAULT) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_CHG_FET_PROTECTIONS_A,
                                  APP_BATMAN_DM_CHG_FET_PROTECTIONS_A_DEFAULT) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_CHG_FET_PROTECTIONS_B,
                                  APP_BATMAN_DM_CHG_FET_PROTECTIONS_B_DEFAULT) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_CHG_FET_PROTECTIONS_C,
                                  APP_BATMAN_DM_CHG_FET_PROTECTIONS_C_DEFAULT) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_DSG_FET_PROTECTIONS_A,
                                  APP_BATMAN_DM_DSG_FET_PROTECTIONS_A_DEFAULT) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_DSG_FET_PROTECTIONS_B,
                                  APP_BATMAN_DM_DSG_FET_PROTECTIONS_B_DEFAULT) ||
        !App_BatMan_WriteConfigU8(BQ76952_DM_DSG_FET_PROTECTIONS_C,
                                  APP_BATMAN_DM_DSG_FET_PROTECTIONS_C_DEFAULT))
    {
        return false;
    }

    return true;
}

/**
 * @brief 让 BQ 进入 FET 固件控制态，并在初始化阶段保持主充放电 MOS 关断。
 *
 * 上电配置阶段先确保 FET_EN 为 firmware control，再写 FET_CONTROL off mask，
 * 明确关断 CHG/DSG/PCHG/PDSG；运行阶段由 App_Power 通过 App_BatMan_SetMainFets()
 * 按保护条件释放主 MOS。
 */
Int_BQ76952_StatusTypeDef App_BatMan_KeepMainFetsOff(void)
{
    return App_BatMan_WriteMainFetControl((uint8_t)APP_BATMAN_MAIN_FET_OFF_MASK);
}

bool App_BatMan_SetMainFets(bool charge_enable, bool discharge_enable)
{
    uint8_t off_mask = 0u;

    if (!charge_enable)
    {
        off_mask |= BQ76952_FET_CONTROL_PCHG_OFF_MASK;
        off_mask |= BQ76952_FET_CONTROL_CHG_OFF_MASK;
    }
    if (!discharge_enable)
    {
        off_mask |= BQ76952_FET_CONTROL_PDSG_OFF_MASK;
        off_mask |= BQ76952_FET_CONTROL_DSG_OFF_MASK;
    }

    if (App_BatMan_WriteMainFetControl(off_mask) == INT_BQ76952_OK)
    {
        return true;
    }

    s_comm_fault = true;
    return false;
}

bool App_BatMan_TestPreDischargeOnly(void)
{
    const uint8_t off_mask = (uint8_t)(BQ76952_FET_CONTROL_PCHG_OFF_MASK |
                                       BQ76952_FET_CONTROL_CHG_OFF_MASK |
                                       BQ76952_FET_CONTROL_DSG_OFF_MASK);

    /*
     * 串口定位专用：只清 PDSG_OFF，保持 DSG_OFF，确认 BQ 和硬件是否
     * 真的能单独拉起预放电通道。FET_CONTROL 只表达关断掩码，随后还需
     * ALL_FETS_ON 释放 FET_INIT_OFF/host off latch，否则 PDSG 可能保持关闭。
     */
    if ((App_BatMan_WriteMainFetControl(off_mask) == INT_BQ76952_OK) &&
        (Int_BQ76952_SendSubcommand(BQ76952_SUBCMD_ALL_FETS_ON) == INT_BQ76952_OK))
    {
        return true;
    }

    s_comm_fault = true;
    return false;
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

/**
 * @brief 清除启动阶段预期出现的告警位。
 *
 * INIT/SCAN/WAKE 类告警在启动中很常见，只清这些位可以降低串口噪声，
 * 同时避免掩盖真正的 safety status。
 */
void App_BatMan_ClearStartupAlarms(void)
{
    uint8_t data[2];
    const uint16_t mask = BQ76952_ALARM_INITSTART_MASK |
                          BQ76952_ALARM_INITCOMP_MASK |
                          BQ76952_ALARM_FULLSCAN_MASK |
                          BQ76952_ALARM_ADSCAN_MASK |
                          BQ76952_ALARM_WAKE_MASK;

    App_BatMan_WriteU16Le(mask, data);
    (void)Int_BQ76952_WriteDirect(BQ76952_CMD_ALARM_STATUS, data, 2u);
}
