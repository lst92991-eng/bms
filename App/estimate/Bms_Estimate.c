#include "Bms_Estimate.h"

#include "App_BatMan_Internal.h"

void Bms_Estimate_Reset(void)
{
    App_BatMan_ResetEstimatorState();
}

void Bms_Estimate_Init(void)
{
    App_BatMan_InitAlgorithms();
}

void Bms_Estimate_UpdateFirstFrame(void)
{
    App_BatMan_UpdateRcModel(0u);
    App_BatMan_UpdateHealth(0u);
    App_BatMan_UpdateSoc(0u);
    App_BatMan_UpdateBalance(APP_BATMAN_BALANCE_PERIOD_MS);
}

void Bms_Estimate_Task(uint32_t interval_ms)
{
    App_BatMan_UpdateRcModel(interval_ms);
    App_BatMan_UpdateHealth(interval_ms);
    App_BatMan_UpdateSoc(interval_ms);
    App_BatMan_UpdateBalance(interval_ms);
}
