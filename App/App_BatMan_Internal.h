#ifndef APP_BATMAN_INTERNAL_H
#define APP_BATMAN_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "App_BatMan.h"
#include "Int_BQ76952.h"

/*
 * 均衡周期被 App_BatMan.c 用作初始化后强制刷新参数，同时被 Estimator
 * 模块用于周期判断，因此只放在内部头文件中共享。
 */
enum
{
    APP_BATMAN_BALANCE_PERIOD_MS = 10000u
};

extern bool s_comm_fault;
extern bool s_cells_sample_valid;
extern bool s_current_sample_valid;
extern bool s_temp_cell_sample_valid;
extern uint32_t s_fault_flags;
extern bool s_soc_full_anchor_used;
extern bool s_soc_empty_anchor_used;
extern bool s_soh_capacity_updated;

uint16_t App_BatMan_ReadU16Le(const uint8_t data[2]);
void App_BatMan_WriteU16Le(uint16_t value, uint8_t data[2]);
void App_BatMan_WriteU32Le(uint32_t value, uint8_t data[4]);

void App_BatMan_ResetSampleState(void);
void App_BatMan_ResetEstimatorState(void);
void App_BatMan_ResetDebugState(void);

bool App_BatMan_ConfigBq(void);
bool App_BatMan_VerifyBqConfig(void);
void App_BatMan_ResetConfigState(void);
void App_BatMan_LatchConfigInvalid(void);
void App_BatMan_MarkConfigRecoveryRequired(void);
bool App_BatMan_PreResetAllFetsOff(void);
bool App_BatMan_EnableFetControlSafely(void);
bool App_BatMan_ObserveFetStatus(uint8_t observed_status);
Int_BQ76952_StatusTypeDef App_BatMan_KeepMainFetsOff(void);
Int_BQ76952_StatusTypeDef App_BatMan_ClearStartupAlarms(void);

bool App_BatMan_Sample(void);

void App_BatMan_InitAlgorithms(void);
void App_BatMan_UpdateRcModel(uint32_t interval_ms);
void App_BatMan_UpdateSoc(uint32_t interval_ms);
void App_BatMan_UpdateHealth(uint32_t interval_ms);
void App_BatMan_UpdateBalance(uint32_t interval_ms);

bool App_BatMan_NvmInit(void);
void App_BatMan_NvmTask(uint32_t interval_ms);
bool App_BatMan_NvmFlush(void);

void App_BatMan_UpdateRuntimeOledStatus(void);
void App_BatMan_UpdateDebugOutput(uint32_t interval_ms);

#endif /* APP_BATMAN_INTERNAL_H */
