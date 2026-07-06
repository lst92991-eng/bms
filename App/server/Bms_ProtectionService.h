#ifndef BMS_PROTECTION_SERVICE_H
#define BMS_PROTECTION_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "Bms_Model.h"

#define BMS_PROTECTION_MONITOR_REASON_MAX 28u

typedef enum
{
    BMS_PROTECTION_REASON_NULL_CONTEXT = 0,
    BMS_PROTECTION_REASON_COMM_FAULT,
    BMS_PROTECTION_REASON_CELLS_INVALID,
    BMS_PROTECTION_REASON_CURRENT_INVALID,
    BMS_PROTECTION_REASON_TEMP_INVALID,
    BMS_PROTECTION_REASON_APP_FAULT,
    BMS_PROTECTION_REASON_SCD,
    BMS_PROTECTION_REASON_OCD2,
    BMS_PROTECTION_REASON_OCD1,
    BMS_PROTECTION_REASON_CUV,
    BMS_PROTECTION_REASON_COV,
    BMS_PROTECTION_REASON_OTF,
    BMS_PROTECTION_REASON_OTD,
    BMS_PROTECTION_REASON_UTD,
    BMS_PROTECTION_REASON_OCD3,
    BMS_PROTECTION_REASON_SCD_LATCH,
    BMS_PROTECTION_REASON_OCD_LATCH,
    BMS_PROTECTION_REASON_ALARM_SSA,
    BMS_PROTECTION_REASON_ALARM_SSBC,
    BMS_PROTECTION_REASON_ALARM_PF,
    BMS_PROTECTION_REASON_ALARM_SF_ALERT,
    BMS_PROTECTION_REASON_ALARM_PF_ALERT,
    BMS_PROTECTION_REASON_XDSG,
    BMS_PROTECTION_REASON_SHUTV,
    BMS_PROTECTION_REASON_FUSE,
    BMS_PROTECTION_REASON_DSG_OFF,
    BMS_PROTECTION_REASON_PF
} Bms_ProtectionReasonCodeTypeDef;

typedef struct
{
    Bms_ProtectionReasonCodeTypeDef code;
    const char *text;
} Bms_ProtectionReasonTypeDef;

bool Bms_ProtectionService_IsMonitorFaultActive(const Bms_ContextTypeDef *ctx);
bool Bms_ProtectionService_IsDischargeShortCircuit(const Bms_ContextTypeDef *ctx);
size_t Bms_ProtectionService_CollectMonitorFaultReasons(const Bms_ContextTypeDef *ctx,
                                                        Bms_ProtectionReasonTypeDef *reasons,
                                                        size_t max_count);

#endif /* BMS_PROTECTION_SERVICE_H */
