#include "App_DebugCli.h"

#if APP_DEBUG_CLI_ENGINEERING_ENABLED

#include <stdbool.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

/*
 * AI bring-up/debug 专用串口入口。
 *
 * 本模块仅允许读取状态、输出 CSV，以及向 Power supervisor 提交受控请求。
 * 生产业务不能依赖这里的命令才能运行；删除步骤见 docs/rules/debug_cli_removal.md。
 */
#include "App_BatMan.h"
#include "App_Buzzer.h"
#include "App_Power.h"
#include "App_SC8815.h"
#include "Int_Log.h"
#include "Int_SC8815.h"
#include "main.h"
#include "usart.h"

enum
{
    APP_DEBUG_CLI_LINE_SIZE = 80u,
    APP_DEBUG_CLI_RX_SIZE = 128u,
    APP_DEBUG_CLI_RX_MASK = APP_DEBUG_CLI_RX_SIZE - 1u,
    APP_DEBUG_CLI_CSV_PERIOD_MS = 1000u,
    APP_DEBUG_CLI_BQ_PERIOD_MS = 1000u,
    APP_DEBUG_CLI_BQ_FAST_PERIOD_MS = 200u,
    APP_DEBUG_CLI_PACK_RAW_MAX_MV = 5000u,
    APP_DEBUG_CLI_UNLOCK_TIMEOUT_MS = 60000u
};

static char s_cli_line[APP_DEBUG_CLI_LINE_SIZE];
static uint8_t s_cli_pos;
static uint8_t s_rx_byte;
static uint8_t s_rx_buf[APP_DEBUG_CLI_RX_SIZE];
static volatile uint8_t s_rx_head;
static volatile uint8_t s_rx_tail;
static bool s_csv_enabled;
static bool s_csv_header_pending;
static uint16_t s_csv_ms;
static bool s_bq_monitor_enabled;
static bool s_bq_monitor_stop_on_fault;
static uint16_t s_bq_monitor_period_ms;
static uint16_t s_bq_monitor_ms;
static bool s_cli_unlocked;
static uint32_t s_cli_unlock_ms;

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
    Int_Log_Printf(
        "CLI help: unlock <token> help ping lanhua diag bq bq on bqfast on bq off bq shutdown "
        "power fault clear sc charge on charge off csv csv on csv off\r\n");
}

static void App_DebugCli_PrintSignedMilli(int32_t milli_value)
{
    uint32_t abs_value;

    if (milli_value < 0)
    {
        Int_Log_Printf("-");
        /* 先向零偏移再取反，避免 INT32_MIN 的有符号溢出。 */
        abs_value = (uint32_t)(-(milli_value + 1)) + 1u;
    }
    else
    {
        abs_value = (uint32_t)milli_value;
    }

    Int_Log_Printf(
        "%lu.%03lu", (unsigned long)(abs_value / 1000u), (unsigned long)(abs_value % 1000u));
}

static void App_DebugCli_PrintUnsignedMilli(uint32_t milli_value)
{
    Int_Log_Printf(
        "%lu.%03lu", (unsigned long)(milli_value / 1000u), (unsigned long)(milli_value % 1000u));
}

static void App_DebugCli_PrintCsvFrame(void)
{
    uint16_t csv_cell_mv[APP_BATMAN_CELL_COUNT];
    uint32_t csv_pack_mv;
    int32_t csv_current_ma;
    uint32_t frame_time_ms;

    /*
     * BatMan 任务会成组更新 PACK、电流和 6 节电压。先复制完整快照，
     * 避免任务切换发生在多次格式化输出之间时，同一 CSV 行混入前后两帧。
     */
    taskENTER_CRITICAL();
    csv_current_ma = current_ma;
    csv_pack_mv = pack_mv;
    for (uint8_t i = 0u; i < APP_BATMAN_CELL_COUNT; i++)
    {
        csv_cell_mv[i] = cell_mv[i];
    }
    taskEXIT_CRITICAL();
    frame_time_ms = HAL_GetTick();

    if ((csv_pack_mv > 0u) && (csv_pack_mv < APP_DEBUG_CLI_PACK_RAW_MAX_MV))
    {
        csv_pack_mv *= APP_BATMAN_STACK_RAW_TO_MV;
    }

    /* current/voltage/time 分别按 A/V/s 输出，均保留 3 位小数。 */
    App_DebugCli_PrintSignedMilli(csv_current_ma);
    Int_Log_Printf(",");
    App_DebugCli_PrintUnsignedMilli(csv_pack_mv);
    for (uint8_t i = 0u; i < APP_BATMAN_CELL_COUNT; i++)
    {
        Int_Log_Printf(",");
        App_DebugCli_PrintUnsignedMilli(csv_cell_mv[i]);
    }
    Int_Log_Printf(",");
    App_DebugCli_PrintUnsignedMilli(frame_time_ms);
    Int_Log_Printf("\r\n");
}

static void App_DebugCli_PrintSc(void)
{
    Int_Log_Printf("---------- SC8815详细 ----------\r\n");
    Int_Log_Printf(
        "CLI sc comm:%u ac:%u fault:%u charging:%u vbus:%lu vbat:%lu input_lim:%lu bus:%02x "
        "swap:%u\r\n",
        App_SC8815_IsCommOk() ? 1u : 0u,
        App_SC8815_IsAcOk() ? 1u : 0u,
        App_SC8815_HasFault() ? 1u : 0u,
        App_SC8815_IsCharging() ? 1u : 0u,
        (unsigned long)App_SC8815_GetVbusMv(),
        (unsigned long)App_SC8815_GetVbatMv(),
        (unsigned long)App_SC8815_GetInputLimitMa(),
        (unsigned int)Int_SC8815_GetBusLevels(),
        Int_SC8815_IsIicLineSwapped() ? 1u : 0u);
}

static bool App_DebugCli_ShouldStopBqFast(void)
{
    if (App_BatMan_IsMonitorFaultActive())
    {
        return true;
    }

    if ((App_Power_GetState() == APP_POWER_STATE_LOW) ||
        (App_Power_GetState() == APP_POWER_STATE_FAULT) || !App_Power_IsDischargeAllowed())
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

    Int_Log_Printf("---------- BQFAST自动停表 ----------\r\n");
    Int_Log_Printf("BQFAST: 检测到放电异常，已停止连续输出，下面是停表瞬间原因。\r\n");
    App_Power_PrintStopReason();
    App_BatMan_PrintMonitorStopReason();
    App_Power_PrintSnapshot();
    App_BatMan_PrintSnapshot();
    Int_Log_Printf("BQFAST: 处理后可发送 fault clear，再发送 bqfast on 重新监视。\r\n");
}

static void App_DebugCli_ProcessLine(char *line)
{
    line = App_DebugCli_Trim(line);
    App_DebugCli_Normalize(line);

    if (line[0] == '\0')
    {
        return;
    }

    if (!APP_DEBUG_CLI_PHYSICAL_ENABLE_ACTIVE())
    {
        s_cli_unlocked = false;
        Int_Log_Printf("CLI locked: physical enable inactive\r\n");
        return;
    }
    if (strcmp(line, "unlock 8815-eng") == 0)
    {
        s_cli_unlocked = true;
        s_cli_unlock_ms = 0u;
        Int_Log_Printf("CLI unlocked: timeout %lu ms\r\n",
                       (unsigned long)APP_DEBUG_CLI_UNLOCK_TIMEOUT_MS);
        return;
    }
    if (!s_cli_unlocked)
    {
        Int_Log_Printf("CLI locked: use unlock token\r\n");
        return;
    }
    s_cli_unlock_ms = 0u;

    if (strcmp(line, "help") == 0)
    {
        App_DebugCli_PrintHelp();
    }
    else if (strcmp(line, "ping") == 0)
    {
        Int_Log_Printf("CLI pong tick:%lu\r\n", (unsigned long)HAL_GetTick());
    }
    else if (strcmp(line, "lanhua") == 0)
    {
        App_Buzzer_PlayLanhua();
        Int_Log_Printf("CLI lanhua: playing %u ms\r\n",
                       (unsigned int)APP_BUZZER_LANHUA_DURATION_MS);
    }
    else if (strcmp(line, "sc") == 0)
    {
        App_DebugCli_PrintSc();
    }
    else if (strcmp(line, "diag") == 0)
    {
        App_Power_PrintSnapshot();
        App_DebugCli_PrintSc();
        App_BatMan_PrintSnapshot();
    }
    else if (strcmp(line, "bq") == 0)
    {
        App_BatMan_PrintSnapshot();
    }
    else if (strcmp(line, "bq on") == 0)
    {
        s_csv_enabled = false;
        s_csv_header_pending = false;
        s_csv_ms = 0u;
        s_bq_monitor_enabled = true;
        s_bq_monitor_stop_on_fault = false;
        s_bq_monitor_period_ms = APP_DEBUG_CLI_BQ_PERIOD_MS;
        s_bq_monitor_ms = APP_DEBUG_CLI_BQ_PERIOD_MS;
        Int_Log_Printf("CLI BQ连续监视: 开启 周期:%u ms\r\n", (unsigned int)s_bq_monitor_period_ms);
    }
    else if ((strcmp(line, "bqfast on") == 0) || (strcmp(line, "bq fast") == 0))
    {
        s_csv_enabled = false;
        s_csv_header_pending = false;
        s_csv_ms = 0u;
        s_bq_monitor_enabled = true;
        s_bq_monitor_stop_on_fault = true;
        s_bq_monitor_period_ms = APP_DEBUG_CLI_BQ_FAST_PERIOD_MS;
        s_bq_monitor_ms = APP_DEBUG_CLI_BQ_FAST_PERIOD_MS;
        Int_Log_Printf("CLI bqfast:1 周期:%u ms 异常自动停表:1\r\n",
                       (unsigned int)s_bq_monitor_period_ms);
    }
    else if (strcmp(line, "bq off") == 0)
    {
        s_bq_monitor_enabled = false;
        s_bq_monitor_stop_on_fault = false;
        s_bq_monitor_ms = 0u;
        Int_Log_Printf("CLI BQ连续监视: 关闭\r\n");
    }
    else if (strcmp(line, "bq shutdown") == 0)
    {
        s_bq_monitor_enabled = false;
        s_bq_monitor_stop_on_fault = false;
        s_bq_monitor_ms = 0u;
        s_csv_enabled = false;
        s_csv_header_pending = false;
        s_csv_ms = 0u;
        if (App_Power_RequestBqShutdown())
        {
            Int_Log_Printf("CLI BQ shutdown: 请求已提交，将由电源任务安全执行\r\n");
        }
        else
        {
            Int_Log_Printf("CLI BQ shutdown: 失败，请发送 bq/power 查看状态\r\n");
        }
    }
    else if (strcmp(line, "power") == 0)
    {
        App_Power_PrintSnapshot();
    }
    else if ((strcmp(line, "fault clear") == 0) || (strcmp(line, "scd clear") == 0) ||
             (strcmp(line, "dsg clear") == 0))
    {
        if (App_Power_ClearDischargeFault())
        {
            Int_Log_Printf("CLI 放电SCD锁存: 已清除，可重新尝试放电\r\n");
        }
        else
        {
            Int_Log_Printf("CLI 放电SCD锁存: 未清除，BQ当前仍处于SCD保护\r\n");
        }
    }
    else if (strcmp(line, "pdsg test") == 0)
    {
        Int_Log_Printf("CLI denied: PDSG actuator bypass removed\r\n");
    }
    else if (strcmp(line, "pdsg probe") == 0)
    {
        Int_Log_Printf("CLI denied: direct BQ probe removed\r\n");
    }
    else if (strcmp(line, "pdsg off") == 0)
    {
        Int_Log_Printf("CLI denied: use Power supervisor\r\n");
    }
    else if (strcmp(line, "scprobe") == 0)
    {
        Int_Log_Printf("CLI denied: runtime I2C line probing removed\r\n");
    }
    else if (strcmp(line, "csv") == 0)
    {
        App_DebugCli_PrintCsvFrame();
    }
    else if (strcmp(line, "csv on") == 0)
    {
        s_bq_monitor_enabled = false;
        s_bq_monitor_stop_on_fault = false;
        s_bq_monitor_ms = 0u;
        s_csv_enabled = true;
        s_csv_header_pending = true;
        s_csv_ms = APP_DEBUG_CLI_CSV_PERIOD_MS;
    }
    else if (strcmp(line, "csv off") == 0)
    {
        s_csv_enabled = false;
        s_csv_header_pending = false;
        s_csv_ms = 0u;
        Int_Log_Printf("CLI csv:0\r\n");
    }
    else if ((strcmp(line, "charge on") == 0) || (strcmp(line, "sc on") == 0))
    {
        (void)App_Power_RequestChargeInhibit(false);
        Int_Log_Printf("CLI Power charge inhibit:0 (policy still applies)\r\n");
    }
    else if ((strcmp(line, "charge off") == 0) || (strcmp(line, "sc off") == 0))
    {
        (void)App_Power_RequestChargeInhibit(true);
        Int_Log_Printf("CLI Power charge inhibit:1\r\n");
    }
    else
    {
        Int_Log_Printf("CLI unknown:%s\r\n", line);
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
        Int_Log_Printf("CLI line too long\r\n");
    }
}

void App_DebugCli_Init(void)
{
    s_cli_pos = 0u;
    s_rx_head = 0u;
    s_rx_tail = 0u;
    s_csv_enabled = false;
    s_csv_header_pending = false;
    s_csv_ms = 0u;
    s_bq_monitor_enabled = false;
    s_bq_monitor_stop_on_fault = false;
    s_bq_monitor_period_ms = APP_DEBUG_CLI_BQ_PERIOD_MS;
    s_bq_monitor_ms = 0u;
    s_cli_unlocked = false;
    s_cli_unlock_ms = 0u;
    if (!APP_DEBUG_CLI_PHYSICAL_ENABLE_ACTIVE())
    {
        return;
    }
    App_DebugCli_StartRx();
    Int_Log_Printf("Engineering CLI locked\r\n");
}

void App_DebugCli_Task(uint16_t interval_ms)
{
    uint8_t byte;

    if (!APP_DEBUG_CLI_PHYSICAL_ENABLE_ACTIVE())
    {
        s_cli_unlocked = false;
        s_csv_enabled = false;
        s_bq_monitor_enabled = false;
        return;
    }

    if (s_cli_unlocked)
    {
        s_cli_unlock_ms += interval_ms;
        if (s_cli_unlock_ms >= APP_DEBUG_CLI_UNLOCK_TIMEOUT_MS)
        {
            s_cli_unlocked = false;
            s_csv_enabled = false;
            s_bq_monitor_enabled = false;
            Int_Log_Printf("Engineering CLI lock timeout\r\n");
        }
    }

    while (s_rx_tail != s_rx_head)
    {
        byte = s_rx_buf[s_rx_tail];
        s_rx_tail = (uint8_t)((s_rx_tail + 1u) & APP_DEBUG_CLI_RX_MASK);
        App_DebugCli_PushByte(byte);
    }

    if (s_csv_enabled && s_csv_header_pending)
    {
        s_csv_header_pending = false;
        Int_Log_Printf(
            "current_a,pack_v,cell1_v,cell2_v,cell3_v,cell4_v,cell5_v,cell6_v,frame_time_s\r\n");
    }

    if (s_csv_enabled)
    {
        s_csv_ms = (uint16_t)(s_csv_ms + interval_ms);
        if (s_csv_ms >= APP_DEBUG_CLI_CSV_PERIOD_MS)
        {
            s_csv_ms = 0u;
            App_DebugCli_PrintCsvFrame();
        }
    }

    if (s_bq_monitor_enabled && !s_csv_enabled)
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
}

bool App_DebugCli_IsStreaming(void)
{
    return s_csv_enabled || s_bq_monitor_enabled;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    uint8_t next_head;

    if (!APP_DEBUG_CLI_PHYSICAL_ENABLE_ACTIVE() || (huart->Instance != USART1))
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

void App_DebugCli_OnUartError(void)
{
    if (APP_DEBUG_CLI_PHYSICAL_ENABLE_ACTIVE())
    {
        App_DebugCli_StartRx();
    }
}

#else

void App_DebugCli_Init(void)
{
}

void App_DebugCli_Task(uint16_t interval_ms)
{
    (void)interval_ms;
}

bool App_DebugCli_IsStreaming(void)
{
    return false;
}

void App_DebugCli_OnUartError(void)
{
}

#endif /* APP_DEBUG_CLI_ENGINEERING_ENABLED */
