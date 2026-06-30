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
#define APP_BATMAN_DM_ENABLED_PROTECTIONS_A_DEFAULT     (0x88u)
#define APP_BATMAN_DM_ENABLED_PROTECTIONS_B_DEFAULT     (0x00u)
#define APP_BATMAN_DM_ENABLED_PROTECTIONS_C_DEFAULT     (0x00u)
#define APP_BATMAN_DM_CHG_FET_PROTECTIONS_A_DEFAULT     (0x98u)
#define APP_BATMAN_DM_CHG_FET_PROTECTIONS_B_DEFAULT     (0xD5u)
#define APP_BATMAN_DM_CHG_FET_PROTECTIONS_C_DEFAULT     (0x56u)
#define APP_BATMAN_DM_DSG_FET_PROTECTIONS_A_DEFAULT     (0xE4u)
#define APP_BATMAN_DM_DSG_FET_PROTECTIONS_B_DEFAULT     (0xE6u)
#define APP_BATMAN_DM_DSG_FET_PROTECTIONS_C_DEFAULT     (0xE2u)
#define APP_BATMAN_DM_DEFAULT_ALARM_MASK_DEFAULT        (0xF800u)
#define APP_BATMAN_DM_FET_OPTIONS_DEFAULT               (BQ76952_FET_OPTIONS_FET_INIT_OFF_MASK | \
                                                         BQ76952_FET_OPTIONS_FET_CTRL_EN_MASK | \
                                                         BQ76952_FET_OPTIONS_HOST_FET_EN_MASK | \
                                                         BQ76952_FET_OPTIONS_SFET_MASK)
#define APP_BATMAN_DM_CHG_PUMP_CONTROL_DEFAULT          (0x01u)
#define APP_BATMAN_DM_BALANCING_CONFIGURATION_DEFAULT   (0x00u)
#define APP_BATMAN_MAIN_FET_OFF_MASK                    (BQ76952_FET_CONTROL_PCHG_OFF_MASK | \
                                                         BQ76952_FET_CONTROL_CHG_OFF_MASK | \
                                                         BQ76952_FET_CONTROL_PDSG_OFF_MASK | \
                                                         BQ76952_FET_CONTROL_DSG_OFF_MASK)

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
     * 保护配置与告警掩码决定 BQ 上报哪些事件，以及哪些事件进入
     * BQ 自己的保护/FET 判定路径。
     */
    if (!App_BatMan_WriteConfigU16(BQ76952_DM_PROTECTION_CONFIGURATION,
                                   APP_BATMAN_DM_PROTECTION_CONFIGURATION_DEFAULT) ||
        !App_BatMan_WriteConfigU16(BQ76952_DM_DEFAULT_ALARM_MASK,
                                   APP_BATMAN_DM_DEFAULT_ALARM_MASK_DEFAULT))
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
 * @brief 让 BQ 进入 FET 固件控制态，并保持主充放电 MOS 关断。
 *
 * 默认安全策略是“能通信、能采样、能均衡，但主功率路径不自动接通”。
 * 这里先确保 FET_EN 为 firmware control，再写 FET_CONTROL off mask，
 * 明确关断 CHG/DSG/PCHG/PDSG。后续若要放开主功率 MOS，应新增单独业务入口。
 */
Int_BQ76952_StatusTypeDef App_BatMan_KeepMainFetsOff(void)
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
    if ((mfg_status & BQ76952_MFG_STATUS_FET_EN_MASK) != 0u)
    {
        data[0] = (uint8_t)APP_BATMAN_MAIN_FET_OFF_MASK;
        return Int_BQ76952_WriteSubcommandData(BQ76952_SUBCMD_FET_CONTROL, data, 1u);
    }

    ret = Int_BQ76952_SendSubcommand(BQ76952_SUBCMD_FET_ENABLE);
    if (ret != INT_BQ76952_OK)
    {
        return ret;
    }

    data[0] = (uint8_t)APP_BATMAN_MAIN_FET_OFF_MASK;
    return Int_BQ76952_WriteSubcommandData(BQ76952_SUBCMD_FET_CONTROL, data, 1u);
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
