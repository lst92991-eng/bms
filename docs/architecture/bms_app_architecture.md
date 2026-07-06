# BMS APP Architecture Plan

## Goal

The APP layer will move to a C-style local object-oriented design while keeping
the whole firmware flow procedural and easy to trace.

The first migration rule is behavior preservation: each step must compile and
run before moving more logic.

## Target Folders

| Folder | Responsibility |
| --- | --- |
| `App/core` | Top-level APP orchestration and data-flow pipeline. |
| `App/model` | Battery pack object, snapshots, service commands, and shared context. |
| `App/config` | Business thresholds, calibration values, and strategy tables. |
| `App/estimate` | SOC, SOH, RC model, OCV, capacity learning, and future algorithms. |
| `App/server` | Charge, discharge, pre-discharge, balancing, protection, and other services. |
| `App/error` | Shared error codes, fault records, latch bookkeeping, and recovery helpers. |
| `App/debug` | CLI, VOFA, CSV, Codex-facing logs, and diagnosis-only output. |
| `App/port` | Adapters for BQ76952, SC8815, OLED, CAN, EEPROM, and board interfaces. |
| `App/storage` | Persisted SOC, learned capacity, cycle count, and calibration data. |
| `App/test` | Firmware-side test hooks and PC-side analysis helpers. |

## Data Flow

```text
port sample -> model update -> estimate -> server/protection -> server/power -> output command -> port apply -> debug
```

Only `port` may call `Int_*` modules directly. Services and estimators should
read from `BmsContext_t` and write decisions back to command/fault/service
fields.

## Migration Phases

1. Create `BmsContext_t` and mirror the existing global BatMan snapshot into it.
2. Move debug CSV/monitor output to read from `BmsContext_t`.
3. Move protection decisions into `App/server`.
4. Wrap BQ76952 and SC8815 access in `App/port`.
5. Split `App_Power` into charge, discharge, pre-discharge, and balance services.
6. Move SOC/SOH/RC algorithms under `App/estimate`.
7. Replace public BatMan globals with read-only snapshot accessors.
8. Remove legacy adapters after all users consume the new context.

## Boundary Rules

- `model` owns data shape only; it must not access hardware.
- `estimate` must not call `Int_*`, `App_SC8815_*`, or FET write APIs.
- `server` owns protection and service decisions, but must not perform register writes.
- `error` stores reusable fault records and latch helpers used by server modules.
- `debug` may read snapshots and issue explicit test commands, but normal safety
  policy must not depend on debug code.
- `port` is the only layer that translates model/service commands to chip APIs.

## Current Legacy Adapter

During migration, `App_BatMan.*`, `App_Power.*`, `App_SC8815.*`, and
`App_DebugCli.*` keep their current behavior. They will gradually become
adapters around the new folders.

## Current Implemented Slice

- The target APP folder set now exists: `config`, `core`, `debug`, `error`,
  `estimate`, `model`, `port`, `server`, and `storage`.
- `App/config/Bms_Config.*` owns power-policy thresholds for charge,
  discharge, pre-discharge, SC input validation, BQ wake timeout, debug period,
  and feature toggles.
- `App/model/Bms_Model.*` owns the shared `Bms_ContextTypeDef` snapshot.
- `App/core/Bms_App.*` initializes the model context and is reserved as the
  future orchestration entry.
- `App/error/Bms_Error.*`, `App/debug/Bms_Debug.*`, `App/estimate/Bms_Estimate.*`,
  and `App/storage/Bms_Storage.*` provide stable module boundaries for upcoming
  migration while preserving current behavior.
- `App_BatMan_SyncModelSnapshot()` mirrors legacy BatMan globals into the model
  after sampling, estimating, and balancing.
- `App/server/Bms_ProtectionService.*` owns monitor fault detection, BQ
  protection reason collection, and SCD status checks.
- `App/server/Bms_ChargeService.*` owns charge-side policy evaluation: charge
  temperature window, full-charge latch, and top-balance charge stop hysteresis.
- `App/server/Bms_DischargeService.*` owns pure discharge policy evaluation:
  sample validity, discharge temperature, low-voltage, RC recovery blocking, and
  software over-current checks.
- `App/server/Bms_PreDischargeService.*` owns pre-discharge wait timing state;
  `App_Power.c` still applies the actual BQ FET request.
- `App/port/Bms_PowerPort.*` adapts legacy `App_BatMan_*` FET APIs and
  `App_SC8815_*` charge/input status APIs behind a power-port boundary.
- `App_BatMan_Debug.c` remains the log printer, but BQFAST stop reasons now come
  from `Bms_ProtectionService_CollectMonitorFaultReasons()`.
- `App_Power.c` still owns the legacy power state machine, but hardware output
  now goes through `Bms_PowerPort` and service state syncs back into
  `BmsContext.service`; it reads battery/protection data from `Bms_Model`
  instead of directly reading legacy BatMan globals.
