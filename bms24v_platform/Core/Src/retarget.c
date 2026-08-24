#include <stdio.h>

#include "Int_Fault.h"
#include "Int_Log.h"

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
  Int_Fault_Panic(INT_FAULT_RUNTIME_EXIT, (uint32_t)status);
}

void _ttywrch(int ch)
{
  uint8_t byte = (uint8_t)ch;

  (void)Int_Log_TryWrite(&byte, 1u);
}
#endif

int fputc(int ch, FILE *file)
{
  uint8_t byte = (uint8_t)ch;

  (void)file;
  (void)Int_Log_TryWrite(&byte, 1u);

  return ch;
}

#if defined(__GNUC__) && !defined(__CC_ARM)
int _write(int file, char *ptr, int len)
{
  (void)file;
  if ((ptr != NULL) && (len > 0))
  {
    (void)Int_Log_TryWrite((const uint8_t *)ptr, (uint32_t)len);
  }

  /* libc 永不因诊断链路背压阻塞；丢弃量由 Int_Log 统计。 */
  return len;
}
#endif
