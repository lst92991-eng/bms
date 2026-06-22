#ifndef __COMMON_DEBUG_H__
#define __COMMON_DEBUG_H__0

#include "usart.h"
#include "stdio.h"
#include "string.h"
#include "FreeRTOS.h"
#include "task.h"

#define DEBUG

#ifdef DEBUG
// strrchr(__FILE__, '/' ) + 1:
#define FILE_NAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define FILE_NAME2 (strrchr(FILE_NAME, '\\') ? strrchr(FILE_NAME, '\\') + 1 : FILE_NAME)

// debug_println("hello_%d_%hhu",a,b)  => [main.c:100]  hello_10_20 \r\n

#define debug_println(format, ...) printf("[%20s:%d] " format "\r\n", FILE_NAME2, __LINE__, ##__VA_ARGS__);

#else

#define debug_println(format, ...)

#endif

#endif
