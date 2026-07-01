#include "App_BatMan_Internal.h"

#include "Com_BQ76952.h"
#include "Int_BQ76952.h"
#include "Int_BQ76952_BSP.h"

static bool s_temp_ts1_sample_valid = false;
static bool s_temp_ts3_sample_valid = false;

void App_BatMan_ResetSampleState(void)
{
    s_cells_sample_valid = false;
    s_current_sample_valid = false;
    s_temp_ts1_sample_valid = false;
    s_temp_ts3_sample_valid = false;
    s_temp_cell_sample_valid = false;
}

/**
 * @brief 读取 BQ direct command 的 1 字节数据。
 *
 * 单次失败只标记当前采样周期通信异常，下一次任务周期会重新尝试。
 */
static bool App_BatMan_ReadDirectU8(uint8_t command, uint8_t *value)
{
    if (Int_BQ76952_ReadDirect(command, value, 1u) == INT_BQ76952_OK)
    {
        return true;
    }

    s_comm_fault = true;
    return false;
}

/**
 * @brief 读取 BQ direct command 的 2 字节 little-endian 数据。
 */
static bool App_BatMan_ReadDirectU16(uint8_t command, uint16_t *value)
{
    uint8_t data[2];

    if (Int_BQ76952_ReadDirect(command, data, 2u) == INT_BQ76952_OK)
    {
        *value = App_BatMan_ReadU16Le(data);
        return true;
    }

    s_comm_fault = true;
    return false;
}

/**
 * @brief 判断温度值是否适合作为 APP 层温度输入。
 *
 * TS 通道未接或未正确配置时可能读到极端值。这里过滤掉明显异常值，
 * 后续用 IC 温度兜底，避免 SOC/SOH 或日志出现 -273C 一类假数据。
 */
static bool App_BatMan_IsTempValid(int16_t temp_c)
{
    return (temp_c >= -40) && (temp_c <= 100);
}

/**
 * @brief 读取 6 串实际电芯电压并计算 min/max/avg/delta。
 *
 * 命令表体现了本硬件的稀疏采样映射；维护时不要按连续 Cell1..Cell6
 * 直觉改写。
 */
static void App_BatMan_LoadCellsVoltage(void)
{
    static const uint8_t commands[APP_BATMAN_CELL_COUNT] =
    {
        BQ76952_CMD_CELL1_VOLTAGE,
        BQ76952_CMD_CELL2_VOLTAGE,
        BQ76952_CMD_CELL6_VOLTAGE,
        BQ76952_CMD_CELL9_VOLTAGE,
        BQ76952_CMD_CELL12_VOLTAGE,
        BQ76952_CMD_CELL16_VOLTAGE
    };
    uint8_t i;
    uint16_t mv;
    uint32_t sum_mv = 0u;
    uint8_t valid_count = 0u;

    s_cells_sample_valid = false;
    cell_min_mv = 0xFFFFu;
    cell_max_mv = 0u;

    for (i = 0u; i < APP_BATMAN_CELL_COUNT; i++)
    {
        if (!App_BatMan_ReadDirectU16(commands[i], &mv))
        {
            cell_mv[i] = 0u;
            continue;
        }

        cell_mv[i] = mv;
        sum_mv += mv;
        valid_count++;
        cell_min_mv = (mv < cell_min_mv) ? mv : cell_min_mv;
        cell_max_mv = (mv > cell_max_mv) ? mv : cell_max_mv;
    }

    if (cell_min_mv == 0xFFFFu)
    {
        cell_min_mv = 0u;
    }

    if (valid_count == APP_BATMAN_CELL_COUNT)
    {
        cell_avg_mv = (uint16_t)(sum_mv / valid_count);
        s_cells_sample_valid = true;
    }
    else
    {
        cell_avg_mv = 0u;
    }
    cell_delta_mv = (uint16_t)(cell_max_mv - cell_min_mv);
}

/**
 * @brief 读取电池总压快照。
 *
 * 当前 `pack_mv` 暂时镜像 `stack_mv`。若后续需要 PACK/LD pin 电压，
 * 应新增独立字段，避免复用 `stack_mv` 造成语义混淆。
 */
static void App_BatMan_LoadBatVoltage(void)
{
    uint16_t raw;

    if (App_BatMan_ReadDirectU16(BQ76952_CMD_STACK_VOLTAGE, &raw))
    {
        stack_mv = raw;
        pack_mv = stack_mv;
    }
}

/**
 * @brief 读取 CC2 电流并转换为 APP 约定方向。
 *
 * 当前约定：`current_ma > 0` 表示充电，`current_ma < 0` 表示放电。
 * 若实测方向相反，只改 `APP_BATMAN_CC2_RAW_POLARITY`。
 */
static void App_BatMan_LoadCurrent(void)
{
    uint16_t raw_u16;

    s_current_sample_valid = false;
    if (App_BatMan_ReadDirectU16(BQ76952_CMD_CC2_CURRENT, &raw_u16))
    {
        current_ma = (int32_t)((int16_t)raw_u16) * APP_BATMAN_CC2_RAW_POLARITY;
        current_a = (float)current_ma / 1000.0f;
        s_current_sample_valid = true;
    }
}

/**
 * @brief 读取 IC/TS 温度并生成 APP 层温度快照。
 *
 * TS1/TS3 有效时用于估算 cell temperature；无效时降级为 IC 温度，
 * 这样 SOH 和日志仍保持可读。
 */
static void App_BatMan_LoadTemperature(void)
{
    uint16_t raw;
    int16_t temp;

    s_temp_ts1_sample_valid = false;
    s_temp_ts3_sample_valid = false;
    s_temp_cell_sample_valid = false;

    if (App_BatMan_ReadDirectU16(BQ76952_CMD_INT_TEMPERATURE, &raw))
    {
        temp_ic_c = Com_BQ76952_Temp0p1KToC((int16_t)raw);
    }
    if (App_BatMan_ReadDirectU16(BQ76952_CMD_TS1_TEMPERATURE, &raw))
    {
        temp = Com_BQ76952_Temp0p1KToC((int16_t)raw);
        if (App_BatMan_IsTempValid(temp))
        {
            temp_ts1_c = temp;
            s_temp_ts1_sample_valid = true;
        }
        else
        {
            temp_ts1_c = temp_ic_c;
        }
    }
    if (App_BatMan_ReadDirectU16(BQ76952_CMD_TS3_TEMPERATURE, &raw))
    {
        temp = Com_BQ76952_Temp0p1KToC((int16_t)raw);
        if (App_BatMan_IsTempValid(temp))
        {
            temp_ts3_c = temp;
            s_temp_ts3_sample_valid = true;
        }
        else
        {
            temp_ts3_c = temp_ic_c;
        }
    }

    if (s_temp_ts1_sample_valid && s_temp_ts3_sample_valid)
    {
        temp_cell_c = (int16_t)((temp_ts1_c + temp_ts3_c) / 2);
        s_temp_cell_sample_valid = true;
    }
    else if (s_temp_ts1_sample_valid)
    {
        temp_cell_c = temp_ts1_c;
        s_temp_cell_sample_valid = true;
    }
    else if (s_temp_ts3_sample_valid)
    {
        temp_cell_c = temp_ts3_c;
        s_temp_cell_sample_valid = true;
    }
    else
    {
        temp_cell_c = temp_ic_c;
    }
    temp_fet_c = temp_ic_c;
}

/**
 * @brief 读取 BQ 告警、FET 和 safety 状态。
 *
 * 这些字段用于解释 BQ 为什么打开或关闭 MOS。APP 只记录状态，
 * 不覆盖 BQ 的保护决策。
 */
static void App_BatMan_LoadBqStatus(void)
{
    uint8_t data[2];
    uint16_t value16;
    uint8_t value8;

    if (App_BatMan_ReadDirectU16(BQ76952_CMD_ALARM_STATUS, &value16))
    {
        alarm_status = value16;
    }
    if (App_BatMan_ReadDirectU16(BQ76952_CMD_ALARM_RAW_STATUS, &value16))
    {
        alarm_raw = value16;
    }
    if (App_BatMan_ReadDirectU16(BQ76952_CMD_BATTERY_STATUS, &value16))
    {
        battery_status = value16;
    }
    if (Int_BQ76952_ReadSubcommand(BQ76952_SUBCMD_MANUFACTURING_STATUS, data, 2u) == INT_BQ76952_OK)
    {
        manufacturing_status = App_BatMan_ReadU16Le(data);
    }
    if (App_BatMan_ReadDirectU8(BQ76952_CMD_FET_STATUS, &value8))
    {
        fet_status = value8;
    }
    if (App_BatMan_ReadDirectU8(BQ76952_CMD_SAFETY_ALERT_A, &value8))
    {
        safety_alert_a = value8;
    }
    if (App_BatMan_ReadDirectU8(BQ76952_CMD_SAFETY_ALERT_B, &value8))
    {
        safety_alert_b = value8;
    }
    if (App_BatMan_ReadDirectU8(BQ76952_CMD_SAFETY_ALERT_C, &value8))
    {
        safety_alert_c = value8;
    }
    if (App_BatMan_ReadDirectU8(BQ76952_CMD_SAFETY_STATUS_A, &value8))
    {
        safety_status_a = value8;
    }
    if (App_BatMan_ReadDirectU8(BQ76952_CMD_SAFETY_STATUS_B, &value8))
    {
        safety_status_b = value8;
    }
    if (App_BatMan_ReadDirectU8(BQ76952_CMD_SAFETY_STATUS_C, &value8))
    {
        safety_status_c = value8;
    }
    if (App_BatMan_ReadDirectU8(BQ76952_CMD_PF_STATUS_A, &value8))
    {
        pf_status_a = value8;
    }
    if (App_BatMan_ReadDirectU8(BQ76952_CMD_PF_STATUS_B, &value8))
    {
        pf_status_b = value8;
    }
    if (App_BatMan_ReadDirectU8(BQ76952_CMD_PF_STATUS_C, &value8))
    {
        pf_status_c = value8;
    }
    if (App_BatMan_ReadDirectU8(BQ76952_CMD_PF_STATUS_D, &value8))
    {
        pf_status_d = value8;
    }
}

/**
 * @brief 更新 APP 层故障摘要。
 *
 * 这是显示/日志用的软件摘要，不是硬件保护动作。通信失败、明显异常
 * 电芯电压或 safety status 非零都会让 `fault_active` 置位。
 */
static void App_BatMan_UpdateFaultState(void)
{
    uint8_t i;
    bool cell_fault = false;
    bool safety_fault;

    for (i = 0u; i < APP_BATMAN_CELL_COUNT; i++)
    {
        if ((cell_mv[i] < APP_BATMAN_CELL_VALID_MIN_MV) ||
            (cell_mv[i] > APP_BATMAN_CELL_VALID_MAX_MV))
        {
            cell_fault = true;
        }
    }

    safety_fault = ((safety_status_a != 0u) ||
                    (safety_status_b != 0u) ||
                    (safety_status_c != 0u));
    fault_active = s_comm_fault || cell_fault || safety_fault;
}

/**
 * @brief 执行一次完整 BQ 采样。
 *
 * 固定采样顺序便于对比不同固件版本的串口日志。
 */
void App_BatMan_Sample(void)
{
    App_BatMan_LoadCellsVoltage();
    App_BatMan_LoadBatVoltage();
    App_BatMan_LoadCurrent();
    App_BatMan_LoadTemperature();
    App_BatMan_LoadBqStatus();
    App_BatMan_UpdateFaultState();
}
