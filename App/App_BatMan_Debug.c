#include "App_BatMan_Internal.h"

#include <stdio.h>

#include "App_OLED.h"
#include "Int_BQ76952.h"

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
    printf("bq dm write8 fail addr:0x%04x\r\n", (unsigned int)address);
}

void App_BatMan_PrintDmWrite16Fail(uint16_t address)
{
    printf("bq dm write16 fail addr:0x%04x\r\n", (unsigned int)address);
}

void App_BatMan_PrintBqResetFail(Int_BQ76952_StatusTypeDef ret)
{
    printf("bq reset fail ret:%d hal:0x%08lx\r\n",
           (int)ret,
           (unsigned long)Int_BQ76952_GetLastHalError());
}

void App_BatMan_PrintBqDeviceFail(Int_BQ76952_StatusTypeDef ret)
{
    printf("bq device fail ret:%d hal:0x%08lx\r\n",
           (int)ret,
           (unsigned long)Int_BQ76952_GetLastHalError());
}

void App_BatMan_PrintBqOkDev(uint16_t device_number)
{
    printf("bq ok dev:0x%04x crc:%u\r\n",
           (unsigned int)device_number,
           Int_BQ76952_IsCrcEnabled() ? 1u : 0u);
}

void App_BatMan_PrintBqCfgEnterFail(void)
{
    printf("bq cfg enter fail\r\n");
}

void App_BatMan_PrintBqCfgWriteFail(void)
{
    printf("bq cfg write fail\r\n");
}

void App_BatMan_PrintBqCfgExitFail(void)
{
    printf("bq cfg exit fail\r\n");
}

void App_BatMan_PrintBqPowerConfig(uint16_t power_config)
{
    printf("bq power_config:0x%04x\r\n", (unsigned int)power_config);
}

void App_BatMan_PrintBqFetOffFail(void)
{
    printf("bq main fet off fail\r\n");
}

void App_BatMan_PrintInitOk(void)
{
    printf("batman init ok\r\n");
}

/**
 * @brief 输出一行紧凑调试信息。
 *
 * 当前版本面向持续运行，不再每秒逐节刷屏，避免串口日志过载。
 */
static void App_BatMan_PrintDebug(void)
{
    printf("bat cell:%u/%u/%u d:%u stack:%lu curr:%ld temp:%d fet:%02x safe:%02x/%02x/%02x soc:%.1f sc:%u res:%.1f%% k:%.3f p:%.3f bal:%04x soh:%u cyc:%lu fault:%u\r\n",
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
