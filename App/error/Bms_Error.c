#include "Bms_Error.h"

#include <stddef.h>

void Bms_Error_Clear(Bms_ErrorRecordTypeDef *record)
{
    if (record == NULL)
    {
        return;
    }

    record->code = BMS_ERROR_NONE;
    record->detail = 0u;
    record->latched = false;
}

void Bms_Error_Set(Bms_ErrorRecordTypeDef *record,
                   Bms_ErrorCodeTypeDef code,
                   uint32_t detail,
                   bool latched)
{
    if (record == NULL)
    {
        return;
    }

    record->code = code;
    record->detail = detail;
    record->latched = latched;
}
