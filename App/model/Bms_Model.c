#include "Bms_Model.h"

#include <string.h>

static Bms_ContextTypeDef s_bms_context;

Bms_ContextTypeDef *Bms_Model_GetMutableContext(void)
{
    return &s_bms_context;
}

const Bms_ContextTypeDef *Bms_Model_GetContext(void)
{
    return &s_bms_context;
}

void Bms_Model_Init(Bms_ContextTypeDef *ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    (void)memset(ctx, 0, sizeof(*ctx));
}

void Bms_Model_SetPackSnapshot(Bms_ContextTypeDef *ctx,
                               const Bms_PackSnapshotTypeDef *snapshot)
{
    if ((ctx == NULL) || (snapshot == NULL))
    {
        return;
    }

    ctx->pack = *snapshot;
    ctx->update_seq++;
}

void Bms_Model_SetProtectionSnapshot(Bms_ContextTypeDef *ctx,
                                     const Bms_ProtectionSnapshotTypeDef *snapshot)
{
    if ((ctx == NULL) || (snapshot == NULL))
    {
        return;
    }

    ctx->protection = *snapshot;
    ctx->update_seq++;
}

void Bms_Model_SetEstimateSnapshot(Bms_ContextTypeDef *ctx,
                                   const Bms_EstimateSnapshotTypeDef *snapshot)
{
    if ((ctx == NULL) || (snapshot == NULL))
    {
        return;
    }

    ctx->estimate = *snapshot;
    ctx->update_seq++;
}

void Bms_Model_SetServiceSnapshot(Bms_ContextTypeDef *ctx,
                                  const Bms_ServiceSnapshotTypeDef *snapshot)
{
    if ((ctx == NULL) || (snapshot == NULL))
    {
        return;
    }

    ctx->service = *snapshot;
    ctx->update_seq++;
}
