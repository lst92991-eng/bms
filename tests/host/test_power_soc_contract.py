#!/usr/bin/env python3
"""Structural guards for SC/Power/SOC release-safety contracts."""

from __future__ import annotations

import argparse
from pathlib import Path


def read(root: Path, relative: str) -> str:
    return (root / relative).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def function_body(source: str, signature: str, next_signature: str) -> str:
    return source.split(signature, maxsplit=1)[1].split(next_signature, maxsplit=1)[0]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    root = parser.parse_args().source_root.resolve()

    driver = read(root, "Int/Int_SC8815.c")
    bsp = read(root, "Int/Int_SC8815_BSP.h")
    sc_app = read(root, "App/App_SC8815.c")
    power = read(root, "App/App_Power.c")
    cli = read(root, "App/App_DebugCli.c")
    soc = read(root, "Com/Com_SOC.c")
    soh = read(root, "Com/Com_SOH.c")
    soh_header = read(root, "Com/Com_SOH.h")
    nvm = read(root, "App/App_BatMan_Nvm.c")

    force_body = function_body(
        driver, "void Int_SC8815_ForceStandby(void)", "bool Int_SC8815_IsStandbyAsserted"
    )
    boot_safe_body = function_body(
        driver, "void Int_SC8815_AssertBootSafeGpio(void)", "static uint32_t Int_SC8815_EnterCritical"
    )
    require("RCC->IOPENR" in boot_safe_body and "GPIOB->BSRR" in boot_safe_body,
            "pre-clock SC safe output must directly enable GPIOB and set both output latches")
    require("HAL_" not in boot_safe_body and "I2C" not in boot_safe_body and
            "printf(" not in boot_safe_body,
            "pre-clock SC safe output must not depend on HAL, I2C, or logging")
    require("BSRR" in force_body, "PSTOP emergency stop must be one direct GPIO write")
    require("HAL_" not in force_body and "printf(" not in force_body and
            "Int_SC8815_Bus" not in force_body,
            "PSTOP primitive must not use HAL, I2C transactions, or logging")
    require("xQueue" not in sc_app, "SC command path must not use a dynamic queue")
    require("s_sc8815_iic_swapped" not in driver,
            "software I2C mapping must be immutable at runtime")
    require("SC8815_SW_I2C_RECOVERY_PULSES" in driver,
            "SDA stuck recovery pulses missing")
    require("SC8815_PROJECT_IBUS_RATIO_X = 3u" in bsp and
            "SC8815_PROJECT_IBAT_RATIO_X = 6u" in bsp and
            "SC8815_PROJECT_RATIO_VALUE = 0x28u" in bsp,
            "SC current ratios must match the confirmed 10 mOhm board")
    require("SC8815_PROJECT_IBUS_LIMIT_MA = 1500u" in bsp and
            "SC8815_PROJECT_MAX_IBUS_LIMIT_MA = 3000u" in bsp,
            "IBUS default/hard limit must remain 1.5A/3A")
    require("SC8815_PROJECT_IBAT_LIMIT_MA = 1000u" in bsp and
            "APP_POWER_CHARGE_LIMIT_NOMINAL_MA = 1000u" in power,
            "25.2V release profile must use the cell's 4.20V standard-charge current")
    require("App_Safety_IsPowerReleaseAuthorized" in sc_app,
            "SC release must revalidate Safety epoch")
    sc_task = function_body(sc_app, "void App_SC8815_Task(uint16_t interval_ms)",
                            "void App_SC8815_RequestCharge(bool enable)")
    interrupt_owner = sc_task.split("if (interrupt_pending)", maxsplit=1)[1].split(
        "if (interrupt_pending ||", maxsplit=1
    )[0]
    require("s_sc8815_interrupt_sequence++" in driver and
            "App_Safety_SetPowerInhibit(APP_SAFETY_INHIBIT_SC_EVENT)" in interrupt_owner,
            "SC event owner must reassert the inhibit and retain monotonic IRQ provenance")
    resolve = sc_task.split("if (s_interrupt_clean_sample_count >= 2u)", maxsplit=1)[1].split(
        "else if (s_interrupt_resolution_pending", maxsplit=1
    )[0]
    require(resolve.index("App_SC8815_EnterCritical") <
            resolve.index("Int_SC8815_GetInterruptSequence") <
            resolve.index("App_Safety_ClearPowerInhibit") <
            resolve.index("App_SC8815_ExitCritical"),
            "SC event sequence recheck and inhibit clear must be atomic against a new IRQ")
    require("App_Safety_GetPowerReleaseAuthorization" in power,
            "Power must obtain Safety release epoch")
    require("App_Safety_GetBqWakeAuthorization" in power and
            "APP_POWER_BQ_WAKE_PULSE_DEADLINE_MS = 500u" in power,
            "BQ wake must use the typed one-shot Safety capability")
    require("#define APP_POWER_BQ_WAKE_HIL_ENABLE BMS_ENGINEERING_BUILD" in power,
            "automatic BQ wake must remain disabled in Release")
    require("App_SC8815_AuthorizeCharge" not in power,
            "Power must not self-clear a Safety trip")
    require("Int_BQ76952_ReadDirect" not in cli,
            "CLI must not bypass the BQ supervisor")
    require("App_SC8815_RequestCharge(true)" not in cli,
            "CLI must not directly start SC8815")
    require("!s_soc.result.rest_ready" in soc,
            "boot OCV seed must require proven rest")
    require("COM_SOC_ANCHOR_FULL_COMPLETE" in soc and
            "COM_SOC_ANCHOR_EMPTY_CUTOFF" in soc,
            "SOC anchors must consume explicit Power events")
    require("while (s_soh.cycle_bucket_mah" not in soh and
            "combined_whole_mah / capacity_mah" in soh and
            "combined_whole_mah % capacity_mah" in soh,
            "SOH cycle accumulation must be O(1) quotient/remainder")
    require("COM_SOH_CAPACITY_MAX_MAH = 200000u" in soh_header and
            "(uint64_t)s_soh.config.capacity_mah" in soh,
            "SOH capacity and learning products need bounded 64-bit math")
    require("APP_BATMAN_NVM_MAGIC" in nvm and "APP_BATMAN_SOC_NVM_MAGIC" in nvm,
            "SOH2 compatibility and independent SOC3 records are required")


if __name__ == "__main__":
    main()
