#include "Bms_ChargeService.h"

#include <stddef.h>
#include <string.h>

void Bms_ChargeService_Reset(Bms_ChargeService_StateTypeDef *state)
{
    if (state == NULL)
    {
        return;
    }

    state->full_latched = false;
    state->stop_reason = BMS_CHARGE_STOP_NONE;
}

bool Bms_ChargeService_Evaluate(Bms_ChargeService_StateTypeDef *state,
                                const Bms_ContextTypeDef *ctx,
                                const Bms_ChargeService_ConfigTypeDef *config,
                                bool cell_sample_ok,
                                Bms_ChargeService_EvaluationTypeDef *evaluation)
{
    if (evaluation == NULL)
    {
        return false;
    }

    (void)memset(evaluation, 0, sizeof(*evaluation));
    if ((state == NULL) || (ctx == NULL) || (config == NULL))
    {
        return false;
    }

    if (cell_sample_ok)
    {
        /*
         * 顶端均衡阶段先按较低电压停充，让均衡有时间拉低最高串；
         * 真正满电停充使用独立回差，避免在 4.20V 附近反复补电。
         */
        if (ctx->pack.max_mv >= config->cell_full_stop_mv)
        {
            state->full_latched = true;
            state->stop_reason = BMS_CHARGE_STOP_FULL;
        }
        else if ((state->stop_reason == BMS_CHARGE_STOP_FULL) &&
                 (ctx->pack.max_mv <= config->cell_full_resume_mv))
        {
            state->full_latched = false;
            state->stop_reason = BMS_CHARGE_STOP_NONE;
        }
        else if ((state->stop_reason == BMS_CHARGE_STOP_TOP_BALANCE) &&
                 ((ctx->pack.max_mv <= config->top_balance_resume_mv) ||
                  (ctx->pack.delta_mv <= config->top_balance_stop_delta_mv)))
        {
            state->full_latched = false;
            state->stop_reason = BMS_CHARGE_STOP_NONE;
        }
        else if ((state->stop_reason == BMS_CHARGE_STOP_NONE) &&
                 (ctx->pack.max_mv >= config->top_balance_stop_mv) &&
                 (ctx->pack.delta_mv >= config->top_balance_start_delta_mv))
        {
            state->full_latched = true;
            state->stop_reason = BMS_CHARGE_STOP_TOP_BALANCE;
        }
    }
    else
    {
        Bms_ChargeService_Reset(state);
    }

    evaluation->temp_ok =
        ((ctx->pack.temp_cell_c >= config->temp_min_c) &&
         (ctx->pack.temp_cell_c <= config->temp_max_c));
    evaluation->voltage_ok = !state->full_latched;
    evaluation->stop_reason = state->stop_reason;

    return true;
}
