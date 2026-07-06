#ifndef BMS_PREDISCHARGE_SERVICE_H
#define BMS_PREDISCHARGE_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint16_t duration_ms;
} Bms_PreDischargeService_ConfigTypeDef;

typedef struct
{
    uint16_t elapsed_ms;
    bool done_reported;
} Bms_PreDischargeService_StateTypeDef;

void Bms_PreDischargeService_Reset(Bms_PreDischargeService_StateTypeDef *state);
bool Bms_PreDischargeService_ShouldRun(const Bms_PreDischargeService_StateTypeDef *state,
                                       const Bms_PreDischargeService_ConfigTypeDef *config,
                                       bool discharge_allowed);
bool Bms_PreDischargeService_Update(Bms_PreDischargeService_StateTypeDef *state,
                                    const Bms_PreDischargeService_ConfigTypeDef *config,
                                    uint32_t interval_ms);

#endif /* BMS_PREDISCHARGE_SERVICE_H */
