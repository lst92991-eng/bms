#include "Bms_DischargeService.h"

#include <stddef.h>
#include <string.h>

bool Bms_DischargeService_Evaluate(const Bms_ContextTypeDef *ctx,
                                   const Bms_DischargeService_ConfigTypeDef *config,
                                   Bms_DischargeService_EvaluationTypeDef *evaluation)
{
    int32_t current_limit_ma;

    if (evaluation == NULL)
    {
        return false;
    }

    (void)memset(evaluation, 0, sizeof(*evaluation));
    if ((ctx == NULL) || (config == NULL))
    {
        return false;
    }

    current_limit_ma = config->current_limit_ma + config->over_current_margin_ma;
    if (current_limit_ma < 0)
    {
        current_limit_ma = 0;
    }

    evaluation->cell_sample_ok =
        ((ctx->pack.min_mv >= config->cell_valid_min_mv) &&
         (ctx->pack.max_mv <= config->cell_valid_max_mv));
    evaluation->temp_ok =
        ((ctx->pack.temp_cell_c >= config->temp_min_c) &&
         (ctx->pack.temp_cell_c <= config->temp_max_c));
    evaluation->over_current = (ctx->pack.current_ma < -current_limit_ma);
    evaluation->low_voltage = (ctx->pack.min_mv <= config->cell_low_mv);
    evaluation->recover_blocked = (ctx->pack.min_rc_mv < config->cell_recover_mv);

    return true;
}
