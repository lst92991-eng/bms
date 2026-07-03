/*
 * FreeRTOS Kernel V10.3.1
 * Cortex-M0/M0+ GCC port layer for this STM32G0B1 project.
 */

#ifndef PORTMACRO_H
#define PORTMACRO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define portCHAR        char
#define portFLOAT       float
#define portDOUBLE      double
#define portLONG        long
#define portSHORT       short
#define portSTACK_TYPE  uint32_t
#define portBASE_TYPE   long

typedef portSTACK_TYPE StackType_t;
typedef long BaseType_t;
typedef unsigned long UBaseType_t;

#if (configUSE_16_BIT_TICKS == 1)
typedef uint16_t TickType_t;
#define portMAX_DELAY (TickType_t)0xffff
#else
typedef uint32_t TickType_t;
#define portMAX_DELAY (TickType_t)0xffffffffUL
#define portTICK_TYPE_IS_ATOMIC 1
#endif

#define portSTACK_GROWTH      (-1)
#define portTICK_PERIOD_MS    ((TickType_t)1000 / configTICK_RATE_HZ)
#define portBYTE_ALIGNMENT    8

extern void vPortYield(void);
#define portNVIC_INT_CTRL_REG     (*(volatile uint32_t *)0xe000ed04)
#define portNVIC_PENDSVSET_BIT    (1UL << 28UL)
#define portYIELD()               vPortYield()
#define portEND_SWITCHING_ISR(xSwitchRequired) do { if (xSwitchRequired) { portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT; } } while (0)
#define portYIELD_FROM_ISR(x)     portEND_SWITCHING_ISR(x)

extern void vPortEnterCritical(void);
extern void vPortExitCritical(void);
extern uint32_t ulSetInterruptMaskFromISR(void);
extern void vClearInterruptMaskFromISR(uint32_t ulMask);

#define portSET_INTERRUPT_MASK_FROM_ISR()     ulSetInterruptMaskFromISR()
#define portCLEAR_INTERRUPT_MASK_FROM_ISR(x)  vClearInterruptMaskFromISR(x)
#define portDISABLE_INTERRUPTS()              __asm volatile ("cpsid i" ::: "memory")
#define portENABLE_INTERRUPTS()               __asm volatile ("cpsie i" ::: "memory")
#define portENTER_CRITICAL()                  vPortEnterCritical()
#define portEXIT_CRITICAL()                   vPortExitCritical()

#ifndef portSUPPRESS_TICKS_AND_SLEEP
extern void vPortSuppressTicksAndSleep(TickType_t xExpectedIdleTime);
#define portSUPPRESS_TICKS_AND_SLEEP(xExpectedIdleTime) vPortSuppressTicksAndSleep(xExpectedIdleTime)
#endif

#define portTASK_FUNCTION_PROTO(vFunction, pvParameters) void vFunction(void *pvParameters)
#define portTASK_FUNCTION(vFunction, pvParameters) void vFunction(void *pvParameters)

#define portNOP() __asm volatile ("nop")

#ifdef __cplusplus
}
#endif

#endif /* PORTMACRO_H */
