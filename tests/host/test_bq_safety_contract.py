#!/usr/bin/env python3
"""Structural guards for BQ76952 transport, proof, and FET safety contracts."""

from __future__ import annotations

import argparse
from pathlib import Path


def read(root: Path, relative: str) -> str:
    return (root / relative).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def function_body(source: str, signature: str, next_signature: str) -> str:
    require(signature in source, f"missing function marker: {signature}")
    require(next_signature in source, f"missing next function marker: {next_signature}")
    return source.split(signature, maxsplit=1)[1].split(next_signature, maxsplit=1)[0]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    root = parser.parse_args().source_root.resolve()

    driver = read(root, "Int/Int_BQ76952.c")
    bsp = read(root, "Int/Int_BQ76952_BSP.h")
    app = read(root, "App/App_BatMan.c")
    config = read(root, "App/App_BatMan_Config.c")
    sample = read(root, "App/App_BatMan_Sample.c")
    power = read(root, "App/App_Power.c")

    require("StaticSemaphore_t storage" in driver,
            "BQ bus lock must use static storage")
    require("xSemaphoreCreateRecursiveMutexStatic" in driver,
            "BQ bus lock must be a static recursive mutex")
    require("xSemaphoreTakeRecursive" in driver and
            "xSemaphoreGiveRecursive" in driver,
            "nested BQ transactions must use recursive mutex operations")
    require("INT_BQ76952_I2C_TIMEOUT_MS = 10u" in driver,
            "each HAL I2C operation must remain bounded to 10 ms")
    require("printf(" not in driver,
            "BQ transport must not block on diagnostic output")
    require("Int_BQ76952_BeginTransaction(APP_BATMAN_SAMPLE_BUDGET_MS)" in sample,
            "the whole telemetry frame must inherit one absolute deadline")
    require("APP_BATMAN_SAMPLE_BUDGET_MS = 100u" in sample and
            "APP_BATMAN_FRAME_DEADLINE_EXCEEDED" in sample,
            "telemetry must expose an explicit frame deadline failure")

    indirect_read = function_body(
        driver,
        "Int_BQ76952_ReadTransfer(uint16_t command_or_address",
        "static Int_BQ76952_StatusTypeDef Int_BQ76952_ReadCfgUpdateBit",
    )
    require(indirect_read.index("Int_BQ76952_WaitEcho") <
            indirect_read.index("BQ76952_TRANSFER_LENGTH") <
            indirect_read.index("BQ76952_TRANSFER_BUFFER_START") <
            indirect_read.index("BQ76952_TRANSFER_CHECKSUM"),
            "indirect reads must validate echo, length, payload, then checksum")
    require("Int_BQ76952_BufferChecksum" in indirect_read and
            "INT_BQ76952_ERROR_CHECKSUM" in indirect_read,
            "indirect reads must reject a checksum mismatch")
    notify = function_body(
        driver,
        "void Int_BQ76952_NotifyAlertFromISR(void)",
        "bool Int_BQ76952_TakeAlertPending",
    )
    require("I2C" not in notify and "HAL_" not in notify and "printf(" not in notify,
            "BQ ALERT ISR path may only latch in-memory provenance")
    require("s_bq76952_alert_sequence++" in notify and
            "Int_BQ76952_GetAlertSequence" in driver,
            "BQ ALERT provenance must use a monotonic sequence")

    require("ALL_FETS_ON" not in (driver + config + bsp),
            "runtime code must never contain ALL_FETS_ON")
    safe_enable = function_body(
        config,
        "bool App_BatMan_EnableFetControlSafely(void)",
        "Int_BQ76952_StatusTypeDef App_BatMan_KeepMainFetsOff",
    )
    runtime_fet = function_body(
        config,
        "static Int_BQ76952_StatusTypeDef App_BatMan_WriteMainFetControl",
        "bool App_BatMan_EnableFetControlSafely(void)",
    )
    all_off_index = safe_enable.find("BQ76952_SUBCMD_ALL_FETS_OFF")
    blocker_readback_index = safe_enable.find("BQ76952_CMD_FET_STATUS", all_off_index)
    enable_index = safe_enable.find("BQ76952_SUBCMD_FET_ENABLE")
    enable_readback_index = safe_enable.find("BQ76952_CMD_FET_STATUS", enable_index)
    require(all_off_index >= 0 and blocker_readback_index > all_off_index and
            enable_index > blocker_readback_index and
            enable_readback_index > enable_index,
            "FET_ENABLE must remain behind ALL_FETS_OFF and two all-off readbacks")
    require("BQ76952_SUBCMD_FET_ENABLE" not in runtime_fet,
            "normal runtime FET requests must not re-enable manufacturing FET control")
    require(runtime_fet.count("App_Safety_IsPowerReleaseAuthorized") >= 3,
            "runtime FET release must validate the Safety epoch before, during, and after I2C")
    require("App_BatMan_RequestAllFetsOffAfterFailure" in runtime_fet,
            "every runtime FET error must converge to the latched all-off path")
    require("s_fet_control_state.commanded_off_mask = off_mask" in runtime_fet and
            runtime_fet.index("Int_BQ76952_ApplyFetControl") <
            runtime_fet.index("s_fet_control_state.commanded_off_mask = off_mask"),
            "commanded FET state may update only after command/readback success")

    early_safe = function_body(
        app,
        "bool App_BatMan_EarlySafeOutputs(void)",
        "static void App_BatMan_ResetState",
    )
    require("App_BatMan_PreResetAllFetsOff" in early_safe and
            "printf(" not in early_safe and "Nvm" not in early_safe,
            "early safe output must be idempotent, silent, and free of EEPROM work")
    early_off = function_body(
        config,
        "bool App_BatMan_PreResetAllFetsOff(void)",
        "static void App_BatMan_RequestAllFetsOffAfterFailure",
    )
    require("Int_BQ76952_BeginTransaction" in early_off and
            "Int_BQ76952_EndTransaction" in early_off,
            "ALL_FETS_OFF and FET_STATUS readback must be one bus transaction")
    require(early_off.index("BQ76952_SUBCMD_ALL_FETS_OFF") <
            early_off.index("BQ76952_CMD_FET_STATUS") <
            early_off.index("s_fet_control_state.commanded_off_mask"),
            "pre-reset software state must follow physical all-off readback")

    verify = function_body(
        config,
        "bool App_BatMan_VerifyBqConfig(void)",
        "void App_BatMan_GetFetControlState",
    )
    require("s_dm_manifest" in verify and "continue;" not in verify,
            "manifest verification must stop at the first bus or mismatch error")
    require("BQ76952_DEVICE_NUMBER_EXPECTED = 0x7695u" in bsp,
            "BQ identity must use the exact 0x7695 device number")
    require("device_number != BQ76952_DEVICE_NUMBER_EXPECTED" in app,
            "startup must reject any unexpected BQ device")
    require("App_BatMan_ClearStartupAlarms() != INT_BQ76952_OK" in config and
            "ret = App_BatMan_ClearStartupAlarms();" in app,
            "startup and reauthentication must check alarm-clear transport status")

    reauth = function_body(
        config,
        "bool App_BatMan_ReauthenticateAfterReset(void)",
        "bool App_BatMan_RequestShutdown",
    )
    reauth_locked = function_body(
        config,
        "static bool App_BatMan_ReauthenticateAfterResetLocked(void)",
        "bool App_BatMan_ReauthenticateAfterReset(void)",
    )
    require("APP_BATMAN_REAUTHENTICATION_DEADLINE_MS = 1500u" in config and
            "Int_BQ76952_BeginTransaction(APP_BATMAN_REAUTHENTICATION_DEADLINE_MS)" in reauth and
            "App_BatMan_ReauthenticateAfterResetLocked" in reauth and
            "Int_BQ76952_EndTransaction" in reauth,
            "full reset recovery must be one bounded recursive transaction")
    for required_step in (
        "App_BatMan_PreResetAllFetsOff",
        "BQ76952_SUBCMD_DEVICE_NUMBER",
        "Int_BQ76952_EnterConfigUpdate",
        "App_BatMan_ConfigBq",
        "Int_BQ76952_ExitConfigUpdate",
        "App_BatMan_VerifyBqConfig",
        "App_BatMan_ClearStartupAlarms",
        "App_BatMan_EnableFetControlSafely",
        "BQ76952_CMD_ALARM_RAW_STATUS",
        "BQ76952_CMD_BATTERY_STATUS",
        "BQ76952_SUBCMD_MANUFACTURING_STATUS",
        "BQ76952_CMD_FET_STATUS",
    ):
        require(required_step in reauth_locked,
                f"reset reauthentication missing step: {required_step}")

    require("App_Safety_ReportBqReady" in app and "App_BatMan_IsConfigValid" in app,
            "BQ readiness must remain owned by BatMan after frame/config checks")
    runtime_proof = function_body(
        app,
        "static void App_BatMan_EnforceRuntimeProof",
        "void App_BatMan_NotifyAlertFromISR",
    )
    require("App_BatMan_FrameLostConfigProof" in runtime_proof and
            "BQ76952_BATTERY_STATUS_POR_MASK" in app and
            "BQ76952_BATTERY_STATUS_CFGUPDATE_MASK" in app and
            "BQ76952_MFG_STATUS_FET_EN_MASK" in app,
            "the safety facade must interpret every complete frame's reset fingerprint")
    require(runtime_proof.index("App_SC8815_EmergencyStop") <
            runtime_proof.index("App_BatMan_MarkConfigRecoveryRequired") <
            runtime_proof.index("App_Safety_ReportBqReady(false)") <
            runtime_proof.index("App_BatMan_KeepMainFetsOff"),
            "proof loss must assert PSTOP before proof mutation, readiness revoke, and BQ all-off")
    require("App_BatMan_MarkConfigRecoveryRequired" not in sample,
            "the sampler must not mutate proof before the safety facade asserts PSTOP")
    require("App_BatMan_ObserveFetStatus(frame->fet_status)" in sample,
            "each complete frame must revalidate forbidden FET observations")

    task_body = function_body(
        app,
        "void App_BatMan_Task(uint32_t interval_ms)",
        "void App_BatMan_MaintenanceTask",
    )
    require(task_body.index("App_BatMan_Sample") <
            task_body.index("App_BatMan_EnforceRuntimeProof") <
            task_body.index("App_BatMan_ProcessAlertAfterSample") <
            task_body.index("App_BatMan_ReportBqReadiness"),
            "proof enforcement must precede ALERT resolution and readiness publication")
    unresolved_mask = app.split(
        "APP_BATMAN_ALERT_UNRESOLVED_RAW_MASK =", maxsplit=1
    )[1].split("\n};", maxsplit=1)[0]
    require("BQ76952_ALARM_XCHG_MASK" not in unresolved_mask and
            "BQ76952_ALARM_XDSG_MASK" not in unresolved_mask,
            "host-requested FET-off observations must not deadlock ALERT recovery")
    require("Int_BQ76952_GetAlertSequence() == sequence_before_sample" in app and
            "!App_BatMan_IsAlertPinAsserted()" in app and
            "App_Safety_ResolveBqAlert(false" in app,
            "ALERT inhibit may clear only after stable sequence and deasserted pin")
    critical_alert = app.split("if (critical)", maxsplit=1)[1].split(
        "/* 配置、温度、FET、范围", maxsplit=1
    )[0]
    require("App_Safety_ResolveBqAlert(true" in critical_alert and
            "App_BatMan_KeepMainFetsOff" in critical_alert,
            "critical BQ alert must latch Safety and force all main FETs off")

    init_body = function_body(
        app,
        "bool App_BatMan_Init(void)",
        "void App_BatMan_Task(uint32_t interval_ms)",
    )
    require(init_body.index("if (!pre_reset_fets_off)") <
            init_body.index("Int_BQ76952_Reset()"),
            "BQ reset must be forbidden until ALL_FETS_OFF readback is confirmed")
    offline_branch = power.split("if (!bq_online)", maxsplit=1)[1].split(
        "if (!config_valid)", maxsplit=1
    )[0]
    require("App_Power_ForceOutputsOff" in offline_branch,
            "BQ offline handling must best-effort close hardware, not only software flags")


if __name__ == "__main__":
    main()
