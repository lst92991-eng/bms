#include "Int_Fault.h"

#include <stddef.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#include "Int_SC8815.h"
#include "stm32g0xx_hal.h"

#define INT_FAULT_RECORD_MAGIC UINT32_C(0x424D5346)
#define INT_FAULT_RECORD_XOR UINT32_C(0xA55A3CC3)

enum
{
    INT_FAULT_RECORD_FRAME_VALID = (1u << 15),
    INT_FAULT_RECORD_CODE_MASK = 0x7FFFu,
    INT_FAULT_EXCEPTION_FRAME_WORDS = 8u,
    INT_FAULT_STACKED_LR_INDEX = 5u,
    INT_FAULT_STACKED_PC_INDEX = 6u,
    INT_FAULT_PROJECT_SRAM_BYTES = (144u * 1024u)
};

static volatile bool s_fault_latched = false;
static volatile Int_FaultCodeTypeDef s_fault_code = INT_FAULT_NONE;
static volatile uint32_t s_fault_context = 0u;
static volatile uint32_t s_fault_tick = 0u;
static uint32_t s_reset_cause = 0u;
static bool s_previous_valid = false;
static uint16_t s_previous_sequence = 0u;
static Int_FaultCodeTypeDef s_previous_code = INT_FAULT_NONE;
static uint32_t s_previous_context = 0u;
static uint32_t s_previous_tick = 0u;
static bool s_previous_frame_valid = false;
static uint32_t s_previous_pc = 0u;
static uint32_t s_previous_lr = 0u;
static bool s_current_frame_valid = false;
static uint32_t s_current_pc = 0u;
static uint32_t s_current_lr = 0u;

static uint32_t Int_Fault_RecordChecksum(uint32_t word1, uint32_t payload0, uint32_t payload1)
{
    return INT_FAULT_RECORD_MAGIC ^ word1 ^ payload0 ^ payload1 ^ INT_FAULT_RECORD_XOR;
}

static uint32_t Int_Fault_Lock(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    return primask;
}

static void Int_Fault_Unlock(uint32_t primask)
{
    if (primask == 0u)
    {
        __enable_irq();
    }
}

static void Int_Fault_EnableBackupAccess(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_RTCAPB_CLK_ENABLE();
    SET_BIT(PWR->CR1, PWR_CR1_DBP);
}

static void Int_Fault_WriteRecord(Int_FaultCodeTypeDef code,
                                  uint32_t context,
                                  uint32_t tick,
                                  bool frame_valid,
                                  uint32_t pc,
                                  uint32_t lr)
{
    uint16_t sequence = s_previous_valid ? (uint16_t)(s_previous_sequence + 1u) : 1u;
    uint32_t word1 = ((uint32_t)sequence << 16u) |
                     (frame_valid ? INT_FAULT_RECORD_FRAME_VALID : 0u) |
                     ((uint32_t)code & INT_FAULT_RECORD_CODE_MASK);
    uint32_t payload0 = frame_valid ? pc : context;
    uint32_t payload1 = frame_valid ? lr : tick;

    Int_Fault_EnableBackupAccess();
    TAMP->BKP0R = INT_FAULT_RECORD_MAGIC;
    TAMP->BKP1R = word1;
    TAMP->BKP2R = payload0;
    TAMP->BKP3R = payload1;
    TAMP->BKP4R = Int_Fault_RecordChecksum(word1, payload0, payload1);
    __DSB();
}

static bool Int_Fault_IsExceptionFrameReadable(const uint32_t *exception_frame)
{
    uintptr_t address = (uintptr_t)exception_frame;
    uintptr_t last_address = address + (INT_FAULT_EXCEPTION_FRAME_WORDS * sizeof(uint32_t));
    uintptr_t sram_end = (uintptr_t)SRAM_BASE + INT_FAULT_PROJECT_SRAM_BYTES;

    return ((address & (sizeof(uint32_t) - 1u)) == 0u) && (address >= (uintptr_t)SRAM_BASE) &&
           (last_address >= address) && (last_address <= sram_end);
}

static void Int_Fault_Latch(
    Int_FaultCodeTypeDef code, uint32_t context, bool frame_valid, uint32_t pc, uint32_t lr)
{
    uint32_t primask = Int_Fault_Lock();

    if (!s_fault_latched)
    {
        uint32_t tick = HAL_GetTick();

        s_fault_code = code;
        s_fault_context = context;
        s_fault_tick = tick;
        s_current_frame_valid = frame_valid;
        s_current_pc = pc;
        s_current_lr = lr;
        s_fault_latched = true;
        Int_Fault_WriteRecord(code, context, tick, frame_valid, pc, lr);
    }
    Int_Fault_Unlock(primask);
}

static uint16_t Int_Fault_HashFile(const char *file)
{
    uint32_t hash = 2166136261u;

    if (file == NULL)
    {
        return 0u;
    }
    while (*file != '\0')
    {
        hash ^= (uint8_t)*file;
        hash *= 16777619u;
        file++;
    }
    return (uint16_t)(hash ^ (hash >> 16u));
}

void Int_Fault_Init(void)
{
    uint32_t word1;
    uint32_t payload0;
    uint32_t payload1;
    uint32_t checksum;

    s_reset_cause = RCC->CSR;
    SET_BIT(RCC->CSR, RCC_CSR_RMVF);

    Int_Fault_EnableBackupAccess();
    word1 = TAMP->BKP1R;
    payload0 = TAMP->BKP2R;
    payload1 = TAMP->BKP3R;
    checksum = TAMP->BKP4R;
    s_previous_valid = (TAMP->BKP0R == INT_FAULT_RECORD_MAGIC) &&
                       (checksum == Int_Fault_RecordChecksum(word1, payload0, payload1));
    if (s_previous_valid)
    {
        s_previous_sequence = (uint16_t)(word1 >> 16u);
        s_previous_code = (Int_FaultCodeTypeDef)(word1 & INT_FAULT_RECORD_CODE_MASK);
        s_previous_frame_valid = (word1 & INT_FAULT_RECORD_FRAME_VALID) != 0u;
        if (s_previous_frame_valid)
        {
            s_previous_pc = payload0;
            s_previous_lr = payload1;
        }
        else
        {
            s_previous_context = payload0;
            s_previous_tick = payload1;
        }
    }
}

void Int_Fault_Trip(Int_FaultCodeTypeDef code, uint32_t context)
{
    /* 故障路径第一动作必须是硬件停机，不能等待队列、日志或任务调度。 */
    Int_SC8815_ForceStandby();
    Int_Fault_Latch(code, context, false, 0u, 0u);
}

void Int_Fault_Panic(Int_FaultCodeTypeDef code, uint32_t context)
{
    Int_Fault_Trip(code, context);
    __disable_irq();
    __DSB();
    NVIC_SystemReset();
    while (1)
    {
    }
}

void Int_Fault_HandleHardFault(const uint32_t *exception_frame, uint32_t exc_return)
{
    bool frame_valid;
    uint32_t pc = 0u;
    uint32_t lr = 0u;

    /* 裸入口只选栈指针；进入 C 后第一条硬件动作仍是直接拉高 PSTOP、进入 standby。 */
    Int_SC8815_ForceStandby();
    frame_valid = Int_Fault_IsExceptionFrameReadable(exception_frame);
    if (frame_valid)
    {
        lr = exception_frame[INT_FAULT_STACKED_LR_INDEX];
        pc = exception_frame[INT_FAULT_STACKED_PC_INDEX];
    }
    Int_Fault_Latch(INT_FAULT_HARDFAULT, exc_return, frame_valid, pc, lr);
    __disable_irq();
    __DSB();
    NVIC_SystemReset();
    while (1)
    {
    }
}

bool Int_Fault_IsLatched(void)
{
    return s_fault_latched;
}

Int_FaultCodeTypeDef Int_Fault_GetLatchedCode(void)
{
    return s_fault_code;
}

void Int_Fault_GetSnapshot(Int_FaultSnapshotTypeDef *snapshot)
{
    uint32_t primask;

    if (snapshot == NULL)
    {
        return;
    }

    primask = Int_Fault_Lock();
    snapshot->reset_cause = s_reset_cause;
    snapshot->previous_record_valid = s_previous_valid;
    snapshot->previous_sequence = s_previous_sequence;
    snapshot->previous_code = s_previous_code;
    snapshot->previous_context = s_previous_context;
    snapshot->previous_tick = s_previous_tick;
    snapshot->previous_frame_valid = s_previous_frame_valid;
    snapshot->previous_pc = s_previous_pc;
    snapshot->previous_lr = s_previous_lr;
    snapshot->current_fault_latched = s_fault_latched;
    snapshot->current_code = s_fault_code;
    snapshot->current_context = s_fault_context;
    snapshot->current_tick = s_fault_tick;
    snapshot->current_frame_valid = s_current_frame_valid;
    snapshot->current_pc = s_current_pc;
    snapshot->current_lr = s_current_lr;
    Int_Fault_Unlock(primask);
}

void Int_Fault_ConfigAssert(const char *file, uint32_t line)
{
    uint32_t context = ((uint32_t)Int_Fault_HashFile(file) << 16u) | (line & 0xFFFFu);

    Int_Fault_Panic(INT_FAULT_CONFIG_ASSERT, context);
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)
{
    (void)task_name;
    Int_Fault_Panic(INT_FAULT_STACK_OVERFLOW, (uint32_t)(uintptr_t)task);
}

void vApplicationMallocFailedHook(void)
{
    Int_Fault_Panic(INT_FAULT_MALLOC_FAILED, 0u);
}
