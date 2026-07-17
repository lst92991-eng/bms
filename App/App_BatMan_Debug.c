#include "App_BatMan_Internal.h"

#include <stdio.h>

#include "App_DebugCli.h"
#include "App_OLED.h"
#include "Int_BQ76952_BSP.h"

static uint32_t s_debug_ms = 0u;

void App_BatMan_ResetDebugState(void)
{
    s_debug_ms = 0u;
}

void App_BatMan_UpdateRuntimeOledStatus(void)
{
    App_OLED_ShowBatteryStatus(!s_comm_fault,
                               display_soc_percent,
                               soh_capacity_valid,
                               soh_percent);
}

static void App_BatMan_PrintSeparator(const char *title)
{
    printf("---------- %s ----------\r\n", title);
}

static void App_BatMan_PrintSummary(void)
{
    printf("电池 摘要 电芯:%u/%u/%u 压差:%u 总压:%lu 电流:%ldmA 温度:%d FET:%02x SCD:%u XDSG:%u 故障:%u\r\n",
           (unsigned int)cell_min_mv,
           (unsigned int)cell_avg_mv,
           (unsigned int)cell_max_mv,
           (unsigned int)cell_delta_mv,
           (unsigned long)stack_mv,
           (long)current_ma,
           temp_cell_c,
           fet_status,
           (safety_status_a & BQ76952_SAFETY_A_SCD_MASK) != 0u ? 1u : 0u,
           (alarm_raw & BQ76952_ALARM_XDSG_MASK) != 0u ? 1u : 0u,
           fault_active ? 1u : 0u);
}

/**
 * @brief 输出一行紧凑调试信息。
 */
static void App_BatMan_PrintDebug(void)
{
    App_BatMan_PrintSeparator("BQ");
    App_BatMan_PrintSummary();
}

void App_BatMan_PrintMonitor(void)
{
    App_BatMan_PrintDebug();
}

void App_BatMan_PrintFastMonitor(void)
{
    const uint8_t ocd_active = (uint8_t)((safety_status_a &
        (BQ76952_SAFETY_A_OCD2_MASK | BQ76952_SAFETY_A_OCD1_MASK)) != 0u);
    const uint8_t pf_active = (uint8_t)((pf_status_a | pf_status_b | pf_status_c | pf_status_d) != 0u);

    printf("BQFAST I:%ldmA V:%lu C:%u/%u/%u RC:%u/%u Drop:%d(%d+%d) d:%u FET:%02x DSG:%u PDSG:%u SCD:%u OCD:%u CUV:%u XDSG:%u PF:%u\r\n",
           (long)current_ma,
           (unsigned long)stack_mv,
           (unsigned int)cell_min_mv,
           (unsigned int)cell_avg_mv,
           (unsigned int)cell_max_mv,
           (unsigned int)cell_min_rc_mv,
           (unsigned int)cell_avg_rc_mv,
           (int)cell_rc_total_mv,
           (int)cell_rc_ohmic_mv,
           (int)cell_rc_polar_mv,
           (unsigned int)cell_delta_mv,
           fet_status,
           (fet_status & BQ76952_FET_STATUS_DSG_FET_MASK) != 0u ? 1u : 0u,
           (fet_status & BQ76952_FET_STATUS_PDSG_FET_MASK) != 0u ? 1u : 0u,
           (safety_status_a & BQ76952_SAFETY_A_SCD_MASK) != 0u ? 1u : 0u,
           ocd_active,
           (safety_status_a & BQ76952_SAFETY_A_CUV_MASK) != 0u ? 1u : 0u,
           (alarm_raw & BQ76952_ALARM_XDSG_MASK) != 0u ? 1u : 0u,
           pf_active);
}

bool App_BatMan_IsMonitorFaultActive(void)
{
    const uint16_t alarm_fault_mask = (BQ76952_ALARM_SSBC_MASK |
                                       BQ76952_ALARM_SSA_MASK |
                                       BQ76952_ALARM_PF_MASK |
                                       BQ76952_ALARM_MSK_SFALERT_MASK |
                                       BQ76952_ALARM_MSK_PFALERT_MASK |
                                       BQ76952_ALARM_XDSG_MASK |
                                       BQ76952_ALARM_SHUTV_MASK |
                                       BQ76952_ALARM_FUSE_MASK);

    if (s_comm_fault ||
        !s_cells_sample_valid ||
        !s_current_sample_valid ||
        !s_temp_cell_sample_valid ||
        fault_active)
    {
        return true;
    }

    if (((safety_status_a | safety_status_b | safety_status_c) != 0u) ||
        ((pf_status_a | pf_status_b | pf_status_c | pf_status_d) != 0u) ||
        ((alarm_raw & alarm_fault_mask) != 0u))
    {
        return true;
    }

    if ((fet_status & BQ76952_FET_STATUS_DSG_FET_MASK) == 0u)
    {
        return true;
    }

    return false;
}

static void App_BatMan_PrintReasonFlag(bool active, const char *text)
{
    if (active)
    {
        printf("BQFAST原因 %s\r\n", text);
    }
}

void App_BatMan_PrintMonitorStopReason(void)
{
    App_BatMan_PrintSeparator("BQFAST停表-BQ原因");
    printf("BQFAST关键量 I:%ldmA Stack:%lu Pack:%lu Cell:%u/%u/%u RC:%u/%u Drop:%d(%d+%d) d:%u Temp:%d FET:%02x Req:%02x\r\n",
           (long)current_ma,
           (unsigned long)stack_mv,
           (unsigned long)pack_mv,
           (unsigned int)cell_min_mv,
           (unsigned int)cell_avg_mv,
           (unsigned int)cell_max_mv,
           (unsigned int)cell_min_rc_mv,
           (unsigned int)cell_avg_rc_mv,
           (int)cell_rc_total_mv,
           (int)cell_rc_ohmic_mv,
           (int)cell_rc_polar_mv,
           (unsigned int)cell_delta_mv,
           temp_cell_c,
           fet_status,
           fet_control_request);
    printf("BQFAST寄存器 ALM:%04x RAW:%04x SAFE:%02x/%02x/%02x PF:%02x/%02x/%02x/%02x BAT:%04x MFG:%04x\r\n",
           (unsigned int)alarm_status,
           (unsigned int)alarm_raw,
           (unsigned int)safety_status_a,
           (unsigned int)safety_status_b,
           (unsigned int)safety_status_c,
           (unsigned int)pf_status_a,
           (unsigned int)pf_status_b,
           (unsigned int)pf_status_c,
           (unsigned int)pf_status_d,
           (unsigned int)battery_status,
           (unsigned int)manufacturing_status);

    App_BatMan_PrintReasonFlag(s_comm_fault, "BQ通信失败或最近一次采样通信异常");
    App_BatMan_PrintReasonFlag(!s_cells_sample_valid, "电芯电压采样无效");
    App_BatMan_PrintReasonFlag(!s_current_sample_valid, "电流采样无效");
    App_BatMan_PrintReasonFlag(!s_temp_cell_sample_valid, "温度采样无效");
    App_BatMan_PrintReasonFlag(fault_active, "APP电池管理总故障置位");
    App_BatMan_PrintReasonFlag((safety_status_a & BQ76952_SAFETY_A_SCD_MASK) != 0u, "SCD放电短路保护触发");
    App_BatMan_PrintReasonFlag((safety_status_a & BQ76952_SAFETY_A_OCD2_MASK) != 0u, "OCD2放电过流二级保护触发");
    App_BatMan_PrintReasonFlag((safety_status_a & BQ76952_SAFETY_A_OCD1_MASK) != 0u, "OCD1放电过流一级保护触发");
    App_BatMan_PrintReasonFlag((safety_status_a & BQ76952_SAFETY_A_CUV_MASK) != 0u, "CUV单体欠压保护触发");
    App_BatMan_PrintReasonFlag((safety_status_a & BQ76952_SAFETY_A_COV_MASK) != 0u, "COV单体过压保护触发");
    App_BatMan_PrintReasonFlag((safety_status_b & BQ76952_SAFETY_B_OTF_MASK) != 0u, "FET过温保护触发");
    App_BatMan_PrintReasonFlag((safety_status_b & BQ76952_SAFETY_B_OTD_MASK) != 0u, "放电过温保护触发");
    App_BatMan_PrintReasonFlag((safety_status_b & BQ76952_SAFETY_B_UTD_MASK) != 0u, "放电低温保护触发");
    App_BatMan_PrintReasonFlag((safety_status_c & BQ76952_SAFETY_C_OCD3_MASK) != 0u, "OCD3放电过流三级保护触发");
    App_BatMan_PrintReasonFlag((safety_status_c & BQ76952_SAFETY_C_SCDL_MASK) != 0u, "SCD短路锁存触发");
    App_BatMan_PrintReasonFlag((safety_status_c & BQ76952_SAFETY_C_OCDL_MASK) != 0u, "OCD过流锁存触发");
    App_BatMan_PrintReasonFlag((alarm_raw & BQ76952_ALARM_XDSG_MASK) != 0u, "XDSG置位，BQ当前禁止/关闭放电FET");
    App_BatMan_PrintReasonFlag((fet_status & BQ76952_FET_STATUS_DSG_FET_MASK) == 0u, "DSG实际未打开，放电主通道已断开");
    App_BatMan_PrintReasonFlag((pf_status_a | pf_status_b | pf_status_c | pf_status_d) != 0u, "PF永久失效状态非零");
}

static void App_BatMan_PrintFetDetail(void)
{
    printf("BQ FET位 CHG:%u DSG:%u PCHG:%u PDSG:%u DCHG:%u DDSG:%u ALRT:%u\r\n",
           (fet_status & BQ76952_FET_STATUS_CHG_FET_MASK) != 0u ? 1u : 0u,
           (fet_status & BQ76952_FET_STATUS_DSG_FET_MASK) != 0u ? 1u : 0u,
           (fet_status & BQ76952_FET_STATUS_PCHG_FET_MASK) != 0u ? 1u : 0u,
           (fet_status & BQ76952_FET_STATUS_PDSG_FET_MASK) != 0u ? 1u : 0u,
           (fet_status & BQ76952_FET_STATUS_DCHG_PIN_MASK) != 0u ? 1u : 0u,
           (fet_status & BQ76952_FET_STATUS_DDSG_PIN_MASK) != 0u ? 1u : 0u,
           (fet_status & BQ76952_FET_STATUS_ALRT_PIN_MASK) != 0u ? 1u : 0u);
    printf("BQ诊断 MFG:%04x BAT:%04x ALM:%04x RAW:%04x FET请求:%02x 安告:%02x/%02x/%02x 安全:%02x/%02x/%02x PF:%02x/%02x/%02x/%02x\r\n",
           (unsigned int)manufacturing_status,
           (unsigned int)battery_status,
           (unsigned int)alarm_status,
           (unsigned int)alarm_raw,
           (unsigned int)fet_control_request,
           (unsigned int)safety_alert_a,
           (unsigned int)safety_alert_b,
           (unsigned int)safety_alert_c,
           (unsigned int)safety_status_a,
           (unsigned int)safety_status_b,
           (unsigned int)safety_status_c,
           (unsigned int)pf_status_a,
           (unsigned int)pf_status_b,
           (unsigned int)pf_status_c,
           (unsigned int)pf_status_d);
    printf("BQ限制 XCHG:%u XDSG:%u FET_EN:%u CFG:%u PCHG:%u SLEEP_EN:%u POR:%u\r\n",
           (alarm_raw & BQ76952_ALARM_XCHG_MASK) != 0u ? 1u : 0u,
           (alarm_raw & BQ76952_ALARM_XDSG_MASK) != 0u ? 1u : 0u,
           (manufacturing_status & BQ76952_MFG_STATUS_FET_EN_MASK) != 0u ? 1u : 0u,
           (battery_status & BQ76952_BATTERY_STATUS_CFGUPDATE_MASK) != 0u ? 1u : 0u,
           (battery_status & BQ76952_BATTERY_STATUS_PCHG_MODE_MASK) != 0u ? 1u : 0u,
           (battery_status & BQ76952_BATTERY_STATUS_SLEEP_EN_MASK) != 0u ? 1u : 0u,
           (battery_status & BQ76952_BATTERY_STATUS_POR_MASK) != 0u ? 1u : 0u);
    printf("BQ SDM:%u\r\n",
           (battery_status & BQ76952_BATTERY_STATUS_SDM_MASK) != 0u ? 1u : 0u);
}

void App_BatMan_PrintSnapshot(void)
{
    App_BatMan_PrintSeparator("BQ详细");
    printf("BQ快照 通信故障:%u 电芯有效:%u 电流有效:%u 温度有效:%u 总故障:%u\r\n",
           s_comm_fault ? 1u : 0u,
           s_cells_sample_valid ? 1u : 0u,
           s_current_sample_valid ? 1u : 0u,
           s_temp_cell_sample_valid ? 1u : 0u,
           fault_active ? 1u : 0u);
    printf("BQ逐串 C1:%u C2:%u C3:%u C4:%u C5:%u C6:%u 最低:%u 平均:%u 最高:%u RC:%u/%u Drop:%d(%d+%d) 压差:%u 总压:%lu 包压:%lu 电流:%ld\r\n",
           (unsigned int)cell_mv[0],
           (unsigned int)cell_mv[1],
           (unsigned int)cell_mv[2],
           (unsigned int)cell_mv[3],
           (unsigned int)cell_mv[4],
           (unsigned int)cell_mv[5],
           (unsigned int)cell_min_mv,
           (unsigned int)cell_avg_mv,
           (unsigned int)cell_max_mv,
           (unsigned int)cell_min_rc_mv,
           (unsigned int)cell_avg_rc_mv,
           (int)cell_rc_total_mv,
           (int)cell_rc_ohmic_mv,
           (int)cell_rc_polar_mv,
           (unsigned int)cell_delta_mv,
           (unsigned long)stack_mv,
           (unsigned long)pack_mv,
           (long)current_ma);
    App_BatMan_PrintSummary();
    App_BatMan_PrintFetDetail();
    printf("BQ保护A SCD:%u OCD2:%u OCD1:%u OCC:%u COV:%u CUV:%u 原始:告警%02x 状态%02x\r\n",
           (safety_status_a & BQ76952_SAFETY_A_SCD_MASK) != 0u ? 1u : 0u,
           (safety_status_a & BQ76952_SAFETY_A_OCD2_MASK) != 0u ? 1u : 0u,
           (safety_status_a & BQ76952_SAFETY_A_OCD1_MASK) != 0u ? 1u : 0u,
           (safety_status_a & BQ76952_SAFETY_A_OCC_MASK) != 0u ? 1u : 0u,
           (safety_status_a & BQ76952_SAFETY_A_COV_MASK) != 0u ? 1u : 0u,
           (safety_status_a & BQ76952_SAFETY_A_CUV_MASK) != 0u ? 1u : 0u,
           (unsigned int)safety_alert_a,
           (unsigned int)safety_status_a);
    printf("BQ保护B OTF:%u OTINT:%u OTD:%u OTC:%u UTINT:%u UTD:%u UTC:%u 原始:告警%02x 状态%02x\r\n",
           (safety_status_b & BQ76952_SAFETY_B_OTF_MASK) != 0u ? 1u : 0u,
           (safety_status_b & BQ76952_SAFETY_B_OTINT_MASK) != 0u ? 1u : 0u,
           (safety_status_b & BQ76952_SAFETY_B_OTD_MASK) != 0u ? 1u : 0u,
           (safety_status_b & BQ76952_SAFETY_B_OTC_MASK) != 0u ? 1u : 0u,
           (safety_status_b & BQ76952_SAFETY_B_UTINT_MASK) != 0u ? 1u : 0u,
           (safety_status_b & BQ76952_SAFETY_B_UTD_MASK) != 0u ? 1u : 0u,
           (safety_status_b & BQ76952_SAFETY_B_UTC_MASK) != 0u ? 1u : 0u,
           (unsigned int)safety_alert_b,
           (unsigned int)safety_status_b);
    printf("BQ保护C OCD3:%u SCD锁存:%u OCD锁存:%u COV锁存:%u 预充超时:%u 看门狗:%u 原始:告警%02x 状态%02x\r\n",
           (safety_status_c & BQ76952_SAFETY_C_OCD3_MASK) != 0u ? 1u : 0u,
           (safety_status_c & BQ76952_SAFETY_C_SCDL_MASK) != 0u ? 1u : 0u,
           (safety_status_c & BQ76952_SAFETY_C_OCDL_MASK) != 0u ? 1u : 0u,
           (safety_status_c & BQ76952_SAFETY_C_COVL_MASK) != 0u ? 1u : 0u,
           (safety_status_c & BQ76952_SAFETY_C_PTO_MASK) != 0u ? 1u : 0u,
           (safety_status_c & BQ76952_SAFETY_C_HWDF_MASK) != 0u ? 1u : 0u,
           (unsigned int)safety_alert_c,
           (unsigned int)safety_status_c);
    printf("BQ告警位 SSA:%u SSBC:%u PF:%u SF_ALERT:%u PF_ALERT:%u XCHG:%u XDSG:%u 均衡:%u 唤醒:%u ALM:%04x RAW:%04x\r\n",
           (alarm_raw & BQ76952_ALARM_SSA_MASK) != 0u ? 1u : 0u,
           (alarm_raw & BQ76952_ALARM_SSBC_MASK) != 0u ? 1u : 0u,
           (alarm_raw & BQ76952_ALARM_PF_MASK) != 0u ? 1u : 0u,
           (alarm_raw & BQ76952_ALARM_MSK_SFALERT_MASK) != 0u ? 1u : 0u,
           (alarm_raw & BQ76952_ALARM_MSK_PFALERT_MASK) != 0u ? 1u : 0u,
           (alarm_raw & BQ76952_ALARM_XCHG_MASK) != 0u ? 1u : 0u,
           (alarm_raw & BQ76952_ALARM_XDSG_MASK) != 0u ? 1u : 0u,
           (alarm_raw & BQ76952_ALARM_CB_MASK) != 0u ? 1u : 0u,
           (alarm_raw & BQ76952_ALARM_WAKE_MASK) != 0u ? 1u : 0u,
           (unsigned int)alarm_status,
           (unsigned int)alarm_raw);
}

void App_BatMan_UpdateDebugOutput(uint32_t interval_ms)
{
    if (App_DebugCli_IsStreaming())
    {
        return;
    }

    s_debug_ms += interval_ms;
    if (s_debug_ms >= APP_BATMAN_DEBUG_PERIOD_MS)
    {
        s_debug_ms = 0u;
        App_BatMan_PrintDebug();
    }
}
