#include "Bms_Config.h"

#include "App_BatMan.h"

static const Bms_ChargeService_ConfigTypeDef s_charge_config =
{
    4200u,
    4180u,
    4100u,
    4070u,
    40u,
    20u,
    0,
    45
};

static const Bms_DischargeService_ConfigTypeDef s_discharge_config =
{
    APP_BATMAN_CELL_VALID_MIN_MV,
    APP_BATMAN_CELL_VALID_MAX_MV,
    3000u,
    3200u,
    12000,
    0,
    -20,
    60
};

static const Bms_PreDischargeService_ConfigTypeDef s_predischarge_config =
{
    5000u
};

static const Bms_Config_PowerTypeDef s_power_config =
{
    12000u,
    60000u,
    5000u,
    12000u,
    false,
    false
};

const Bms_ChargeService_ConfigTypeDef *Bms_Config_GetChargeServiceConfig(void)
{
    return &s_charge_config;
}

const Bms_DischargeService_ConfigTypeDef *Bms_Config_GetDischargeServiceConfig(void)
{
    return &s_discharge_config;
}

const Bms_PreDischargeService_ConfigTypeDef *Bms_Config_GetPreDischargeServiceConfig(void)
{
    return &s_predischarge_config;
}

const Bms_Config_PowerTypeDef *Bms_Config_GetPowerConfig(void)
{
    return &s_power_config;
}
