#ifndef BMS_SNAPSHOT_H
#define BMS_SNAPSHOT_H

/**
 * @file bms_snapshot.h
 * @brief BMS V2 不可变测量快照及数据质量模型。
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BMS_CELL_COUNT             (6u)
#define BMS_CELL_TEMP_SENSOR_COUNT (2u)

typedef enum
{
    BMS_DATA_QUALITY_UNAVAILABLE = 0,
    BMS_DATA_QUALITY_VALID,
    BMS_DATA_QUALITY_STALE,
    BMS_DATA_QUALITY_OUT_OF_RANGE,
    BMS_DATA_QUALITY_IO_ERROR
} BmsData_Quality;

typedef struct
{
    BmsData_Quality quality;
    uint32_t timestamp_ms;
    uint32_t source_flags;
} BmsData_Meta;

typedef struct
{
    BmsData_Meta meta;
    uint16_t cell_mv[BMS_CELL_COUNT];
} BmsCellMeasurements;

typedef struct
{
    BmsData_Meta voltage_meta;
    BmsData_Meta current_meta;
    uint32_t stack_mv;
    uint32_t pack_mv;
    int32_t current_ma;
} BmsPackMeasurements;

typedef struct
{
    BmsData_Meta meta;
    int16_t temperature_deg_c;
} BmsTemperatureMeasurement;

typedef struct
{
    BmsTemperatureMeasurement cell_sensor[BMS_CELL_TEMP_SENSOR_COUNT];
    BmsTemperatureMeasurement ic;
} BmsThermalMeasurements;

typedef struct
{
    BmsData_Meta meta;
    bool is_online;
    bool is_config_valid;
    bool is_safety_active;
    bool is_permanent_failure_active;
    uint8_t fet_status_raw;
} BmsBqStatus;

typedef struct
{
    BmsData_Meta meta;
    bool is_communication_ok;
    bool is_ac_present;
    bool is_vbus_short;
    bool is_over_temperature;
    bool is_standby;
    uint32_t vbus_mv;
    uint32_t vbat_mv;
} BmsScStatus;

typedef struct
{
    uint32_t sequence;
    uint32_t published_at_ms;
    BmsCellMeasurements cells;
    BmsPackMeasurements pack;
    BmsThermalMeasurements thermal;
    BmsBqStatus bq;
    BmsScStatus sc;
} BmsSnapshot;

#ifdef __cplusplus
}
#endif

#endif /* BMS_SNAPSHOT_H */
