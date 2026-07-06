#include "Bms_PreDischargeService.h"

#include <stddef.h>

void Bms_PreDischargeService_Reset(Bms_PreDischargeService_StateTypeDef *state)
{
    if (state == NULL)
    {
        return;
    }

    state->elapsed_ms = 0u;
    state->done_reported = false;
}

bool Bms_PreDischargeService_ShouldRun(const Bms_PreDischargeService_StateTypeDef *state,
                                       const Bms_PreDischargeService_ConfigTypeDef *config,
                                       bool discharge_allowed)
{
    if ((state == NULL) || (config == NULL))
    {
        return false;
    }

    return (discharge_allowed && (state->elapsed_ms < config->duration_ms));
}

bool Bms_PreDischargeService_Update(Bms_PreDischargeService_StateTypeDef *state,
                                    const Bms_PreDischargeService_ConfigTypeDef *config,
                                    uint32_t interval_ms)
{
    uint32_t elapsed_ms;

    if ((state == NULL) || (config == NULL))
    {
        return false;
    }

    elapsed_ms = (uint32_t)state->elapsed_ms + interval_ms;
    if (elapsed_ms >= config->duration_ms)
    {
        state->elapsed_ms = config->duration_ms;
        if (!state->done_reported)
        {
            state->done_reported = true;
            return true;
        }
        return false;
    }

    state->elapsed_ms = (uint16_t)elapsed_ms;
    return false;
}
