#ifndef BMS_CHARGE_SERVICE_H
#define BMS_CHARGE_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "Bms_Model.h"

typedef enum
{
    BMS_CHARGE_STOP_NONE = 0,
    BMS_CHARGE_STOP_FULL,
    BMS_CHARGE_STOP_TOP_BALANCE
} Bms_ChargeService_StopReasonTypeDef;

typedef struct
{
    uint16_t cell_full_stop_mv;
    uint16_t cell_full_resume_mv;
    uint16_t top_balance_stop_mv;
    uint16_t top_balance_resume_mv;
    uint16_t top_balance_start_delta_mv;
    uint16_t top_balance_stop_delta_mv;
    int16_t temp_min_c;
    int16_t temp_max_c;
} Bms_ChargeService_ConfigTypeDef;

typedef struct
{
    bool full_latched;
    Bms_ChargeService_StopReasonTypeDef stop_reason;
} Bms_ChargeService_StateTypeDef;

typedef struct
{
    bool temp_ok;
    bool voltage_ok;
    Bms_ChargeService_StopReasonTypeDef stop_reason;
} Bms_ChargeService_EvaluationTypeDef;

void Bms_ChargeService_Reset(Bms_ChargeService_StateTypeDef *state);
bool Bms_ChargeService_Evaluate(Bms_ChargeService_StateTypeDef *state,
                                const Bms_ContextTypeDef *ctx,
                                const Bms_ChargeService_ConfigTypeDef *config,
                                bool cell_sample_ok,
                                Bms_ChargeService_EvaluationTypeDef *evaluation);

#endif /* BMS_CHARGE_SERVICE_H */
