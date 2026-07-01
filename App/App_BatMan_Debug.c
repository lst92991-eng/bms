

#include "App_BatMan_Internal.h"

#include <stdio.h>

#include "App_OLED.h"
#include "Int_BQ76952.h"
#include "Int_BQ76952_BSP.h"

static uint32_t s_debug_ms = 0u;

void App_BatMan_ResetDebugState(void)
{
    s_debug_ms = 0u;
}

void App_BatMan_ShowIicStatus(bool ok)
{
    App_OLED_ShowIicStatus(ok);
}

void App_BatMan_ShowPowerConfig(bool ok, uint16_t power_config)
{
    App_OLED_ShowBqIicPowerConfig(ok, power_config);
}

void App_BatMan_UpdateRuntimeOledStatus(void)
{
    App_OLED_ShowIicStatus(!s_comm_fault);
}

void App_BatMan_PrintDmWrite8Fail(uint16_t address)
{
    printf("BQ配置写8位失败 地址:0x%04x\r\n", (unsigned int)address);
}

void App_BatMan_PrintDmWrite16Fail(uint16_t address)
{
    printf("BQ配置写16位失败 地址:0x%04x\r\n", (unsigned int)address);
}

void App_BatMan_PrintBqResetFail(Int_BQ76952_StatusTypeDef ret)
{
    printf("BQ复位失败 ret:%d hal:0x%08lx\r\n",
           (int)ret,
           (unsigned long)Int_BQ76952_GetLastHalError());
}

void App_BatMan_PrintBqDeviceFail(Int_BQ76952_StatusTypeDef ret)
{
    printf("BQ设备号读取失败 ret:%d hal:0x%08lx\r\n",
           (int)ret,
           (unsigned long)Int_BQ76952_GetLastHalError());
}

void App_BatMan_PrintBqOkDev(uint16_t device_number)
{
    printf("BQ通信正常 设备号:0x%04x CRC:%u\r\n",
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

/**
 * @brief 输出一行紧凑调试信息。
 *
 * 当前版本面向持续运行，不再每秒逐节刷屏，避免串口日志过载。
 */
static void App_BatMan_PrintDebug(void)
{
    printf("电池 电芯:%u/%u/%u 压差:%u 总压:%lu 电流:%ld 温度:%d FET:%02x 安全:%02x/%02x/%02x SOC:%.1f 可信:%u 残差:%.1f%% K:%.3f P:%.3f 均衡:%04x SOH:%u 循环:%lu 故障:%u\r\n",
           (unsigned int)cell_min_mv,
           (unsigned int)cell_avg_mv,
           (unsigned int)cell_max_mv,
           (unsigned int)cell_delta_mv,
           (unsigned long)stack_mv,
           (long)current_ma,
           temp_cell_c,
           fet_status,
           safety_status_a,
           safety_status_b,
           safety_status_c,
           (double)display_soc_percent,
           soc_confidence_percent,
           (double)soc_residual_percent,
           (double)soc_kalman_gain,
           (double)soc_p,
           balance_mask,
           soh_percent,
           (unsigned long)cycle_count,
           fault_active ? 1u : 0u);
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
}

void App_BatMan_UpdateDebugOutput(uint32_t interval_ms)
{
    s_debug_ms += interval_ms;
    if (s_debug_ms >= APP_BATMAN_DEBUG_PERIOD_MS)
    {
        s_debug_ms = 0u;
        App_BatMan_PrintDebug();
    }
}
