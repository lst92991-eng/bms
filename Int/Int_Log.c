#include "Int_Log.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>

#include "Com_Format.h"
#include "usart.h"

enum
{
    INT_LOG_TX_BUFFER_SIZE = 1024u,
    INT_LOG_TX_BUFFER_MASK = INT_LOG_TX_BUFFER_SIZE - 1u,
    INT_LOG_FORMAT_BUFFER_SIZE = 384u
};

static uint8_t s_tx_buffer[INT_LOG_TX_BUFFER_SIZE];
static volatile uint16_t s_tx_head = 0u;
static volatile uint16_t s_tx_tail = 0u;
static volatile uint16_t s_tx_active_length = 0u;
static volatile bool s_tx_active = false;
static volatile bool s_log_ready = false;
static volatile Int_LogStatsTypeDef s_stats = {0u, 0u, 0u, 0u};

static uint32_t Int_Log_Lock(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    return primask;
}

static void Int_Log_Unlock(uint32_t primask)
{
    if (primask == 0u)
    {
        __enable_irq();
    }
}

static uint16_t Int_Log_FreeBytes(void)
{
    return (uint16_t)((s_tx_tail - s_tx_head - 1u) & INT_LOG_TX_BUFFER_MASK);
}

static void Int_Log_StartLocked(void)
{
    uint16_t contiguous_length;
    HAL_StatusTypeDef status;

    if (!s_log_ready || s_tx_active || (s_tx_head == s_tx_tail))
    {
        return;
    }

    if (s_tx_head > s_tx_tail)
    {
        contiguous_length = (uint16_t)(s_tx_head - s_tx_tail);
    }
    else
    {
        contiguous_length = (uint16_t)(INT_LOG_TX_BUFFER_SIZE - s_tx_tail);
    }

    s_tx_active = true;
    s_tx_active_length = contiguous_length;
    status = HAL_UART_Transmit_IT(&huart1, &s_tx_buffer[s_tx_tail], contiguous_length);
    if (status != HAL_OK)
    {
        s_tx_active = false;
        s_tx_active_length = 0u;
        if (status == HAL_BUSY)
        {
            s_stats.busy_count++;
        }
        else
        {
            s_stats.error_count++;
        }
    }
}

void Int_Log_Init(void)
{
    uint32_t primask = Int_Log_Lock();

    s_tx_head = 0u;
    s_tx_tail = 0u;
    s_tx_active_length = 0u;
    s_tx_active = false;
    s_stats.accepted_bytes = 0u;
    s_stats.dropped_bytes = 0u;
    s_stats.busy_count = 0u;
    s_stats.error_count = 0u;
    s_log_ready = true;
    Int_Log_Unlock(primask);
}

uint32_t Int_Log_TryWrite(const uint8_t *data, uint32_t length)
{
    uint32_t primask;
    uint32_t index;

    if ((data == NULL) || (length == 0u))
    {
        return 0u;
    }

    primask = Int_Log_Lock();
    if (!s_log_ready || (length > Int_Log_FreeBytes()))
    {
        s_stats.dropped_bytes += length;
        Int_Log_StartLocked();
        Int_Log_Unlock(primask);
        return 0u;
    }

    for (index = 0u; index < length; index++)
    {
        s_tx_buffer[s_tx_head] = data[index];
        s_tx_head = (uint16_t)((s_tx_head + 1u) & INT_LOG_TX_BUFFER_MASK);
    }
    s_stats.accepted_bytes += length;
    Int_Log_StartLocked();
    Int_Log_Unlock(primask);
    return length;
}

void Int_Log_TryWriteString(const char *text)
{
    if (text != NULL)
    {
        (void)Int_Log_TryWrite((const uint8_t *)text, (uint32_t)strlen(text));
    }
}

int Int_Log_Printf(const char *format, ...)
{
    char buffer[INT_LOG_FORMAT_BUFFER_SIZE];
    va_list arguments;
    int formatted_length;
    uint32_t write_length;

    if (format == NULL)
    {
        return -1;
    }

    va_start(arguments, format);
    formatted_length = Com_FormatV(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    if (formatted_length < 0)
    {
        uint32_t primask = Int_Log_Lock();

        s_stats.error_count++;
        Int_Log_Unlock(primask);
        return formatted_length;
    }

    write_length = (uint32_t)formatted_length;
    if (write_length >= sizeof(buffer))
    {
        uint32_t primask = Int_Log_Lock();

        s_stats.dropped_bytes += write_length - (sizeof(buffer) - 1u);
        Int_Log_Unlock(primask);
        write_length = sizeof(buffer) - 1u;
    }
    (void)Int_Log_TryWrite((const uint8_t *)buffer, write_length);
    return formatted_length;
}

void Int_Log_GetStats(Int_LogStatsTypeDef *stats)
{
    uint32_t primask;

    if (stats == NULL)
    {
        return;
    }
    primask = Int_Log_Lock();
    stats->accepted_bytes = s_stats.accepted_bytes;
    stats->dropped_bytes = s_stats.dropped_bytes;
    stats->busy_count = s_stats.busy_count;
    stats->error_count = s_stats.error_count;
    Int_Log_Unlock(primask);
}

void Int_Log_OnUartError(UART_HandleTypeDef *uart)
{
    uint32_t primask;

    if ((uart == NULL) || (uart->Instance != USART1))
    {
        return;
    }

    primask = Int_Log_Lock();
    if (s_tx_active)
    {
        s_stats.dropped_bytes += s_tx_active_length;
        s_tx_tail = (uint16_t)((s_tx_tail + s_tx_active_length) & INT_LOG_TX_BUFFER_MASK);
        s_tx_active = false;
        s_tx_active_length = 0u;
    }
    s_stats.error_count++;
    Int_Log_StartLocked();
    Int_Log_Unlock(primask);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart)
{
    uint32_t primask;

    if ((uart == NULL) || (uart->Instance != USART1))
    {
        return;
    }

    primask = Int_Log_Lock();
    if (s_tx_active)
    {
        s_tx_tail = (uint16_t)((s_tx_tail + s_tx_active_length) & INT_LOG_TX_BUFFER_MASK);
        s_tx_active_length = 0u;
        s_tx_active = false;
    }
    Int_Log_StartLocked();
    Int_Log_Unlock(primask);
}
