/*
 * FreeRTOS Kernel V10.3.1
 * Cortex-M0/M0+ GCC port layer for this STM32G0B1 project.
 */

#include "FreeRTOS.h"
#include "task.h"

#define portNVIC_SYSTICK_CTRL_REG           (*(volatile uint32_t *)0xe000e010)
#define portNVIC_SYSTICK_LOAD_REG           (*(volatile uint32_t *)0xe000e014)
#define portNVIC_SYSTICK_CURRENT_VALUE_REG  (*(volatile uint32_t *)0xe000e018)
#define portNVIC_INT_CTRL_REG               (*(volatile uint32_t *)0xe000ed04)
#define portNVIC_SYSPRI2_REG                (*(volatile uint32_t *)0xe000ed20)
#define portNVIC_SYSTICK_CLK_BIT            (1UL << 2UL)
#define portNVIC_SYSTICK_INT_BIT            (1UL << 1UL)
#define portNVIC_SYSTICK_ENABLE_BIT         (1UL << 0UL)
#define portNVIC_SYSTICK_COUNT_FLAG_BIT     (1UL << 16UL)
#define portNVIC_PENDSVSET_BIT              (1UL << 28UL)
#define portMIN_INTERRUPT_PRIORITY          (255UL)
#define portNVIC_PENDSV_PRI                 (portMIN_INTERRUPT_PRIORITY << 16UL)
#define portNVIC_SYSTICK_PRI                (portMIN_INTERRUPT_PRIORITY << 24UL)
#define portINITIAL_XPSR                    (0x01000000UL)
#define portMAX_24_BIT_NUMBER               (0xffffffUL)
#define portSY_FULL_READ_WRITE              (15)

#ifndef portMISSED_COUNTS_FACTOR
#define portMISSED_COUNTS_FACTOR            (45UL)
#endif

#ifndef configOVERRIDE_DEFAULT_TICK_CONFIGURATION
#define configOVERRIDE_DEFAULT_TICK_CONFIGURATION 0
#endif

static UBaseType_t uxCriticalNesting = 0xaaaaaaaaUL;

#if (configUSE_TICKLESS_IDLE == 1)
static uint32_t ulTimerCountsForOneTick = 0;
static uint32_t xMaximumPossibleSuppressedTicks = 0;
static uint32_t ulStoppedTimerCompensation = 0;
#endif

void vPortSetupTimerInterrupt(void);
void xPortPendSVHandler(void);
void xPortSysTickHandler(void);
void vPortSVCHandler(void);

static void prvPortStartFirstTask(void);
static void prvTaskExitError(void);

StackType_t *pxPortInitialiseStack(StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters)
{
    pxTopOfStack--;
    *pxTopOfStack = portINITIAL_XPSR;
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)pxCode;
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)prvTaskExitError;
    pxTopOfStack -= 5;
    *pxTopOfStack = (StackType_t)pvParameters;
    pxTopOfStack -= 8;

    return pxTopOfStack;
}

static void prvTaskExitError(void)
{
    configASSERT(uxCriticalNesting == ~0UL);
    portDISABLE_INTERRUPTS();
    for (;;) {
    }
}

void vPortSVCHandler(void)
{
}

__attribute__((naked)) static void prvPortStartFirstTask(void)
{
    __asm volatile
    (
        ".syntax unified              \n"
        "ldr r3, =pxCurrentTCB        \n"
        "ldr r1, [r3]                \n"
        "ldr r0, [r1]                \n"
        "movs r2, #32                \n"
        "adds r0, r0, r2             \n"
        "msr psp, r0                 \n"
        "movs r0, #2                 \n"
        "msr CONTROL, r0             \n"
        "isb                         \n"
        "pop {r0-r5}                 \n"
        "mov lr, r5                  \n"
        "pop {r3}                    \n"
        "pop {r2}                    \n"
        "cpsie i                     \n"
        "bx r3                       \n"
    );
}

BaseType_t xPortStartScheduler(void)
{
    portNVIC_SYSPRI2_REG |= portNVIC_PENDSV_PRI;
    portNVIC_SYSPRI2_REG |= portNVIC_SYSTICK_PRI;

    vPortSetupTimerInterrupt();
    uxCriticalNesting = 0;
    prvPortStartFirstTask();

    return 0;
}

void vPortEndScheduler(void)
{
    configASSERT(uxCriticalNesting == 1000UL);
}

void vPortYield(void)
{
    portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
    __asm volatile ("dsb %0" :: "i"(portSY_FULL_READ_WRITE) : "memory");
    __asm volatile ("isb %0" :: "i"(portSY_FULL_READ_WRITE) : "memory");
}

void vPortEnterCritical(void)
{
    portDISABLE_INTERRUPTS();
    uxCriticalNesting++;
    __asm volatile ("dsb %0" :: "i"(portSY_FULL_READ_WRITE) : "memory");
    __asm volatile ("isb %0" :: "i"(portSY_FULL_READ_WRITE) : "memory");
}

void vPortExitCritical(void)
{
    configASSERT(uxCriticalNesting);
    uxCriticalNesting--;
    if (uxCriticalNesting == 0) {
        portENABLE_INTERRUPTS();
    }
}

__attribute__((naked)) uint32_t ulSetInterruptMaskFromISR(void)
{
    __asm volatile
    (
        ".syntax unified  \n"
        "mrs r0, PRIMASK \n"
        "cpsid i         \n"
        "bx lr           \n"
    );
}

__attribute__((naked)) void vClearInterruptMaskFromISR(uint32_t ulMask)
{
    (void)ulMask;
    __asm volatile
    (
        ".syntax unified  \n"
        "msr PRIMASK, r0 \n"
        "bx lr           \n"
    );
}

__attribute__((naked)) void xPortPendSVHandler(void)
{
    __asm volatile
    (
        ".syntax unified              \n"
        "mrs r0, psp                 \n"
        "ldr r3, =pxCurrentTCB       \n"
        "ldr r2, [r3]                \n"
        "movs r1, #32                \n"
        "subs r0, r0, r1             \n"
        "str r0, [r2]                \n"
        "stmia r0!, {r4-r7}          \n"
        "mov r4, r8                  \n"
        "mov r5, r9                  \n"
        "mov r6, r10                 \n"
        "mov r7, r11                 \n"
        "stmia r0!, {r4-r7}          \n"
        "push {r3, r14}              \n"
        "cpsid i                     \n"
        "bl vTaskSwitchContext       \n"
        "cpsie i                     \n"
        "pop {r2, r3}                \n"
        "ldr r1, [r2]                \n"
        "ldr r0, [r1]                \n"
        "movs r1, #16                \n"
        "adds r0, r0, r1             \n"
        "ldmia r0!, {r4-r7}          \n"
        "mov r8, r4                  \n"
        "mov r9, r5                  \n"
        "mov r10, r6                 \n"
        "mov r11, r7                 \n"
        "msr psp, r0                 \n"
        "movs r1, #32                \n"
        "subs r0, r0, r1             \n"
        "ldmia r0!, {r4-r7}          \n"
        "bx r3                       \n"
    );
}

void xPortSysTickHandler(void)
{
    uint32_t ulPreviousMask;

    ulPreviousMask = portSET_INTERRUPT_MASK_FROM_ISR();
    {
        if (xTaskIncrementTick() != pdFALSE) {
            portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
        }
    }
    portCLEAR_INTERRUPT_MASK_FROM_ISR(ulPreviousMask);
}

#if (configOVERRIDE_DEFAULT_TICK_CONFIGURATION == 0)
__attribute__((weak)) void vPortSetupTimerInterrupt(void)
{
#if (configUSE_TICKLESS_IDLE == 1)
    ulTimerCountsForOneTick = (configCPU_CLOCK_HZ / configTICK_RATE_HZ);
    xMaximumPossibleSuppressedTicks = portMAX_24_BIT_NUMBER / ulTimerCountsForOneTick;
    ulStoppedTimerCompensation = portMISSED_COUNTS_FACTOR;
#endif

    portNVIC_SYSTICK_CTRL_REG = 0UL;
    portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;
    portNVIC_SYSTICK_LOAD_REG = (configCPU_CLOCK_HZ / configTICK_RATE_HZ) - 1UL;
    portNVIC_SYSTICK_CTRL_REG = portNVIC_SYSTICK_CLK_BIT | portNVIC_SYSTICK_INT_BIT | portNVIC_SYSTICK_ENABLE_BIT;
}
#endif

#if (configUSE_TICKLESS_IDLE == 1)
__attribute__((weak)) void vPortSuppressTicksAndSleep(TickType_t xExpectedIdleTime)
{
    (void)xExpectedIdleTime;
}
#endif
