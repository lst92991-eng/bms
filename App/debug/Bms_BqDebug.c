

#include "App_BatMan_Internal.h"

#include <stddef.h>
#include <stdio.h>

#include "Bms_DebugCli.h"
#include "Bms_OledDebugView.h"
#include "Bms_Model.h"
#include "Bms_ProtectionService.h"
#include "Int_BQ76952.h"
#include "Int_BQ76952_BSP.h"

static uint32_t s_debug_ms = 0u;

void App_BatMan_ResetDebugState(void)
{
    s_debug_ms = 0u;
}

void App_BatMan_ShowIicStatus(bool ok)
{
    Bms_OledDebugView_ShowIicStatus(ok);
}

void App_BatMan_ShowPowerConfig(bool ok, uint16_t power_config)
{
    Bms_OledDebugView_ShowBqIicPowerConfig(ok, power_config);
}

void App_BatMan_UpdateRuntimeOledStatus(void)
{
    Bms_OledDebugView_ShowIicStatus(!s_comm_fault);
}

void App_BatMan_PrintDmWrite8Fail(uint16_t address)
{
    printf("BQ配置�?位失�?地址:0x%04x\r\n", (unsigned int)address);
}

void App_BatMan_PrintDmWrite16Fail(uint16_t address)
{
    printf("BQ配置�?6位失�?地址:0x%04x\r\n", (unsigned int)address);
}

void App_BatMan_PrintDmWrite32Fail(uint16_t address)
{
    printf("BQ配置�?2位失�?地址:0x%04x\r\n", (unsigned int)address);
}

void App_BatMan_PrintBqResetFail(Int_BQ76952_StatusTypeDef ret)
{
    printf("BQ复位失败 ret:%d hal:0x%08lx\r\n",
           (int)ret,
           (unsigned long)Int_BQ76952_GetLastHalError());
}

void App_BatMan_PrintBqDeviceFail(Int_BQ76952_StatusTypeDef ret)
{
    printf("BQ设备号读取失�?ret:%d hal:0x%08lx\r\n",
           (int)ret,
           (unsigned long)Int_BQ76952_GetLastHalError());
}

void App_BatMan_PrintBqOkDev(uint16_t device_number)
{
    printf("BQ通信正常 设备�?0x%04x CRC:%u\r\n",
           (unsigned int)device_number,
           Int_BQ76952_IsCrcEnabled() ? 1u : 0u);
}

void App_BatMan_PrintBqCfgEnterFail(void)
{
    printf("BQ配置模式进入失败\r\n");
}

void App_BatMan_PrintBqCfgWriteFail(void)
{
    printf("BQ配置写入失败\r\n");
}

void App_BatMan_PrintBqCfgExitFail(void)
{
    printf("BQ配置模式退出失败\r\n");
}

void App_BatMan_PrintBqPowerConfig(uint16_t power_config)
{
    printf("BQ电源配置:0x%04x\r\n", (unsigned int)power_config);
}

void App_BatMan_PrintBqFetOffFail(void)
{
    printf("BQ主FET默认关断失败\r\n");
}

void App_BatMan_PrintInitOk(void)
{
    printf("电池管理初始化成功\r\n");
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
 * @brief 输出一行紧凑调试信息�? */
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
    return Bms_ProtectionService_IsMonitorFaultActive(Bms_Model_GetContext());
}

void App_BatMan_PrintMonitorStopReason(void)
{
    Bms_ProtectionReasonTypeDef reasons[BMS_PROTECTION_MONITOR_REASON_MAX];
    size_t reason_count;
    size_t i;

    App_BatMan_PrintSeparator("BQFAST停表-BQ原因");
    printf("BQFAST关键�?I:%ldmA Stack:%lu Pack:%lu Cell:%u/%u/%u RC:%u/%u Drop:%d(%d+%d) d:%u Temp:%d FET:%02x Req:%02x\r\n",
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
    printf("BQFAST寄存�?ALM:%04x RAW:%04x SAFE:%02x/%02x/%02x PF:%02x/%02x/%02x/%02x BAT:%04x MFG:%04x\r\n",
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

    reason_count = Bms_ProtectionService_CollectMonitorFaultReasons(Bms_Model_GetContext(),
                                                                     reasons,
                                                                     BMS_PROTECTION_MONITOR_REASON_MAX);
    for (i = 0u; (i < reason_count) && (i < BMS_PROTECTION_MONITOR_REASON_MAX); i++)
    {
        printf("BQFAST原因 %s\r\n", reasons[i].text);
    }
    if (reason_count > BMS_PROTECTION_MONITOR_REASON_MAX)
    {
        printf("BQFAST原因 保护原因过多，已截断显示:%u/%u\r\n",
               (unsigned int)BMS_PROTECTION_MONITOR_REASON_MAX,
               (unsigned int)reason_count);
    }
}

static void App_BatMan_PrintFetDetail(void)
{
    printf("BQ FET�?CHG:%u DSG:%u PCHG:%u PDSG:%u DCHG:%u DDSG:%u ALRT:%u\r\n",
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
}

void App_BatMan_PrintSnapshot(void)
{
    App_BatMan_PrintSeparator("BQ详细");
    printf("BQ快照 通信故障:%u 电芯有效:%u 电流有效:%u 温度有效:%u 总故�?%u\r\n",
           s_comm_fault ? 1u : 0u,
           s_cells_sample_valid ? 1u : 0u,
           s_current_sample_valid ? 1u : 0u,
           s_temp_cell_sample_valid ? 1u : 0u,
           fault_active ? 1u : 0u);
    printf("BQ逐串 C1:%u C2:%u C3:%u C4:%u C5:%u C6:%u 最�?%u 平均:%u 最�?%u RC:%u/%u Drop:%d(%d+%d) 压差:%u 总压:%lu 包压:%lu 电流:%ld\r\n",
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
    printf("BQA SCD:%u OCD2:%u OCD1:%u OCC:%u COV:%u CUV:%u alert:%02x status:%02x\r\n",
           (safety_status_a & BQ76952_SAFETY_A_SCD_MASK) != 0u ? 1u : 0u,
           (safety_status_a & BQ76952_SAFETY_A_OCD2_MASK) != 0u ? 1u : 0u,
           (safety_status_a & BQ76952_SAFETY_A_OCD1_MASK) != 0u ? 1u : 0u,
           (safety_status_a & BQ76952_SAFETY_A_OCC_MASK) != 0u ? 1u : 0u,
           (safety_status_a & BQ76952_SAFETY_A_COV_MASK) != 0u ? 1u : 0u,
           (safety_status_a & BQ76952_SAFETY_A_CUV_MASK) != 0u ? 1u : 0u,
           (unsigned int)safety_alert_a,
           (unsigned int)safety_status_a);
    printf("BQB OTF:%u OTINT:%u OTD:%u OTC:%u UTINT:%u UTD:%u UTC:%u alert:%02x status:%02x\r\n",
           (safety_status_b & BQ76952_SAFETY_B_OTF_MASK) != 0u ? 1u : 0u,
           (safety_status_b & BQ76952_SAFETY_B_OTINT_MASK) != 0u ? 1u : 0u,
           (safety_status_b & BQ76952_SAFETY_B_OTD_MASK) != 0u ? 1u : 0u,
           (safety_status_b & BQ76952_SAFETY_B_OTC_MASK) != 0u ? 1u : 0u,
           (safety_status_b & BQ76952_SAFETY_B_UTINT_MASK) != 0u ? 1u : 0u,
           (safety_status_b & BQ76952_SAFETY_B_UTD_MASK) != 0u ? 1u : 0u,
           (safety_status_b & BQ76952_SAFETY_B_UTC_MASK) != 0u ? 1u : 0u,
           (unsigned int)safety_alert_b,
           (unsigned int)safety_status_b);
    printf("BQC OCD3:%u SCDL:%u OCDL:%u COVL:%u PTO:%u WDT:%u alert:%02x status:%02x\r\n",
           (safety_status_c & BQ76952_SAFETY_C_OCD3_MASK) != 0u ? 1u : 0u,
           (safety_status_c & BQ76952_SAFETY_C_SCDL_MASK) != 0u ? 1u : 0u,
           (safety_status_c & BQ76952_SAFETY_C_OCDL_MASK) != 0u ? 1u : 0u,
           (safety_status_c & BQ76952_SAFETY_C_COVL_MASK) != 0u ? 1u : 0u,
           (safety_status_c & BQ76952_SAFETY_C_PTO_MASK) != 0u ? 1u : 0u,
           (safety_status_c & BQ76952_SAFETY_C_HWDF_MASK) != 0u ? 1u : 0u,
           (unsigned int)safety_alert_c,
           (unsigned int)safety_status_c);
    printf("BQ告警�?SSA:%u SSBC:%u PF:%u SF_ALERT:%u PF_ALERT:%u XCHG:%u XDSG:%u 均衡:%u 唤醒:%u ALM:%04x RAW:%04x\r\n",
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
    if (Bms_DebugCli_IsVofaStreaming() || Bms_DebugCli_IsBqMonitoring())
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
