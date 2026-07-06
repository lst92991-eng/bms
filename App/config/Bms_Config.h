#ifndef BMS_CONFIG_H
#define BMS_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#include "Bms_ChargeService.h"
#include "Bms_DischargeService.h"
#include "Bms_PreDischargeService.h"

typedef struct
{
    uint32_t sc_input_valid_mv;
    uint32_t bq_wake_timeout_ms;
    uint16_t debug_period_ms;
    uint16_t discharge_current_ma;
    bool buzzer_enable;
    bool charge_only_test_enable;
} Bms_Config_PowerTypeDef;

const Bms_ChargeService_ConfigTypeDef *Bms_Config_GetChargeServiceConfig(void);
const Bms_DischargeService_ConfigTypeDef *Bms_Config_GetDischargeServiceConfig(void);
const Bms_PreDischargeService_ConfigTypeDef *Bms_Config_GetPreDischargeServiceConfig(void);
const Bms_Config_PowerTypeDef *Bms_Config_GetPowerConfig(void);

#endif /* BMS_CONFIG_H */
