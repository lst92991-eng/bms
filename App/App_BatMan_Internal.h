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
#define APP_BATMAN_BALANCE_PERIOD_MS            (10000u)

extern bool s_comm_fault;
extern bool s_cells_sample_valid;
extern bool s_current_sample_valid;
extern bool s_temp_cell_sample_valid;

uint16_t App_BatMan_ReadU16Le(const uint8_t data[2]);
void App_BatMan_WriteU16Le(uint16_t value, uint8_t data[2]);

void App_BatMan_ResetSampleState(void);
void App_BatMan_ResetEstimatorState(void);
void App_BatMan_ResetDebugState(void);

bool App_BatMan_ConfigBq(void);
Int_BQ76952_StatusTypeDef App_BatMan_KeepMainFetsOff(void);
void App_BatMan_ClearStartupAlarms(void);

void App_BatMan_Sample(void);

void App_BatMan_InitAlgorithms(void);
void App_BatMan_UpdateSoc(uint32_t interval_ms);
void App_BatMan_UpdateHealth(uint32_t interval_ms);
void App_BatMan_UpdateBalance(uint32_t interval_ms);

void App_BatMan_ShowIicStatus(bool ok);
void App_BatMan_ShowPowerConfig(bool ok, uint16_t power_config);
void App_BatMan_UpdateRuntimeOledStatus(void);
void App_BatMan_UpdateDebugOutput(uint32_t interval_ms);

void App_BatMan_PrintDmWrite8Fail(uint16_t address);
void App_BatMan_PrintDmWrite16Fail(uint16_t address);
void App_BatMan_PrintBqResetFail(Int_BQ76952_StatusTypeDef ret);
void App_BatMan_PrintBqDeviceFail(Int_BQ76952_StatusTypeDef ret);
void App_BatMan_PrintBqOkDev(uint16_t device_number);
void App_BatMan_PrintBqCfgEnterFail(void);
void App_BatMan_PrintBqCfgWriteFail(void);
void App_BatMan_PrintBqCfgExitFail(void);
void App_BatMan_PrintBqPowerConfig(uint16_t power_config);
void App_BatMan_PrintBqFetOffFail(void);
void App_BatMan_PrintInitOk(void);

#endif /* APP_BATMAN_INTERNAL_H */
