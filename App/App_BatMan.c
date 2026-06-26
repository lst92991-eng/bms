#include "App_BatMan.h"

#include <stdio.h>

#include "Com_BQ76952.h"
#include "Int_BQ76952.h"
#include "Int_BQ76952_BSP.h"

/*
 * 这一版 App_BatMan.c 的目标很简单：
 * 1. 结构尽量像旧项目，逻辑顺序一眼能读懂；
 * 2. APP 直接调 INT，不再经由厚 COM wrapper；
 * 3. BQ 相关业务全部放在这个文件里，便于教学和带新人。
 */

#define APP_BATMAN_CRC_BOOT_ENABLE          (1u)
#define APP_BATMAN_MAX_DEBUG_LINES          (6u)
#define APP_BATMAN_SOC_BLEND_ALPHA_CHG      (0.25f)
#define APP_BATMAN_SOC_BLEND_ALPHA_DSG      (0.12f)
#define APP_BATMAN_SOC_BLEND_ALPHA_IDLE     (0.18f)

/* BQ Data Memory 默认值只保留当前项目已经确认的最小集合。 */
#define APP_BATMAN_DM_DA_CONFIGURATION_DEFAULT              (0x05u)
#define APP_BATMAN_DM_PROTECTION_CONFIGURATION_DEFAULT      (0x0002u)
#define APP_BATMAN_DM_ENABLED_PROTECTIONS_A_DEFAULT         (0x88u)
#define APP_BATMAN_DM_ENABLED_PROTECTIONS_B_DEFAULT         (0x00u)
#define APP_BATMAN_DM_ENABLED_PROTECTIONS_C_DEFAULT         (0x00u)
#define APP_BATMAN_DM_CHG_FET_PROTECTIONS_A_DEFAULT         (0x98u)
#define APP_BATMAN_DM_CHG_FET_PROTECTIONS_B_DEFAULT         (0xD5u)
#define APP_BATMAN_DM_CHG_FET_PROTECTIONS_C_DEFAULT         (0x56u)
#define APP_BATMAN_DM_DSG_FET_PROTECTIONS_A_DEFAULT         (0xE4u)
#define APP_BATMAN_DM_DSG_FET_PROTECTIONS_B_DEFAULT         (0xE6u)
#define APP_BATMAN_DM_DSG_FET_PROTECTIONS_C_DEFAULT         (0xE2u)
#define APP_BATMAN_DM_DEFAULT_ALARM_MASK_DEFAULT            (0xF800u)
#define APP_BATMAN_DM_FET_OPTIONS_DEFAULT                   (0x0Du)
#define APP_BATMAN_DM_CHG_PUMP_CONTROL_DEFAULT              (0x01u)
#define APP_BATMAN_DM_BALANCING_CONFIGURATION_HOST_ONLY     (0x08u)
#define APP_BATMAN_DM_CB_MAX_CELLS_DEFAULT                  (1u)
#define APP_BATMAN_DM_CB_INTERVAL_DEFAULT_S                 (20u)
#define APP_BATMAN_DM_CB_MIN_CELL_TEMP_DEFAULT              (0u)
#define APP_BATMAN_DM_CB_MAX_CELL_TEMP_DEFAULT              (45u)
#define APP_BATMAN_DM_CB_MAX_INTERNAL_TEMP_DEFAULT          (70u)
#define APP_BATMAN_DM_CB_MIN_CELL_V_CHARGE_DEFAULT          (3900u)
#define APP_BATMAN_DM_CB_MIN_DELTA_CHARGE_DEFAULT           (40u)
#define APP_BATMAN_DM_CB_STOP_DELTA_CHARGE_DEFAULT          (20u)

uint16_t cell_mv[APP_BATMAN_CELL_COUNT];
uint32_t stack_mv;
uint32_t pack_mv;
int32_t current_ma;
float current_a;
uint16_t cell_min_mv;
uint16_t cell_max_mv;
uint16_t cell_avg_mv;
uint16_t cell_delta_mv;
int16_t temp_ic_c;
int16_t temp_ts1_c;
int16_t temp_ts3_c;
int16_t temp_cell_c;
int16_t temp_fet_c;
uint16_t alarm_status;
uint16_t alarm_raw;
uint16_t battery_status;
uint16_t balance_mask;
uint8_t fet_status;
uint8_t safety_status_a;
uint8_t safety_status_b;
uint8_t safety_status_c;
bool fault_active;
bool charge_allowed;
bool discharge_allowed;
float soc_percent;
float display_soc_percent;

static uint8_t s_fet_off_mask = 0u;
static bool s_charge_request = true;
static bool s_discharge_request = true;
static bool s_comm_fault = false;
static bool s_balance_active = false;
static bool s_soc_seeded = false;
static float s_coulomb_mah = (float)APP_BATMAN_CELL_CAP_TYP_MAH * 0.5f;
static uint16_t s_debug_ms = 0u;

static uint16_t App_BatMan_ReadU16Le(const uint8_t data[2])
{
    return (uint16_t)(((uint16_t)data[1] << 8u) | data[0]);
}

static uint16_t App_BatMan_PhysicalBalanceMaskToBqMask(const uint8_t cell_to_balance[APP_BATMAN_CELL_COUNT])
{
    static const uint16_t bq_balance_bit_map[APP_BATMAN_CELL_COUNT] =
    {
        0x0001u, /* physical cell 0 -> BQ Cell1  -> bit0  */
        0x0002u, /* physical cell 1 -> BQ Cell2  -> bit1  */
        0x0020u, /* physical cell 2 -> BQ Cell6  -> bit5  */
        0x0100u, /* physical cell 3 -> BQ Cell9  -> bit8  */
        0x0800u, /* physical cell 4 -> BQ Cell12 -> bit11 */
        0x8000u  /* physical cell 5 -> BQ Cell16 -> bit15 */
    };
    uint16_t bq_mask = 0u;
    uint8_t i;

    for (i = 0u; i < APP_BATMAN_CELL_COUNT; i++)
    {
        if (cell_to_balance[i] != 0u)
        {
            bq_mask |= bq_balance_bit_map[i];
        }
    }

    return bq_mask;
}

static Int_BQ76952_StatusTypeDef App_BatMan_SetBalanceMask(uint16_t bq_mask)
{
    uint8_t data[2];

    data[0] = (uint8_t)(bq_mask & 0xFFu);
    data[1] = (uint8_t)(bq_mask >> 8u);

    return Int_BQ76952_WriteSubcommandData(BQ76952_SUBCMD_CB_ACTIVE_CELLS, data, 2u);
}

static Int_BQ76952_StatusTypeDef App_BatMan_StopBalancing(void)
{
    return App_BatMan_SetBalanceMask(0u);
}

static void App_BatMan_PrintBringupConfig(void)
{
    printf("bq bringup config only, not final safety release\r\n");
    printf("cell_map:0x%04x phys_to_bq:0->1 1->2 2->6 3->9 4->12 5->16\r\n",
           (unsigned int)BQ76952_CELL_MASK_6S_HW_CONFIRMED);
    printf("dm da:0x%02x prot_cfg:0x%04x alarm_mask:0x%04x fet_opt:0x%02x pump:0x%02x\r\n",
           (unsigned int)APP_BATMAN_DM_DA_CONFIGURATION_DEFAULT,
           (unsigned int)APP_BATMAN_DM_PROTECTION_CONFIGURATION_DEFAULT,
           (unsigned int)APP_BATMAN_DM_DEFAULT_ALARM_MASK_DEFAULT,
           (unsigned int)APP_BATMAN_DM_FET_OPTIONS_DEFAULT,
           (unsigned int)APP_BATMAN_DM_CHG_PUMP_CONTROL_DEFAULT);
    printf("enabled_prot A:0x%02x B:0x%02x C:0x%02x\r\n",
           (unsigned int)APP_BATMAN_DM_ENABLED_PROTECTIONS_A_DEFAULT,
           (unsigned int)APP_BATMAN_DM_ENABLED_PROTECTIONS_B_DEFAULT,
           (unsigned int)APP_BATMAN_DM_ENABLED_PROTECTIONS_C_DEFAULT);
    printf("fet_prot chg:%02x/%02x/%02x dsg:%02x/%02x/%02x\r\n",
           (unsigned int)APP_BATMAN_DM_CHG_FET_PROTECTIONS_A_DEFAULT,
           (unsigned int)APP_BATMAN_DM_CHG_FET_PROTECTIONS_B_DEFAULT,
           (unsigned int)APP_BATMAN_DM_CHG_FET_PROTECTIONS_C_DEFAULT,
           (unsigned int)APP_BATMAN_DM_DSG_FET_PROTECTIONS_A_DEFAULT,
           (unsigned int)APP_BATMAN_DM_DSG_FET_PROTECTIONS_B_DEFAULT,
           (unsigned int)APP_BATMAN_DM_DSG_FET_PROTECTIONS_C_DEFAULT);
    printf("balance cfg:0x%02x max_cells:%u interval:%us temp:%d..%dC int_max:%dC vmin:%umV delta:%u/%umV\r\n",
           (unsigned int)APP_BATMAN_DM_BALANCING_CONFIGURATION_HOST_ONLY,
           (unsigned int)APP_BATMAN_DM_CB_MAX_CELLS_DEFAULT,
           (unsigned int)APP_BATMAN_DM_CB_INTERVAL_DEFAULT_S,
           (int)APP_BATMAN_DM_CB_MIN_CELL_TEMP_DEFAULT,
           (int)APP_BATMAN_DM_CB_MAX_CELL_TEMP_DEFAULT,
           (int)APP_BATMAN_DM_CB_MAX_INTERNAL_TEMP_DEFAULT,
           (unsigned int)APP_BATMAN_DM_CB_MIN_CELL_V_CHARGE_DEFAULT,
           (unsigned int)APP_BATMAN_DM_CB_MIN_DELTA_CHARGE_DEFAULT,
           (unsigned int)APP_BATMAN_DM_CB_STOP_DELTA_CHARGE_DEFAULT);
}

static void App_BatMan_ConfigParameters(void)
{
    uint8_t data_u8;
    uint8_t data_u16[2];

    /*
     * 这一段只写“项目默认且已经明确”的参数：
     * - 6S cell mapping；
     * - 默认告警掩码；
     * - host balancing 模式；
     * - 充电泵/FET 相关默认值。
     * 阈值编码、保护细节不在这里臆造。
     */
    data_u8 = APP_BATMAN_DM_DA_CONFIGURATION_DEFAULT;
    (void)Int_BQ76952_WriteDataMemory(BQ76952_DM_DA_CONFIGURATION, &data_u8, 1u);

    data_u16[0] = (uint8_t)(BQ76952_CELL_MASK_6S_HW_CONFIRMED & 0xFFu);
    data_u16[1] = (uint8_t)(BQ76952_CELL_MASK_6S_HW_CONFIRMED >> 8u);
    (void)Int_BQ76952_WriteDataMemory(BQ76952_DM_VCELL_MODE, data_u16, 2u);

    data_u16[0] = (uint8_t)(APP_BATMAN_DM_PROTECTION_CONFIGURATION_DEFAULT & 0xFFu);
    data_u16[1] = (uint8_t)(APP_BATMAN_DM_PROTECTION_CONFIGURATION_DEFAULT >> 8u);
    (void)Int_BQ76952_WriteDataMemory(BQ76952_DM_PROTECTION_CONFIGURATION, data_u16, 2u);

    /* APP_BATMAN_DM_DEFAULT_ALARM_MASK_DEFAULT = 0xF800 -> little-endian {0x00, 0xF8}. */
    data_u16[0] = 0x00u;
    data_u16[1] = 0xF8u;
    (void)Int_BQ76952_WriteDataMemory(BQ76952_DM_DEFAULT_ALARM_MASK, data_u16, 2u);

    data_u8 = APP_BATMAN_DM_FET_OPTIONS_DEFAULT;
    (void)Int_BQ76952_WriteDataMemory(BQ76952_DM_FET_OPTIONS, &data_u8, 1u);

    data_u8 = APP_BATMAN_DM_CHG_PUMP_CONTROL_DEFAULT;
    (void)Int_BQ76952_WriteDataMemory(BQ76952_DM_CHG_PUMP_CONTROL, &data_u8, 1u);

    data_u8 = APP_BATMAN_DM_BALANCING_CONFIGURATION_HOST_ONLY;
    (void)Int_BQ76952_WriteDataMemory(BQ76952_DM_BALANCING_CONFIGURATION, &data_u8, 1u);

    data_u8 = APP_BATMAN_DM_CB_MIN_CELL_TEMP_DEFAULT;
    (void)Int_BQ76952_WriteDataMemory(BQ76952_DM_CB_MIN_CELL_TEMP, &data_u8, 1u);

    data_u8 = APP_BATMAN_DM_CB_MAX_CELL_TEMP_DEFAULT;
    (void)Int_BQ76952_WriteDataMemory(BQ76952_DM_CB_MAX_CELL_TEMP, &data_u8, 1u);

    data_u8 = APP_BATMAN_DM_CB_MAX_INTERNAL_TEMP_DEFAULT;
    (void)Int_BQ76952_WriteDataMemory(BQ76952_DM_CB_MAX_INTERNAL_TEMP, &data_u8, 1u);

    data_u8 = APP_BATMAN_DM_CB_INTERVAL_DEFAULT_S;
    (void)Int_BQ76952_WriteDataMemory(BQ76952_DM_CB_INTERVAL, &data_u8, 1u);

    data_u8 = APP_BATMAN_DM_CB_MAX_CELLS_DEFAULT;
    (void)Int_BQ76952_WriteDataMemory(BQ76952_DM_CB_MAX_CELLS, &data_u8, 1u);

    data_u16[0] = (uint8_t)(APP_BATMAN_DM_CB_MIN_CELL_V_CHARGE_DEFAULT & 0xFFu);
    data_u16[1] = (uint8_t)(APP_BATMAN_DM_CB_MIN_CELL_V_CHARGE_DEFAULT >> 8u);
    (void)Int_BQ76952_WriteDataMemory(BQ76952_DM_CB_MIN_CELL_V_CHARGE, data_u16, 2u);

    data_u8 = APP_BATMAN_DM_CB_MIN_DELTA_CHARGE_DEFAULT;
    (void)Int_BQ76952_WriteDataMemory(BQ76952_DM_CB_MIN_DELTA_CHARGE, &data_u8, 1u);

    data_u8 = APP_BATMAN_DM_CB_STOP_DELTA_CHARGE_DEFAULT;
    (void)Int_BQ76952_WriteDataMemory(BQ76952_DM_CB_STOP_DELTA_CHARGE, &data_u8, 1u);
}

static void App_BatMan_ConfigProtectSet(void)
{
    uint8_t data_u8;
    uint8_t data_u16[2];

    /*
     * 这里保留旧项目那种“先写默认保护再看回读”的风格。
     * 目的是 bring-up 阶段尽早确认寄存器写入是否正常，
     * 不是在这里做完整保护阈值编码。
     */
    data_u8 = APP_BATMAN_DM_PROTECTION_CONFIGURATION_DEFAULT;
    data_u16[0] = (uint8_t)(data_u8 & 0xFFu);
    data_u16[1] = (uint8_t)(data_u8 >> 8u);
    (void)Int_BQ76952_WriteDataMemory(BQ76952_DM_PROTECTION_CONFIGURATION, data_u16, 2u);

    data_u8 = APP_BATMAN_DM_ENABLED_PROTECTIONS_A_DEFAULT;
    (void)Int_BQ76952_WriteDataMemory(BQ76952_DM_ENABLED_PROTECTIONS_A, &data_u8, 1u);

    data_u8 = APP_BATMAN_DM_ENABLED_PROTECTIONS_B_DEFAULT;
    (void)Int_BQ76952_WriteDataMemory(BQ76952_DM_ENABLED_PROTECTIONS_B, &data_u8, 1u);

    data_u8 = APP_BATMAN_DM_ENABLED_PROTECTIONS_C_DEFAULT;
    (void)Int_BQ76952_WriteDataMemory(BQ76952_DM_ENABLED_PROTECTIONS_C, &data_u8, 1u);

    data_u8 = APP_BATMAN_DM_CHG_FET_PROTECTIONS_A_DEFAULT;
    (void)Int_BQ76952_WriteDataMemory(BQ76952_DM_CHG_FET_PROTECTIONS_A, &data_u8, 1u);

    data_u8 = APP_BATMAN_DM_CHG_FET_PROTECTIONS_B_DEFAULT;
    (void)Int_BQ76952_WriteDataMemory(BQ76952_DM_CHG_FET_PROTECTIONS_B, &data_u8, 1u);

    data_u8 = APP_BATMAN_DM_CHG_FET_PROTECTIONS_C_DEFAULT;
    (void)Int_BQ76952_WriteDataMemory(BQ76952_DM_CHG_FET_PROTECTIONS_C, &data_u8, 1u);

    data_u8 = APP_BATMAN_DM_DSG_FET_PROTECTIONS_A_DEFAULT;
    (void)Int_BQ76952_WriteDataMemory(BQ76952_DM_DSG_FET_PROTECTIONS_A, &data_u8, 1u);

    data_u8 = APP_BATMAN_DM_DSG_FET_PROTECTIONS_B_DEFAULT;
    (void)Int_BQ76952_WriteDataMemory(BQ76952_DM_DSG_FET_PROTECTIONS_B, &data_u8, 1u);

    data_u8 = APP_BATMAN_DM_DSG_FET_PROTECTIONS_C_DEFAULT;
    (void)Int_BQ76952_WriteDataMemory(BQ76952_DM_DSG_FET_PROTECTIONS_C, &data_u8, 1u);
}

static void App_BatMan_WriteFetControl(uint8_t fet_off_mask)
{
    (void)Int_BQ76952_WriteSubcommandData(BQ76952_SUBCMD_FET_CONTROL, &fet_off_mask, 1u);
}

void App_BatMan_Init(void)
{
    uint16_t device_number = 0u;
    uint8_t data[2];

    printf("batman init start\r\n");
    printf("battery:6S Li-ion pack:%u mv target:%u mv shunt:%u uohm balance:%u ohm\r\n",
           (unsigned int)APP_BATMAN_PACK_FULL_MV,
           (unsigned int)APP_BATMAN_PACK_CHG_5A_MV,
           (unsigned int)BQ76952_SHUNT_RESISTOR_UOHM,
           (unsigned int)APP_BATMAN_BALANCE_RESISTOR_OHM);
    App_BatMan_PrintBringupConfig();

    for (uint8_t i = 0u; i < APP_BATMAN_CELL_COUNT; i++)
    {
        cell_mv[i] = 0u;
    }

    stack_mv = 0u;
    pack_mv = 0u;
    current_ma = 0;
    current_a = 0.0f;
    cell_min_mv = 0u;
    cell_max_mv = 0u;
    cell_avg_mv = 0u;
    cell_delta_mv = 0u;
    temp_ic_c = 0;
    temp_ts1_c = 0;
    temp_ts3_c = 0;
    temp_cell_c = 0;
    temp_fet_c = 0;
    alarm_status = 0u;
    alarm_raw = 0u;
    battery_status = 0u;
    balance_mask = 0u;
    fet_status = 0u;
    safety_status_a = 0u;
    safety_status_b = 0u;
    safety_status_c = 0u;
    fault_active = false;
    charge_allowed = false;
    discharge_allowed = false;
    s_charge_request = true;
    s_discharge_request = true;
    s_comm_fault = false;
    s_balance_active = false;
    s_soc_seeded = false;
    s_coulomb_mah = (float)APP_BATMAN_CAPACITY_MAH * 0.5f;
    soc_percent = APP_BATMAN_DEFAULT_SOC_PERCENT;
    display_soc_percent = APP_BATMAN_DEFAULT_SOC_PERCENT;
    s_fet_off_mask = 0u;
    s_debug_ms = 0u;

    /*
     * 1. 板级 BQ 假设初始化
     *    - CRC 模式按项目默认打开；
     *    - WAKE 脚回到安全初值；
     *    - 不在这里直接写业务寄存器。
     */
    Int_BQ76952_InitBoard();

    /*
     * 2. 先做一次板级唤醒和协议复位
     *    旧项目风格是“先让芯片醒，再开始问它话”。
     */
    if (Int_BQ76952_Reset() != INT_BQ76952_OK)
    {
        printf("bq reset fail\r\n");
        return;
    }

    /*
     * 3. 读 Device Number，确认总线和 BQ76952 响应正确。
     */
    if (Int_BQ76952_ReadSubcommand(BQ76952_SUBCMD_DEVICE_NUMBER, data, 2u) != INT_BQ76952_OK)
    {
        printf("bq read device number fail\r\n");
        return;
    }
    device_number = App_BatMan_ReadU16Le(data);
    printf("bq device number:0x%04x\r\n", (unsigned int)device_number);

    /*
     * 4. 进入 ConfigUpdate，写本项目默认 Data Memory。
     */
    if (Int_BQ76952_EnterConfigUpdate() != INT_BQ76952_OK)
    {
        printf("bq enter config fail\r\n");
        return;
    }

    App_BatMan_ConfigParameters();
    App_BatMan_ConfigProtectSet();

    /*
     * 5. 回读关键配置，尽量在 bring-up 阶段早一点发现写错地址或 CRC 问题。
     */
    if (Int_BQ76952_ReadDataMemory(BQ76952_DM_VCELL_MODE, data, 2u) != INT_BQ76952_OK)
    {
        /* bring-up optional readback: keep init path running even if this read fails */
    }
    else
    {
        printf("vcell_mode:0x%04x\r\n", (unsigned int)App_BatMan_ReadU16Le(data));
    }

    if (Int_BQ76952_ExitConfigUpdate() != INT_BQ76952_OK)
    {
        printf("bq exit config fail\r\n");
        return;
    }

    /*
     * 6. 清理启动期告警，关闭均衡，读取一次状态。
     */
    data[0] = (uint8_t)(BQ76952_ALARM_INITSTART_MASK |
                        BQ76952_ALARM_INITCOMP_MASK |
                        BQ76952_ALARM_FULLSCAN_MASK |
                        BQ76952_ALARM_ADSCAN_MASK |
                        BQ76952_ALARM_WAKE_MASK);
    data[1] = (uint8_t)(((BQ76952_ALARM_INITSTART_MASK |
                          BQ76952_ALARM_INITCOMP_MASK |
                          BQ76952_ALARM_FULLSCAN_MASK |
                          BQ76952_ALARM_ADSCAN_MASK |
                          BQ76952_ALARM_WAKE_MASK) >> 8u) & 0xFFu);
    (void)Int_BQ76952_WriteDirect(BQ76952_CMD_ALARM_STATUS, data, 2u);
    (void)App_BatMan_StopBalancing();

    /*
     * 7. 允许 BQ 按保护条件自己管理 FET。
     *    APP 只下发“允许/禁止”的意图，不做强制硬拉高拉低。
     */
    if (Int_BQ76952_SendSubcommand(BQ76952_SUBCMD_FET_ENABLE) != INT_BQ76952_OK)
    {
        printf("bq fet enable fail\r\n");
        return;
    }

    /*
     * 8. 初次采样，给 SOC 和调试变量一个真实起点。
     */
    App_BatMan_LoadCellsVoltage();
    App_BatMan_LoadBatVoltage();
    App_BatMan_LoadCurrent();
    App_BatMan_LoadTemperature();
    App_BatMan_LoadBqStatus();
    App_BatMan_UpdateFaultState();

    if (cell_avg_mv > 0u)
    {
        uint8_t seed_soc = Com_BQ76952_GetPercentByVoltage(cell_avg_mv);

        soc_percent = (float)seed_soc;
        display_soc_percent = soc_percent;
        s_coulomb_mah = ((float)APP_BATMAN_CAPACITY_MAH * soc_percent) / 100.0f;
        s_soc_seeded = true;
    }

    charge_allowed = true;
    discharge_allowed = true;
    App_BatMan_ApplyPolicy();

    printf("batman init success\r\n");
}

void App_BatMan_Task(uint16_t interval_ms)
{
    s_comm_fault = false;

    App_BatMan_LoadCellsVoltage();
    App_BatMan_LoadBatVoltage();
    App_BatMan_LoadCurrent();
    App_BatMan_LoadTemperature();
    App_BatMan_LoadBqStatus();
    App_BatMan_UpdateFaultState();
    App_BatMan_CalcCoulomb(interval_ms);
    App_BatMan_CalcSoc(interval_ms);
    App_BatMan_CellBalance();
    App_BatMan_ApplyPolicy();

    s_debug_ms = (uint16_t)(s_debug_ms + interval_ms);
    if (s_debug_ms >= APP_BATMAN_DEBUG_PERIOD_MS)
    {
        s_debug_ms = 0u;
        App_BatMan_PrintDebug();
    }
}

void App_BatMan_LoadCellsVoltage(void)
{
    uint32_t cell_sum_mv = 0u;
    uint8_t data[2];
    const uint8_t commands[APP_BATMAN_CELL_COUNT] =
    {
        BQ76952_CMD_CELL1_VOLTAGE,
        BQ76952_CMD_CELL2_VOLTAGE,
        BQ76952_CMD_CELL6_VOLTAGE,
        BQ76952_CMD_CELL9_VOLTAGE,
        BQ76952_CMD_CELL12_VOLTAGE,
        BQ76952_CMD_CELL16_VOLTAGE
    };

    cell_min_mv = 0xFFFFu;
    cell_max_mv = 0u;

    for (uint8_t i = 0u; i < APP_BATMAN_CELL_COUNT; i++)
    {
        if (Int_BQ76952_ReadDirect(commands[i], data, 2u) != INT_BQ76952_OK)
        {
            s_comm_fault = true;
            printf("load cell voltage fail index:%u\r\n", (unsigned int)i);
            cell_mv[i] = 0u;
            continue;
        }

        cell_mv[i] = App_BatMan_ReadU16Le(data);
        cell_sum_mv += cell_mv[i];

        if (cell_mv[i] < cell_min_mv)
        {
            cell_min_mv = cell_mv[i];
        }

        if (cell_mv[i] > cell_max_mv)
        {
            cell_max_mv = cell_mv[i];
        }
    }

    if (cell_min_mv == 0xFFFFu)
    {
        cell_min_mv = 0u;
    }

    cell_avg_mv = (uint16_t)(cell_sum_mv / APP_BATMAN_CELL_COUNT);
    cell_delta_mv = (uint16_t)(cell_max_mv - cell_min_mv);
}

void App_BatMan_LoadBatVoltage(void)
{
    uint8_t data[2];
    uint16_t raw = 0u;

    if (Int_BQ76952_ReadDirect(BQ76952_CMD_STACK_VOLTAGE, data, 2u) != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        printf("load stack voltage fail\r\n");
        return;
    }

    raw = App_BatMan_ReadU16Le(data);
    stack_mv = raw;
    pack_mv = stack_mv;
    printf("stack_mv:%lu mv\r\n", (unsigned long)stack_mv);
}

void App_BatMan_LoadCurrent(void)
{
    uint8_t data[2];
    int16_t raw;

    if (Int_BQ76952_ReadDirect(BQ76952_CMD_CC2_CURRENT, data, 2u) != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        printf("load current fail\r\n");
        return;
    }

    raw = (int16_t)App_BatMan_ReadU16Le(data);
    current_ma = (int32_t)raw * APP_BATMAN_CC2_RAW_POLARITY;
    current_a = (float)current_ma / 1000.0f;

    printf("current_ma:%ld current_a:%.3f\r\n", (long)current_ma, (double)current_a);
}

void App_BatMan_LoadTemperature(void)
{
    uint8_t data[2];
    int16_t temp_0p1k;

    if (Int_BQ76952_ReadDirect(BQ76952_CMD_INT_TEMPERATURE, data, 2u) != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        printf("load ic temp fail\r\n");
        return;
    }
    temp_0p1k = (int16_t)App_BatMan_ReadU16Le(data);
    temp_ic_c = Com_BQ76952_Temp0p1KToC(temp_0p1k);

    if (Int_BQ76952_ReadDirect(BQ76952_CMD_TS1_TEMPERATURE, data, 2u) != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        temp_ts1_c = temp_ic_c;
        printf("load ts1 temp fail\r\n");
    }
    else
    {
        temp_0p1k = (int16_t)App_BatMan_ReadU16Le(data);
        temp_ts1_c = Com_BQ76952_Temp0p1KToC(temp_0p1k);
    }

    if (Int_BQ76952_ReadDirect(BQ76952_CMD_TS3_TEMPERATURE, data, 2u) != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        temp_ts3_c = temp_ic_c;
        printf("load ts3 temp fail\r\n");
    }
    else
    {
        temp_0p1k = (int16_t)App_BatMan_ReadU16Le(data);
        temp_ts3_c = Com_BQ76952_Temp0p1KToC(temp_0p1k);
    }

    temp_cell_c = (int16_t)((temp_ts1_c + temp_ts3_c) / 2);
    temp_fet_c = temp_ic_c;

    printf("temp_ic:%dC ts1:%dC ts3:%dC cell:%dC fet:%dC\r\n",
           temp_ic_c,
           temp_ts1_c,
           temp_ts3_c,
           temp_cell_c,
           temp_fet_c);
}

void App_BatMan_LoadBqStatus(void)
{
    uint8_t data[2];

    if (Int_BQ76952_ReadDirect(BQ76952_CMD_ALARM_STATUS, data, 2u) != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        printf("load alarm status fail\r\n");
    }
    else
    {
        alarm_status = App_BatMan_ReadU16Le(data);
    }

    if (Int_BQ76952_ReadDirect(BQ76952_CMD_ALARM_RAW_STATUS, data, 2u) != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        printf("load alarm raw fail\r\n");
    }
    else
    {
        alarm_raw = App_BatMan_ReadU16Le(data);
    }

    if (Int_BQ76952_ReadDirect(BQ76952_CMD_BATTERY_STATUS, data, 2u) != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        printf("load battery status fail\r\n");
    }
    else
    {
        battery_status = App_BatMan_ReadU16Le(data);
    }

    if (Int_BQ76952_ReadDirect(BQ76952_CMD_FET_STATUS, data, 1u) != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        printf("load fet status fail\r\n");
    }
    else
    {
        fet_status = data[0];
    }

    if (Int_BQ76952_ReadDirect(BQ76952_CMD_SAFETY_STATUS_A, data, 1u) != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        safety_status_a = 0u;
    }
    else
    {
        safety_status_a = data[0];
    }

    if (Int_BQ76952_ReadDirect(BQ76952_CMD_SAFETY_STATUS_B, data, 1u) != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        safety_status_b = 0u;
    }
    else
    {
        safety_status_b = data[0];
    }

    if (Int_BQ76952_ReadDirect(BQ76952_CMD_SAFETY_STATUS_C, data, 1u) != INT_BQ76952_OK)
    {
        s_comm_fault = true;
        safety_status_c = 0u;
    }
    else
    {
        safety_status_c = data[0];
    }
}

void App_BatMan_UpdateFaultState(void)
{
    bool cell_voltage_fault = false;
    bool safety_fault = false;

    for (uint8_t i = 0u; i < APP_BATMAN_CELL_COUNT; i++)
    {
        if ((cell_mv[i] < APP_BATMAN_CELL_VALID_MIN_MV) ||
            (cell_mv[i] > APP_BATMAN_CELL_VALID_MAX_MV))
        {
            cell_voltage_fault = true;
            printf("cell voltage fault index:%u mv:%u\r\n",
                   (unsigned int)i,
                   (unsigned int)cell_mv[i]);
        }
    }

    safety_fault = ((safety_status_a != 0u) ||
                    (safety_status_b != 0u) ||
                    (safety_status_c != 0u));

    fault_active = s_comm_fault || cell_voltage_fault || safety_fault;
    charge_allowed = s_charge_request && !fault_active;
    discharge_allowed = s_discharge_request && !fault_active;

    if (safety_fault)
    {
        printf("safety_a:%02x safety_b:%02x safety_c:%02x\r\n",
               safety_status_a,
               safety_status_b,
               safety_status_c);
    }
}

void App_BatMan_CalcCoulomb(uint16_t delta_ms)
{
    float delta_mah;

    delta_mah = ((float)current_ma * (float)delta_ms) / 3600000.0f;
    s_coulomb_mah += delta_mah;

    if (s_coulomb_mah < 0.0f)
    {
        s_coulomb_mah = 0.0f;
    }
    else if (s_coulomb_mah > (float)APP_BATMAN_CAPACITY_MAH)
    {
        s_coulomb_mah = (float)APP_BATMAN_CAPACITY_MAH;
    }
}

void App_BatMan_CalcSoc(uint16_t interval_ms)
{
    float coulomb_soc;
    float target_soc;
    float alpha;
    uint8_t ocv_soc;

    (void)interval_ms;

    if (!s_soc_seeded)
    {
        ocv_soc = Com_BQ76952_GetPercentByVoltage(cell_avg_mv);
        soc_percent = (float)ocv_soc;
        display_soc_percent = soc_percent;
        s_coulomb_mah = ((float)APP_BATMAN_CAPACITY_MAH * soc_percent) / 100.0f;
        s_soc_seeded = true;
    }

    coulomb_soc = (s_coulomb_mah / (float)APP_BATMAN_CAPACITY_MAH) * 100.0f;
    if (coulomb_soc < 0.0f)
    {
        coulomb_soc = 0.0f;
    }
    else if (coulomb_soc > 100.0f)
    {
        coulomb_soc = 100.0f;
    }

    target_soc = coulomb_soc;

    /*
     * 低电流时把 OCV 表轻轻拉进来，保留旧项目那种“电流积分 + 电压修正”的味道。
     * 这里不做厚 EKF，只做适合教学和 bring-up 的轻量融合。
     */
    if ((current_a >= -APP_BATMAN_CURRENT_LOW_CURRENT_A) &&
        (current_a <= APP_BATMAN_CURRENT_LOW_CURRENT_A) &&
        (cell_avg_mv > 0u))
    {
        ocv_soc = Com_BQ76952_GetPercentByVoltage(cell_avg_mv);
        target_soc = (0.7f * coulomb_soc) + (0.3f * (float)ocv_soc);
    }

    soc_percent = target_soc;

    if (current_a > 0.01f)
    {
        alpha = APP_BATMAN_SOC_BLEND_ALPHA_CHG;
    }
    else if (current_a < -0.01f)
    {
        alpha = APP_BATMAN_SOC_BLEND_ALPHA_DSG;
    }
    else
    {
        alpha = APP_BATMAN_SOC_BLEND_ALPHA_IDLE;
    }

    if (display_soc_percent == 0.0f)
    {
        display_soc_percent = soc_percent;
    }
    else
    {
        float candidate = display_soc_percent + alpha * (soc_percent - display_soc_percent);

        if ((current_a > 0.01f) && (candidate < display_soc_percent))
        {
            candidate = display_soc_percent;
        }
        else if ((current_a < -0.01f) && (candidate > display_soc_percent))
        {
            candidate = display_soc_percent;
        }

        if (candidate < 0.0f)
        {
            candidate = 0.0f;
        }
        else if (candidate > 100.0f)
        {
            candidate = 100.0f;
        }

        display_soc_percent = candidate;
    }

    soc_percent = display_soc_percent;
}

void App_BatMan_CellBalance(void)
{
    uint8_t cell_to_balance[APP_BATMAN_CELL_COUNT];
    uint16_t min_voltage_mv = 5000u;
    uint16_t start_delta_mv;
    uint8_t best_balance_index = APP_BATMAN_CELL_COUNT;
    uint16_t best_balance_mv = 0u;
    bool has_balance_cell = false;
    uint8_t i;

    for (i = 0u; i < APP_BATMAN_CELL_COUNT; i++)
    {
        cell_to_balance[i] = 0u;
    }

    if (fault_active ||
        (temp_cell_c < APP_BATMAN_BALANCE_MIN_TEMP_C) ||
        (temp_cell_c > APP_BATMAN_BALANCE_MAX_TEMP_C))
    {
        s_balance_active = false;
        balance_mask = 0u;
        (void)App_BatMan_StopBalancing();
        return;
    }

    for (i = 0u; i < APP_BATMAN_CELL_COUNT; i++)
    {
        if (cell_mv[i] < min_voltage_mv)
        {
            min_voltage_mv = cell_mv[i];
        }
    }

    start_delta_mv = s_balance_active ? APP_BATMAN_BALANCE_STOP_DELTA_MV :
                                        APP_BATMAN_BALANCE_START_DELTA_MV;

    for (i = 0u; i < APP_BATMAN_CELL_COUNT; i++)
    {
        if ((cell_mv[i] >= APP_BATMAN_BALANCE_MIN_CELL_MV) &&
            ((uint16_t)(cell_mv[i] - min_voltage_mv) >= start_delta_mv))
        {
            cell_to_balance[i] = 1u;
        }
    }

    for (i = 0u; i < (APP_BATMAN_CELL_COUNT - 1u); i++)
    {
        if ((cell_to_balance[i] != 0u) && (cell_to_balance[i + 1u] != 0u))
        {
            if (cell_mv[i] < cell_mv[i + 1u])
            {
                cell_to_balance[i] = 0u;
            }
            else
            {
                cell_to_balance[i + 1u] = 0u;
            }
        }
    }

    for (i = 0u; i < APP_BATMAN_CELL_COUNT; i++)
    {
        if ((cell_to_balance[i] != 0u) && (cell_mv[i] > best_balance_mv))
        {
            best_balance_mv = cell_mv[i];
            best_balance_index = i;
        }
    }

    for (i = 0u; i < APP_BATMAN_CELL_COUNT; i++)
    {
        if (i != best_balance_index)
        {
            cell_to_balance[i] = 0u;
        }
    }

    for (i = 0u; i < APP_BATMAN_CELL_COUNT; i++)
    {
        if (cell_to_balance[i] != 0u)
        {
            has_balance_cell = true;
        }
    }

    if (!has_balance_cell)
    {
        s_balance_active = false;
        balance_mask = 0u;
        (void)App_BatMan_StopBalancing();
        return;
    }

    s_balance_active = true;
    balance_mask = App_BatMan_PhysicalBalanceMaskToBqMask(cell_to_balance);
    if (App_BatMan_SetBalanceMask(balance_mask) != INT_BQ76952_OK)
    {
        /*
         * 这里直接写 host balancing mask，失败时就关掉均衡并留给打印输出。
         * 由于是 bring-up / 教学版本，优先让流程清晰，而不是把错误码传一整层。
         */
        s_balance_active = false;
        balance_mask = 0u;
        (void)App_BatMan_StopBalancing();
        printf("set balance mask fail\r\n");
        return;
    }
}

void App_BatMan_ApplyPolicy(void)
{
    uint8_t new_fet_off_mask = 0u;

    if (fault_active)
    {
        new_fet_off_mask = (BQ76952_FET_CONTROL_PCHG_OFF_MASK |
                            BQ76952_FET_CONTROL_CHG_OFF_MASK |
                            BQ76952_FET_CONTROL_PDSG_OFF_MASK |
                            BQ76952_FET_CONTROL_DSG_OFF_MASK);
    }
    else
    {
        if (!charge_allowed)
        {
            new_fet_off_mask |= (BQ76952_FET_CONTROL_PCHG_OFF_MASK |
                                  BQ76952_FET_CONTROL_CHG_OFF_MASK);
        }

        if (!discharge_allowed)
        {
            new_fet_off_mask |= (BQ76952_FET_CONTROL_PDSG_OFF_MASK |
                                  BQ76952_FET_CONTROL_DSG_OFF_MASK);
        }
    }

    if (new_fet_off_mask == s_fet_off_mask)
    {
        return;
    }

    s_fet_off_mask = new_fet_off_mask;
    App_BatMan_WriteFetControl(s_fet_off_mask);
}

void App_BatMan_PrintDebug(void)
{
    printf("stack:%lu mv pack:%lu mv current:%ld ma temp_ic:%d temp_ts1:%d temp_ts3:%d temp_cell:%d temp_fet:%d alarm:%04x raw:%04x batt:%04x fet:%02x safety:%02x/%02x/%02x bal:%04x soc:%.1f%% fault:%d chg:%d dsg:%d\r\n",
           (unsigned long)stack_mv,
           (unsigned long)pack_mv,
           (long)current_ma,
           temp_ic_c,
           temp_ts1_c,
           temp_ts3_c,
           temp_cell_c,
           temp_fet_c,
           (unsigned int)alarm_status,
           (unsigned int)alarm_raw,
           (unsigned int)battery_status,
           (unsigned int)fet_status,
           safety_status_a,
           safety_status_b,
           safety_status_c,
           (unsigned int)balance_mask,
           (double)display_soc_percent,
           fault_active ? 1 : 0,
           charge_allowed ? 1 : 0,
           discharge_allowed ? 1 : 0);

    for (uint8_t i = 0u; i < APP_BATMAN_CELL_COUNT; i++)
    {
        printf("cell_mv[%u]:%u mv\r\n",
               (unsigned int)i,
               (unsigned int)cell_mv[i]);
    }
}

void App_BatMan_SetChargeState(uint8_t charge_state)
{
    s_charge_request = (charge_state != 0u);
    charge_allowed = s_charge_request && !fault_active;
    App_BatMan_ApplyPolicy();
}

void App_BatMan_SetDischargeState(uint8_t discharge_state)
{
    s_discharge_request = (discharge_state != 0u);
    discharge_allowed = s_discharge_request && !fault_active;
    App_BatMan_ApplyPolicy();
}

void App_BatMan_SetChargeAllowed(bool allowed)
{
    App_BatMan_SetChargeState(allowed ? 1u : 0u);
}

void App_BatMan_SetDischargeAllowed(bool allowed)
{
    App_BatMan_SetDischargeState(allowed ? 1u : 0u);
}
