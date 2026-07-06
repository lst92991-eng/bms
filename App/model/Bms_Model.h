#ifndef BMS_MODEL_H
#define BMS_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#define BMS_MODEL_CELL_COUNT 6u

typedef struct
{
    uint16_t mv[BMS_MODEL_CELL_COUNT];
    uint32_t stack_mv;
    uint32_t pack_mv;
    int32_t current_ma;
    uint16_t min_mv;
    uint16_t avg_mv;
    uint16_t max_mv;
    uint16_t delta_mv;
    uint16_t min_rc_mv;
    uint16_t avg_rc_mv;
    int16_t rc_ohmic_mv;
    int16_t rc_polar_mv;
    int16_t rc_total_mv;
    int16_t temp_cell_c;
    int16_t temp_fet_c;
    int16_t temp_ic_c;
    bool cells_valid;
    bool current_valid;
    bool temp_valid;
    bool comm_fault;
} Bms_PackSnapshotTypeDef;

typedef struct
{
    uint16_t alarm_status;
    uint16_t alarm_raw;
    uint16_t battery_status;
    uint16_t manufacturing_status;
    uint8_t fet_status;
    uint8_t fet_request;
    uint8_t safety_alert_a;
    uint8_t safety_alert_b;
    uint8_t safety_alert_c;
    uint8_t safety_status_a;
    uint8_t safety_status_b;
    uint8_t safety_status_c;
    uint8_t pf_status_a;
    uint8_t pf_status_b;
    uint8_t pf_status_c;
    uint8_t pf_status_d;
    bool fault_active;
} Bms_ProtectionSnapshotTypeDef;

typedef struct
{
    float soc_percent;
    float display_soc_percent;
    uint8_t soc_confidence_percent;
    float soc_residual_percent;
    float soc_kalman_gain;
    float soc_p;
    float active_capacity_mah;
    uint32_t charge_throughput_mah;
    uint32_t discharge_throughput_mah;
    uint32_t cycle_count;
    uint8_t soh_percent;
    uint8_t soh_confidence_percent;
} Bms_EstimateSnapshotTypeDef;

typedef struct
{
    bool charge_allowed;
    bool discharge_allowed;
    bool predischarge_active;
    uint16_t balance_mask;
} Bms_ServiceSnapshotTypeDef;

typedef struct
{
    Bms_PackSnapshotTypeDef pack;
    Bms_ProtectionSnapshotTypeDef protection;
    Bms_EstimateSnapshotTypeDef estimate;
    Bms_ServiceSnapshotTypeDef service;
    uint32_t update_seq;
} Bms_ContextTypeDef;

Bms_ContextTypeDef *Bms_Model_GetMutableContext(void);
const Bms_ContextTypeDef *Bms_Model_GetContext(void);

void Bms_Model_Init(Bms_ContextTypeDef *ctx);
void Bms_Model_SetPackSnapshot(Bms_ContextTypeDef *ctx,
                               const Bms_PackSnapshotTypeDef *snapshot);
void Bms_Model_SetProtectionSnapshot(Bms_ContextTypeDef *ctx,
                                     const Bms_ProtectionSnapshotTypeDef *snapshot);
void Bms_Model_SetEstimateSnapshot(Bms_ContextTypeDef *ctx,
                                   const Bms_EstimateSnapshotTypeDef *snapshot);
void Bms_Model_SetServiceSnapshot(Bms_ContextTypeDef *ctx,
                                  const Bms_ServiceSnapshotTypeDef *snapshot);

#endif /* BMS_MODEL_H */
