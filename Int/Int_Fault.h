#ifndef INT_FAULT_H
#define INT_FAULT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    INT_FAULT_NONE = 0,
    INT_FAULT_BQ_ALERT = 1,
    INT_FAULT_TASK_DEADLINE = 2,
    INT_FAULT_STACK_MARGIN = 3,
    INT_FAULT_CRITICAL_INIT = 4,
    INT_FAULT_TASK_CREATE = 5,
    INT_FAULT_WATCHDOG_INIT = 6,
    INT_FAULT_STACK_OVERFLOW = 7,
    INT_FAULT_MALLOC_FAILED = 8,
    INT_FAULT_CONFIG_ASSERT = 9,
    INT_FAULT_ERROR_HANDLER = 10,
    INT_FAULT_NMI = 11,
    INT_FAULT_HARDFAULT = 12,
    INT_FAULT_SCHEDULER_RETURN = 13,
    INT_FAULT_RUNTIME_EXIT = 14
} Int_FaultCodeTypeDef;

typedef struct
{
    uint32_t reset_cause;
    bool previous_record_valid;
    uint16_t previous_sequence;
    Int_FaultCodeTypeDef previous_code;
    uint32_t previous_context;
    uint32_t previous_tick;
    bool previous_frame_valid;
    uint32_t previous_pc;
    uint32_t previous_lr;
    bool current_fault_latched;
    Int_FaultCodeTypeDef current_code;
    uint32_t current_context;
    uint32_t current_tick;
    bool current_frame_valid;
    uint32_t current_pc;
    uint32_t current_lr;
} Int_FaultSnapshotTypeDef;

/** @brief 捕获并清除 RCC 复位标志，同时读取上次备份域故障记录。 */
void Int_Fault_Init(void);

/**
 * @brief 安全锁存故障；第一条硬件动作始终是强制 SC8815 待机。
 *
 * 可从任务或 ISR 调用。首次故障写入 TAMP BKP0R~BKP4R，后续故障不覆盖根因。
 * HardFault 记录用 BKP2R/BKP3R 保存异常栈 PC/LR；普通故障保存 context/tick。
 */
void Int_Fault_Trip(Int_FaultCodeTypeDef code, uint32_t context);

/** @brief 锁存故障后立即请求系统复位；函数不会返回。 */
void Int_Fault_Panic(Int_FaultCodeTypeDef code, uint32_t context);

/** @brief HardFault 裸入口的 C 处理器；exception_frame 指向 8 字异常栈帧。 */
void Int_Fault_HandleHardFault(const uint32_t *exception_frame, uint32_t exc_return);

bool Int_Fault_IsLatched(void);
Int_FaultCodeTypeDef Int_Fault_GetLatchedCode(void);
void Int_Fault_GetSnapshot(Int_FaultSnapshotTypeDef *snapshot);

/** @brief FreeRTOS configASSERT 的统一入口。 */
void Int_Fault_ConfigAssert(const char *file, uint32_t line);

#endif /* INT_FAULT_H */
