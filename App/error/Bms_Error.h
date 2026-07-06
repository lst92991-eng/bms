#ifndef BMS_ERROR_H
#define BMS_ERROR_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    BMS_ERROR_NONE = 0,
    BMS_ERROR_COMM,
    BMS_ERROR_SAMPLE,
    BMS_ERROR_PROTECTION,
    BMS_ERROR_POWER_PORT,
    BMS_ERROR_CONFIG
} Bms_ErrorCodeTypeDef;

typedef struct
{
    Bms_ErrorCodeTypeDef code;
    uint32_t detail;
    bool latched;
} Bms_ErrorRecordTypeDef;

void Bms_Error_Clear(Bms_ErrorRecordTypeDef *record);
void Bms_Error_Set(Bms_ErrorRecordTypeDef *record,
                   Bms_ErrorCodeTypeDef code,
                   uint32_t detail,
                   bool latched);

#endif /* BMS_ERROR_H */
