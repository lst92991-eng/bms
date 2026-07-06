#include "Bms_DebugCli.h"

#include "App_DebugCli.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "App_BatMan.h"
#include "Bms_PowerService.h"
#include "Bms_Sc8815Port.h"
#include "Bms_Model.h"
#include "Int_BQ76952.h"
#include "Int_BQ76952_BSP.h"
#include "Int_SC8815.h"
#include "Int_SC8815_BSP.h"
#include "main.h"
#include "usart.h"

#define APP_DEBUG_CLI_LINE_SIZE 80u
#define APP_DEBUG_CLI_RX_SIZE   128u
#define APP_DEBUG_CLI_RX_MASK   (APP_DEBUG_CLI_RX_SIZE - 1u)
#define APP_DEBUG_CLI_VOFA_PERIOD_MS 100u
#define APP_DEBUG_CLI_BQ_PERIOD_MS 1000u
#define APP_DEBUG_CLI_BQ_FAST_PERIOD_MS 200u
#define APP_DEBUG_CLI_SOC_CSV_PERIOD_MS 1000u
#define APP_DEBUG_CLI_PACK_RAW_MAX_MV 5000u
#define APP_DEBUG_CLI_PDSG_PROBE_COUNT 20u

static char s_cli_line[APP_DEBUG_CLI_LINE_SIZE];
static uint8_t s_cli_pos;
static uint8_t s_rx_byte;
static uint8_t s_rx_buf[APP_DEBUG_CLI_RX_SIZE];
static volatile uint8_t s_rx_head;
static volatile uint8_t s_rx_tail;
static bool s_vofa_enabled;
static uint16_t s_vofa_ms;
static bool s_bq_monitor_enabled;
static bool s_bq_monitor_stop_on_fault;
static uint16_t s_bq_monitor_period_ms;
static uint16_t s_bq_monitor_ms;
static bool s_soc_csv_enabled;
static uint16_t s_soc_csv_ms;

static void App_DebugCli_StartRx(void)
{
    (void)HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1u);
}

static bool App_DebugCli_IsSpace(char ch)
{
    return ((ch == ' ') || (ch == '\t'));
}

static char App_DebugCli_ToLower(char ch)
{
    if ((ch >= 'A') && (ch <= 'Z'))
    {
        ch = (char)(ch - 'A' + 'a');
    }

    return ch;
}

static char *App_DebugCli_Trim(char *line)
{
    char *end;

    while (App_DebugCli_IsSpace(*line))
    {
        line++;
    }

    end = line + strlen(line);
    while ((end > line) && App_DebugCli_IsSpace(*(end - 1)))
    {
        end--;
    }
    *end = '\0';

    return line;
}

static void App_DebugCli_Normalize(char *line)
{
    bool last_space = false;
    char *src = line;
    char *dst = line;

    while (*src != '\0')
    {
        char ch = App_DebugCli_ToLower(*src++);

        if (App_DebugCli_IsSpace(ch))
        {
            if (!last_space)
            {
                *dst++ = ' ';
            }
            last_space = true;
        }
        else
        {
            *dst++ = ch;
            last_space = false;
        }
    }
    *dst = '\0';
}

static void App_DebugCli_PrintHelp(void)
{
    printf("CLI help: help ping diag bq bq on bqfast on bq off power fault clear scd clear dsg clear pdsg test pdsg probe pdsg off sc scprobe charge on charge off vofa vofa on vofa off csv csv off soccsv soccsv on soccsv off\r\n");
}

static void App_DebugCli_PrintSignedMilli(int32_t milli_value)
{
    uint32_t abs_value;

    if (milli_value < 0)
    {
        printf("-");
        abs_value = (uint32_t)(-milli_value);
    }
    else
    {
        abs_value = (uint32_t)milli_value;
    }

    printf("%lu.%03lu",
           (unsigned long)(abs_value / 1000u),
           (unsigned long)(abs_value % 1000u));
}

static void App_DebugCli_PrintUnsignedMilli(uint32_t milli_value)
{
    printf("%lu.%03lu",
           (unsigned long)(milli_value / 1000u),
           (unsigned long)(milli_value % 1000u));
}

static void App_DebugCli_PrintVofaFrame(void)
{
    uint32_t vofa_pack_mv = pack_mv;

    if ((vofa_pack_mv > 0u) && (vofa_pack_mv < APP_DEBUG_CLI_PACK_RAW_MAX_MV))
    {
        vofa_pack_mv *= APP_BATMAN_STACK_RAW_TO_MV;
    }

    App_DebugCli_PrintSignedMilli(current_ma);
    printf(",");
    App_DebugCli_PrintUnsignedMilli(vofa_pack_mv);
    printf("\r\n");
}

static int32_t App_DebugCli_AbsI32(int32_t value)
{
    if (value < 0)
    {
        return -value;
    }

    return value;
}

static int32_t App_DebugCli_FloatToCenti(float value)
{
    if (value < 0.0f)
    {
        return (int32_t)((value * 100.0f) - 0.5f);
    }

    return (int32_t)((value * 100.0f) + 0.5f);
}

static void App_DebugCli_PrintSignedCenti(int32_t centi_value)
{
    uint32_t abs_value;

    if (centi_value < 0)
    {
        printf("-");
        abs_value = (uint32_t)(-centi_value);
    }
    else
    {
        abs_value = (uint32_t)centi_value;
    }

    printf("%lu.%02lu",
           (unsigned long)(abs_value / 100u),
           (unsigned long)(abs_value % 100u));
}

static void App_DebugCli_PrintSocCsvHeader(void)
{
    printf("BMSCSV,MS,VOLTAGE(V),CURRENT(A),POWER(W),E_CAPACITY(mAh),SOC(%%),DISPLAY_SOC(%%),SOC_CONF(%%),SOC_RESIDUAL(%%),SOC_KALMAN_GAIN,SOC_P,CELL1(mV),CELL2(mV),CELL3(mV),CELL4(mV),CELL5(mV),CELL6(mV),CELL_MIN(mV),CELL_AVG(mV),CELL_MAX(mV),RC_MIN(mV),RC_AVG(mV),DROP(mV),DROP_OHMIC(mV),DROP_POLAR(mV),TEMP_CELL(C),TEMP_FET(C),FET,SAFE_A,SAFE_B,SAFE_C,PF_A,PF_B,PF_C,PF_D,ALARM_RAW,CHG_mAh,DSG_mAh,CYCLE,SOH(%%)\r\n");
}

static void App_DebugCli_PrintSocCsvFrame(void)
{
    const Bms_ContextTypeDef *ctx = Bms_Model_GetContext();
    int32_t power_mw;
    uint32_t power_abs_mw;

    power_mw = (int32_t)(((int64_t)ctx->pack.pack_mv *
                          (int64_t)ctx->pack.current_ma) / 1000);
    power_abs_mw = (uint32_t)App_DebugCli_AbsI32(power_mw);

    printf("BMSCSV,%lu,",
           (unsigned long)HAL_GetTick());
    App_DebugCli_PrintUnsignedMilli(ctx->pack.pack_mv);
    printf(",");
    App_DebugCli_PrintSignedMilli(ctx->pack.current_ma);
    printf(",");
    App_DebugCli_PrintUnsignedMilli(power_abs_mw);
    printf(",%lu,",
           (unsigned long)ctx->estimate.discharge_throughput_mah);
    App_DebugCli_PrintSignedCenti(App_DebugCli_FloatToCenti(ctx->estimate.soc_percent));
    printf(",");
    App_DebugCli_PrintSignedCenti(App_DebugCli_FloatToCenti(ctx->estimate.display_soc_percent));
    printf(",%u,",
           (unsigned int)ctx->estimate.soc_confidence_percent);
    App_DebugCli_PrintSignedCenti(App_DebugCli_FloatToCenti(ctx->estimate.soc_residual_percent));
    printf(",");
    App_DebugCli_PrintSignedCenti(App_DebugCli_FloatToCenti(ctx->estimate.soc_kalman_gain));
    printf(",");
    App_DebugCli_PrintSignedCenti(App_DebugCli_FloatToCenti(ctx->estimate.soc_p));
    printf(",%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%d,%d,%d,%d,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%04x,%lu,%lu,%lu,%u\r\n",
           (unsigned int)ctx->pack.mv[0],
           (unsigned int)ctx->pack.mv[1],
           (unsigned int)ctx->pack.mv[2],
           (unsigned int)ctx->pack.mv[3],
           (unsigned int)ctx->pack.mv[4],
           (unsigned int)ctx->pack.mv[5],
           (unsigned int)ctx->pack.min_mv,
           (unsigned int)ctx->pack.avg_mv,
           (unsigned int)ctx->pack.max_mv,
           (unsigned int)ctx->pack.min_rc_mv,
           (unsigned int)ctx->pack.avg_rc_mv,
           (int)ctx->pack.rc_total_mv,
           (int)ctx->pack.rc_ohmic_mv,
           (int)ctx->pack.rc_polar_mv,
           ctx->pack.temp_cell_c,
           ctx->pack.temp_fet_c,
           ctx->protection.fet_status,
           ctx->protection.safety_status_a,
           ctx->protection.safety_status_b,
           ctx->protection.safety_status_c,
           ctx->protection.pf_status_a,
           ctx->protection.pf_status_b,
           ctx->protection.pf_status_c,
           ctx->protection.pf_status_d,
           (unsigned int)ctx->protection.alarm_raw,
           (unsigned long)ctx->estimate.charge_throughput_mah,
           (unsigned long)ctx->estimate.discharge_throughput_mah,
           (unsigned long)ctx->estimate.cycle_count,
           (unsigned int)ctx->estimate.soh_percent);
}

static void App_DebugCli_StartSocCsv(void)
{
    s_soc_csv_enabled = true;
    s_soc_csv_ms = APP_DEBUG_CLI_SOC_CSV_PERIOD_MS;
    App_DebugCli_PrintSocCsvHeader();
    printf("CLI csv:1 period:%u ms\r\n",
           (unsigned int)APP_DEBUG_CLI_SOC_CSV_PERIOD_MS);
}

static void App_DebugCli_StopSocCsv(void)
{
    s_soc_csv_enabled = false;
    s_soc_csv_ms = 0u;
    printf("CLI csv:0\r\n");
}

static void App_DebugCli_PrintSc(void)
{
    Bms_Sc8815Port_PrintCliSnapshot();
}

static void App_DebugCli_PrintPower(void)
{
    Bms_PowerService_PrintSnapshot();
}

static void App_DebugCli_PrintDiag(void)
{
    App_DebugCli_PrintPower();
    App_DebugCli_PrintSc();
    App_BatMan_PrintSnapshot();
}

static bool App_DebugCli_ShouldStopBqFast(void)
{
    if (App_BatMan_IsMonitorFaultActive())
    {
        return true;
    }

    if ((Bms_PowerService_GetState() == APP_POWER_STATE_LOW) ||
        (Bms_PowerService_GetState() == APP_POWER_STATE_FAULT) ||
        !Bms_PowerService_IsDischargeAllowed())
    {
        return true;
    }

    return false;
}

static void App_DebugCli_StopBqFastAndPrintReason(void)
{
    s_bq_monitor_enabled = false;
    s_bq_monitor_stop_on_fault = false;
    s_bq_monitor_ms = 0u;

    printf("---------- BQFAST自动停表 ----------\r\n");
    printf("BQFAST: 检测到放电异常，已停止连续输出，下面是停表瞬间原因。\r\n");
    Bms_PowerService_PrintStopReason();
    App_BatMan_PrintMonitorStopReason();
    Bms_PowerService_PrintSnapshot();
    App_BatMan_PrintSnapshot();
    printf("BQFAST: 处理后可发�?fault clear，再发�?bqfast on 重新监视。\r\n");
}

static void App_DebugCli_PrintScProbe(void)
{
    uint8_t reg = 0u;
    uint8_t normal_mask = 0u;
    uint8_t swapped_mask = 0u;
    Int_SC8815_StatusTypeDef normal_read;
    Int_SC8815_StatusTypeDef swapped_read;

    for (uint8_t addr = 0x70u; addr <= 0x77u; addr++)
    {
        if (Int_SC8815_ProbeAddress(addr, false) == INT_SC8815_OK)
        {
            normal_mask |= (uint8_t)(1u << (addr - 0x70u));
        }
        if (Int_SC8815_ProbeAddress(addr, true) == INT_SC8815_OK)
        {
            swapped_mask |= (uint8_t)(1u << (addr - 0x70u));
        }
    }

    printf("---------- SC8815探测 ----------\r\n");
    normal_read = Int_SC8815_ReadRegWithLineOrder(SC8815_REG_STATUS, false, &reg);
    printf("CLI scprobe normal bus:%02x ack70_77:%02x read:%u val:%02x\r\n",
           (unsigned int)Int_SC8815_GetBusLevels(),
           (unsigned int)normal_mask,
           (unsigned int)normal_read,
           (unsigned int)reg);

    reg = 0u;
    swapped_read = Int_SC8815_ReadRegWithLineOrder(SC8815_REG_STATUS, true, &reg);
    printf("CLI scprobe swapped bus:%02x ack70_77:%02x read:%u val:%02x\r\n",
           (unsigned int)Int_SC8815_GetBusLevels(),
           (unsigned int)swapped_mask,
           (unsigned int)swapped_read,
           (unsigned int)reg);
}

static uint16_t App_DebugCli_ReadDirectU16(uint8_t command)
{
    uint8_t data[2] = {0u, 0u};

    if (Int_BQ76952_ReadDirect(command, data, 2u) != INT_BQ76952_OK)
    {
        return 0xFFFFu;
    }

    return (uint16_t)(((uint16_t)data[1] << 8u) | data[0]);
}

static uint8_t App_DebugCli_ReadDirectU8(uint8_t command)
{
    uint8_t value = 0xFFu;

    (void)Int_BQ76952_ReadDirect(command, &value, 1u);
    return value;
}

static void App_DebugCli_RunPdsgProbe(void)
{
    printf("---------- PDSG探测 ----------\r\n");
    if (!App_BatMan_TestPreDischargeOnly())
    {
        printf("PDSG探测 写入失败\r\n");
        return;
    }

    for (uint8_t i = 0u; i < APP_DEBUG_CLI_PDSG_PROBE_COUNT; i++)
    {
        uint8_t fet = App_DebugCli_ReadDirectU8(BQ76952_CMD_FET_STATUS);
        uint8_t safety_a = App_DebugCli_ReadDirectU8(BQ76952_CMD_SAFETY_STATUS_A);
        uint16_t alarm = App_DebugCli_ReadDirectU16(BQ76952_CMD_ALARM_RAW_STATUS);
        uint16_t pack_raw = App_DebugCli_ReadDirectU16(BQ76952_CMD_PACK_PIN_VOLTAGE);
        uint16_t ld_raw = App_DebugCli_ReadDirectU16(BQ76952_CMD_LD_PIN_VOLTAGE);

        printf("PDSG探测 %u FET:%02x CHG:%u DSG:%u PCHG:%u PDSG:%u SCD:%u XDSG:%u PACKraw:%u LDraw:%u\r\n",
               (unsigned int)i,
               (unsigned int)fet,
               (fet & BQ76952_FET_STATUS_CHG_FET_MASK) != 0u ? 1u : 0u,
               (fet & BQ76952_FET_STATUS_DSG_FET_MASK) != 0u ? 1u : 0u,
               (fet & BQ76952_FET_STATUS_PCHG_FET_MASK) != 0u ? 1u : 0u,
               (fet & BQ76952_FET_STATUS_PDSG_FET_MASK) != 0u ? 1u : 0u,
               (safety_a & BQ76952_SAFETY_A_SCD_MASK) != 0u ? 1u : 0u,
               (alarm & BQ76952_ALARM_XDSG_MASK) != 0u ? 1u : 0u,
               (unsigned int)pack_raw,
               (unsigned int)ld_raw);
    }

    (void)App_BatMan_AllMainFetsOff();
    printf("PDSG探测 完成: 已恢复全FET关断\r\n");
}

static void App_DebugCli_ProcessLine(char *line)
{
    line = App_DebugCli_Trim(line);
    App_DebugCli_Normalize(line);

    if (line[0] == '\0')
    {
        return;
    }

    if (strcmp(line, "help") == 0)
    {
        App_DebugCli_PrintHelp();
    }
    else if (strcmp(line, "ping") == 0)
    {
        printf("CLI pong tick:%lu\r\n", (unsigned long)HAL_GetTick());
    }
    else if (strcmp(line, "sc") == 0)
    {
        App_DebugCli_PrintSc();
    }
    else if (strcmp(line, "diag") == 0)
    {
        App_DebugCli_PrintDiag();
    }
    else if (strcmp(line, "bq") == 0)
    {
        App_BatMan_PrintSnapshot();
    }
    else if (strcmp(line, "bq on") == 0)
    {
        s_bq_monitor_enabled = true;
        s_bq_monitor_stop_on_fault = false;
        s_bq_monitor_period_ms = APP_DEBUG_CLI_BQ_PERIOD_MS;
        s_bq_monitor_ms = APP_DEBUG_CLI_BQ_PERIOD_MS;
        printf("CLI BQ连续监视: 开�?周期:%u ms\r\n",
               (unsigned int)s_bq_monitor_period_ms);
    }
    else if ((strcmp(line, "bqfast on") == 0) || (strcmp(line, "bq fast") == 0))
    {
        s_bq_monitor_enabled = true;
        s_bq_monitor_stop_on_fault = true;
        s_bq_monitor_period_ms = APP_DEBUG_CLI_BQ_FAST_PERIOD_MS;
        s_bq_monitor_ms = APP_DEBUG_CLI_BQ_FAST_PERIOD_MS;
        printf("CLI bqfast:1 周期:%u ms 异常自动停表:1\r\n",
               (unsigned int)s_bq_monitor_period_ms);
    }
    else if (strcmp(line, "bq off") == 0)
    {
        s_bq_monitor_enabled = false;
        s_bq_monitor_stop_on_fault = false;
        s_bq_monitor_ms = 0u;
        printf("CLI BQ连续监视: 关闭\r\n");
    }
    else if (strcmp(line, "power") == 0)
    {
        App_DebugCli_PrintPower();
    }
    else if ((strcmp(line, "fault clear") == 0) ||
             (strcmp(line, "scd clear") == 0) ||
             (strcmp(line, "dsg clear") == 0))
    {
        if (Bms_PowerService_ClearDischargeFault())
        {
            printf("CLI 放电SCD锁存: 已清除，可重新尝试放电\r\n");
        }
        else
        {
            printf("CLI 放电SCD锁存: 未清除，BQ当前仍处于SCD保护\r\n");
        }
    }
    else if (strcmp(line, "pdsg test") == 0)
    {
        if (App_BatMan_TestPreDischargeOnly())
        {
            printf("CLI PDSG测试: 已写�?x0d并发送ALL_FETS_ON，建议立刻发�?bq 查看FET位\r\n");
        }
        else
        {
            printf("CLI PDSG测试: 写入失败\r\n");
        }
    }
    else if (strcmp(line, "pdsg probe") == 0)
    {
        App_DebugCli_RunPdsgProbe();
    }
    else if (strcmp(line, "pdsg off") == 0)
    {
        if (App_BatMan_AllMainFetsOff())
        {
            printf("CLI PDSG测试: 已恢复全FET关断\r\n");
        }
        else
        {
            printf("CLI PDSG测试: 恢复全关失败\r\n");
        }
    }
    else if (strcmp(line, "scprobe") == 0)
    {
        App_DebugCli_PrintScProbe();
    }
    else if (strcmp(line, "vofa") == 0)
    {
        App_DebugCli_PrintVofaFrame();
    }
    else if (strcmp(line, "vofa on") == 0)
    {
        s_vofa_enabled = true;
        s_vofa_ms = APP_DEBUG_CLI_VOFA_PERIOD_MS;
        printf("CLI vofa:1\r\n");
    }
    else if (strcmp(line, "vofa off") == 0)
    {
        s_vofa_enabled = false;
        s_vofa_ms = 0u;
        printf("CLI vofa:0\r\n");
    }
    else if (strcmp(line, "soccsv") == 0)
    {
        App_DebugCli_PrintSocCsvHeader();
        App_DebugCli_PrintSocCsvFrame();
    }
    else if ((strcmp(line, "csv") == 0) ||
             (strcmp(line, "csv on") == 0) ||
             (strcmp(line, "soccsv on") == 0))
    {
        App_DebugCli_StartSocCsv();
    }
    else if ((strcmp(line, "csv off") == 0) ||
             (strcmp(line, "soccsv off") == 0))
    {
        App_DebugCli_StopSocCsv();
    }
    else if ((strcmp(line, "charge on") == 0) || (strcmp(line, "sc on") == 0))
    {
        Bms_Sc8815Port_RequestCharge(true);
        printf("CLI charge request:1\r\n");
    }
    else if ((strcmp(line, "charge off") == 0) || (strcmp(line, "sc off") == 0))
    {
        Bms_Sc8815Port_RequestCharge(false);
        printf("CLI charge request:0\r\n");
    }
    else
    {
        printf("CLI unknown:%s\r\n", line);
        App_DebugCli_PrintHelp();
    }
}

static void App_DebugCli_PushByte(uint8_t byte)
{
    if ((byte == '\r') || (byte == '\n'))
    {
        if (s_cli_pos > 0u)
        {
            s_cli_line[s_cli_pos] = '\0';
            App_DebugCli_ProcessLine(s_cli_line);
            s_cli_pos = 0u;
        }
        return;
    }

    if ((byte == 0x08u) || (byte == 0x7Fu))
    {
        if (s_cli_pos > 0u)
        {
            s_cli_pos--;
        }
        return;
    }

    if ((byte < 0x20u) || (byte > 0x7Eu))
    {
        return;
    }

    if (s_cli_pos < (APP_DEBUG_CLI_LINE_SIZE - 1u))
    {
        s_cli_line[s_cli_pos++] = (char)byte;
    }
    else
    {
        s_cli_pos = 0u;
        printf("CLI line too long\r\n");
    }
}

void Bms_DebugCli_Init(void)
{
    s_cli_pos = 0u;
    s_rx_head = 0u;
    s_rx_tail = 0u;
    s_vofa_enabled = false;
    s_vofa_ms = 0u;
    s_bq_monitor_enabled = false;
    s_bq_monitor_stop_on_fault = false;
    s_bq_monitor_period_ms = APP_DEBUG_CLI_BQ_PERIOD_MS;
    s_bq_monitor_ms = 0u;
    s_soc_csv_enabled = false;
    s_soc_csv_ms = 0u;
    App_DebugCli_StartRx();
    printf("CLI ready: type help\r\n");
}

void Bms_DebugCli_Task(uint16_t interval_ms)
{
    uint8_t byte;

    while (s_rx_tail != s_rx_head)
    {
        byte = s_rx_buf[s_rx_tail];
        s_rx_tail = (uint8_t)((s_rx_tail + 1u) & APP_DEBUG_CLI_RX_MASK);
        App_DebugCli_PushByte(byte);
    }

    if (s_vofa_enabled)
    {
        s_vofa_ms = (uint16_t)(s_vofa_ms + interval_ms);
        if (s_vofa_ms >= APP_DEBUG_CLI_VOFA_PERIOD_MS)
        {
            s_vofa_ms = 0u;
            App_DebugCli_PrintVofaFrame();
        }
    }

    if (s_bq_monitor_enabled && !s_vofa_enabled)
    {
        s_bq_monitor_ms = (uint16_t)(s_bq_monitor_ms + interval_ms);
        if (s_bq_monitor_ms >= s_bq_monitor_period_ms)
        {
            s_bq_monitor_ms = 0u;
            if (s_bq_monitor_stop_on_fault)
            {
                App_BatMan_PrintFastMonitor();
                if (App_DebugCli_ShouldStopBqFast())
                {
                    App_DebugCli_StopBqFastAndPrintReason();
                }
            }
            else
            {
                App_BatMan_PrintMonitor();
            }
        }
    }

    if (s_soc_csv_enabled && !s_vofa_enabled && !s_bq_monitor_enabled)
    {
        s_soc_csv_ms = (uint16_t)(s_soc_csv_ms + interval_ms);
        if (s_soc_csv_ms >= APP_DEBUG_CLI_SOC_CSV_PERIOD_MS)
        {
            s_soc_csv_ms = 0u;
            App_DebugCli_PrintSocCsvFrame();
        }
    }
}

bool Bms_DebugCli_IsVofaStreaming(void)
{
    return s_vofa_enabled;
}

bool Bms_DebugCli_IsBqMonitoring(void)
{
    return s_bq_monitor_enabled || s_soc_csv_enabled;
}


void App_DebugCli_Init(void)
{
    Bms_DebugCli_Init();
}

void App_DebugCli_Task(uint16_t interval_ms)
{
    Bms_DebugCli_Task(interval_ms);
}

bool App_DebugCli_IsVofaStreaming(void)
{
    return Bms_DebugCli_IsVofaStreaming();
}

bool App_DebugCli_IsBqMonitoring(void)
{
    return Bms_DebugCli_IsBqMonitoring();
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    uint8_t next_head;

    if (huart->Instance != USART1)
    {
        return;
    }

    next_head = (uint8_t)((s_rx_head + 1u) & APP_DEBUG_CLI_RX_MASK);
    if (next_head != s_rx_tail)
    {
        s_rx_buf[s_rx_head] = s_rx_byte;
        s_rx_head = next_head;
    }

    App_DebugCli_StartRx();
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        App_DebugCli_StartRx();
    }
}
