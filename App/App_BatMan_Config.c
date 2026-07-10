#include "App_BatMan_Internal.h"

#include <stdio.h>

#include "Int_BQ76952.h"
#include "Int_BQ76952_BSP.h"

/*
 * 项目 Data Memory 基线值。
 * 这里只保留已经和硬件/bring-up 目标绑定的最小集合：6S 映射、主 FET
 * 默认关断、告警掩码、保护路由和均衡模式。最终阈值必须来自实验标定，
 * 不在 APP 层凭经验补齐。
 */
enum
{
    APP_BATMAN_DM_DA_CONFIGURATION_DEFAULT = 0x05u,
    APP_BATMAN_DM_PROTECTION_CONFIGURATION_DEFAULT = 0x0002u,
    APP_BATMAN_DM_ENABLED_PROTECTIONS_A_DEFAULT = BQ76952_ENABLED_PROTECTIONS_A_SCD_MASK |
                                                   BQ76952_ENABLED_PROTECTIONS_A_OCD2_MASK |
                                                   BQ76952_ENABLED_PROTECTIONS_A_OCD1_MASK |
                                                   BQ76952_ENABLED_PROTECTIONS_A_OCC_MASK |
                                                   BQ76952_ENABLED_PROTECTIONS_A_CUV_MASK |
                                                   BQ76952_ENABLED_PROTECTIONS_A_COV_MASK,
    APP_BATMAN_DM_ENABLED_PROTECTIONS_B_DEFAULT = BQ76952_ENABLED_PROTECTIONS_B_OTD_MASK |
                                                   BQ76952_ENABLED_PROTECTIONS_B_OTC_MASK,
    APP_BATMAN_DM_ENABLED_PROTECTIONS_C_DEFAULT = 0x00u,
    APP_BATMAN_DM_CHG_FET_PROTECTIONS_A_DEFAULT = 0x98u,
    APP_BATMAN_DM_CHG_FET_PROTECTIONS_B_DEFAULT = 0xD5u,
    APP_BATMAN_DM_CHG_FET_PROTECTIONS_C_DEFAULT = 0x56u,
    APP_BATMAN_DM_DSG_FET_PROTECTIONS_A_DEFAULT = 0xE4u,
    APP_BATMAN_DM_DSG_FET_PROTECTIONS_B_DEFAULT = 0xE6u,
    APP_BATMAN_DM_DSG_FET_PROTECTIONS_C_DEFAULT = 0xE2u,
    APP_BATMAN_DM_DEFAULT_ALARM_MASK_DEFAULT = 0xF800u,

    /* 5mΩ 实板采样电阻对应的 IEEE-754 小端写入值。 */
    APP_BATMAN_DM_CC_GAIN_5_MOHM_IEEE754 = 0x3FBF67F5u,
    APP_BATMAN_DM_CAPACITY_GAIN_5_MOHM_IEEE754 = 0x48D9C710u,

    APP_BATMAN_DM_CUV_THRESHOLD_2V83 = 56u,             /* 约 2.83V。 */
    APP_BATMAN_DM_CUV_DELAY_1S = 300u,                  /* 约 996.6ms。 */
    APP_BATMAN_DM_CUV_RECOVERY_HYS_200MV = 4u,          /* 约 202mV。 */
    APP_BATMAN_DM_OCC_THRESHOLD_6A = 15u,
    APP_BATMAN_DM_OCC_DELAY_426MS = 127u,
    APP_BATMAN_DM_OCD1_THRESHOLD_14A = 35u,
    APP_BATMAN_DM_OCD1_DELAY_300MS = 89u,
    APP_BATMAN_DM_OCD2_THRESHOLD_15A2 = 38u,
    APP_BATMAN_DM_OCD2_DELAY_80MS = 22u,
    APP_BATMAN_DM_SCD_THRESHOLD_80MV = 0x04u,            /* 5mΩ 下约 16A。 */
    APP_BATMAN_DM_SCD_DELAY_400US = 0x1Cu,
    APP_BATMAN_DM_OCC_RECOVERY_NEG_200MA = 0xFF38u,
    APP_BATMAN_DM_OCD_RECOVERY_500MA = 500u,
    APP_BATMAN_DM_OTC_THRESHOLD_50C = 50u,
    APP_BATMAN_DM_OTC_DELAY_3S = 3u,
    APP_BATMAN_DM_OTC_RECOVERY_45C = 45u,
    APP_BATMAN_DM_OTD_THRESHOLD_60C = 60u,
    APP_BATMAN_DM_OTD_DELAY_3S = 3u,
    APP_BATMAN_DM_OTD_RECOVERY_55C = 55u,

    APP_BATMAN_DM_FET_OPTIONS_DEFAULT = BQ76952_FET_OPTIONS_FET_INIT_OFF_MASK |
                                         BQ76952_FET_OPTIONS_PDSG_EN_MASK |
                                         BQ76952_FET_OPTIONS_FET_CTRL_EN_MASK |
                                         BQ76952_FET_OPTIONS_HOST_FET_EN_MASK |
                                         BQ76952_FET_OPTIONS_SFET_MASK,
    APP_BATMAN_DM_CHG_PUMP_CONTROL_DEFAULT = 0x01u,
    APP_BATMAN_DM_BALANCING_CONFIGURATION_DEFAULT = 0x00u,
    APP_BATMAN_MAIN_FET_OFF_MASK = BQ76952_FET_CONTROL_PCHG_OFF_MASK |
                                    BQ76952_FET_CONTROL_CHG_OFF_MASK |
                                    BQ76952_FET_CONTROL_PDSG_OFF_MASK |
                                    BQ76952_FET_CONTROL_DSG_OFF_MASK
};

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
    printf("BQ配置写8位失败 地址:0x%04x\r\n", (unsigned int)address);
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
    printf("BQ配置写16位失败 地址:0x%04x\r\n", (unsigned int)address);
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
    printf("BQ配置写32位失败 地址:0x%04x\r\n", (unsigned int)address);
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

bool App_BatMan_RecoverAfterWake(void)
{
    uint8_t data[2];
    uint8_t fet;
    uint16_t raw_alarm;
    uint16_t status;
    Int_BQ76952_StatusTypeDef ret;

    /*
     * BQ shutdown 唤醒后可能带 POR/SLEEP_EN/FET_INIT_OFF 等上电残留。
     * 先恢复项目 Data Memory 基线，再释放主 FET，避免 XCHG 在无保护异常时一直压住 CHG。
     */
    if (Int_BQ76952_EnterConfigUpdate() != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        return false;
    }

    if (!App_BatMan_ConfigBq())
    {
        (void)Int_BQ76952_ExitConfigUpdate();
        s_comm_fault = true;
        return false;
    }

    if (Int_BQ76952_ExitConfigUpdate() != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        return false;
    }

    ret = Int_BQ76952_SendSubcommand(BQ76952_SUBCMD_SLEEP_DISABLE);
    if (ret != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        return false;
    }

    App_BatMan_ClearStartupAlarms();
    if (!App_BatMan_SetMainFets(true, true))
    {
        return false;
    }

    if (Int_BQ76952_ReadDirect(BQ76952_CMD_ALARM_RAW_STATUS, data, 2u) != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        return false;
    }

    raw_alarm = App_BatMan_ReadU16Le(data);
    if (Int_BQ76952_ReadDirect(BQ76952_CMD_BATTERY_STATUS, data, 2u) != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        return false;
    }
    status = App_BatMan_ReadU16Le(data);
    if (Int_BQ76952_ReadDirect(BQ76952_CMD_FET_STATUS, &fet, 1u) != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        return false;
    }

    if (((status & BQ76952_BATTERY_STATUS_SDM_MASK) != 0u) ||
        ((raw_alarm & (BQ76952_ALARM_XCHG_MASK | BQ76952_ALARM_XDSG_MASK)) != 0u) ||
        ((fet & (BQ76952_FET_STATUS_CHG_FET_MASK | BQ76952_FET_STATUS_DSG_FET_MASK)) !=
         (BQ76952_FET_STATUS_CHG_FET_MASK | BQ76952_FET_STATUS_DSG_FET_MASK)))
    {
        return false;
    }

    return true;
}

bool App_BatMan_RequestShutdown(void)
{
    /*
     * SHUTDOWN 是危险子命令。进入前先关主 FET，避免 BQ 掉电过程中
     * 充放电 MOS 仍保持在不明确状态。
     */
    if (App_BatMan_KeepMainFetsOff() != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        return false;
    }

    if (Int_BQ76952_Shutdown() != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        return false;
    }

    return true;
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
