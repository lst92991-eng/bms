#ifndef BMS_DISCHARGE_SERVICE_H
#define BMS_DISCHARGE_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "Bms_Model.h"

typedef struct
{
    uint16_t cell_valid_min_mv;
    uint16_t cell_valid_max_mv;
    uint16_t cell_low_mv;
    uint16_t cell_recover_mv;
    int32_t current_limit_ma;
    int32_t over_current_margin_ma;
    int16_t temp_min_c;
    int16_t temp_max_c;
} Bms_DischargeService_ConfigTypeDef;

typedef struct
{
    bool cell_sample_ok;
    bool temp_ok;
    bool over_current;
    bool low_voltage;
    bool recover_blocked;
} Bms_DischargeService_EvaluationTypeDef;

bool Bms_DischargeService_Evaluate(const Bms_ContextTypeDef *ctx,
                                   const Bms_DischargeService_ConfigTypeDef *config,
                                   Bms_DischargeService_EvaluationTypeDef *evaluation);

#endif /* BMS_DISCHARGE_SERVICE_H */
