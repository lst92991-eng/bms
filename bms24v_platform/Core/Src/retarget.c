#include <stdio.h>

#include "usart.h"

#define RETARGET_UART_TIMEOUT_MS 1000u

#if defined(__CC_ARM)
#pragma import(__use_no_semihosting)

struct __FILE
{
  int handle;
};

FILE __stdout;
FILE __stdin;

void _sys_exit(int status)
{
  (void)status;
  while (1) {
  }
}

void _ttywrch(int ch)
{
  uint8_t byte = (uint8_t)ch;

  (void)HAL_UART_Transmit(&huart1, &byte, 1u, RETARGET_UART_TIMEOUT_MS);
}
#endif

int fputc(int ch, FILE *file)
{
  uint8_t byte = (uint8_t)ch;

  (void)file;
  (void)HAL_UART_Transmit(&huart1, &byte, 1u, RETARGET_UART_TIMEOUT_MS);

  return ch;
}

#if defined(__GNUC__) && !defined(__CC_ARM)
int _write(int file, char *ptr, int len)
{
  (void)file;
  (void)HAL_UART_Transmit(&huart1,
                          (uint8_t *)ptr,
                          (uint16_t)len,
                          RETARGET_UART_TIMEOUT_MS);

  return len;
}
#endif
