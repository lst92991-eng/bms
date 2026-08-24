#ifndef INT_LOG_H
#define INT_LOG_H

#include <stdint.h>

#include "stm32g0xx_hal.h"

typedef struct
{
    uint32_t accepted_bytes;
    uint32_t dropped_bytes;
    uint32_t busy_count;
    uint32_t error_count;
} Int_LogStatsTypeDef;

/** @brief 绑定 USART1 并启动非阻塞日志发送。必须在 MX_USART1_UART_Init 后调用。 */
void Int_Log_Init(void);

/**
 * @brief 尝试把完整数据块写入 TX 环形缓冲。
 * @return 实际接收字节数；空间不足时整块丢弃并累计统计。
 */
uint32_t Int_Log_TryWrite(const uint8_t *data, uint32_t length);
void Int_Log_TryWriteString(const char *text);

/**
 * @brief 在固定栈缓冲内格式化并整块写入非阻塞日志队列。
 * @note 超长文本会截断并统计为 dropped_bytes，不使用 FILE 或动态内存。
 *       仅允许整数/字符串格式，禁止 %f/%e/%g/%a 浮点占位符。
 */
#if (defined(__GNUC__) || defined(__clang__)) && !defined(__CC_ARM)
int Int_Log_Printf(const char *format, ...) __attribute__((format(printf, 1, 2)));
#else
int Int_Log_Printf(const char *format, ...);
#endif
void Int_Log_GetStats(Int_LogStatsTypeDef *stats);

/** @brief 供工程态 UART ErrorCallback 转交 TX 错误，不能在此重复定义该回调。 */
void Int_Log_OnUartError(UART_HandleTypeDef *uart);

#endif /* INT_LOG_H */
