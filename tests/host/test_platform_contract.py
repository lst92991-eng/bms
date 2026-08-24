#!/usr/bin/env python3
"""Host-side structural guards for safety-critical platform contracts."""

from __future__ import annotations

import argparse
from pathlib import Path


def read(root: Path, relative: str) -> str:
    return (root / relative).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def require_define(source: str, name: str, value: str, message: str) -> None:
    normalized = {" ".join(line.split()) for line in source.splitlines()}
    require(f"#define {name} {value}" in normalized, message)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    args = parser.parse_args()
    root = args.source_root.resolve()

    retarget = read(root, "bms24v_platform/Core/Src/retarget.c")
    core_main = read(root, "bms24v_platform/Core/Src/main.c")
    oled = read(root, "Int/Int_OLED.c")
    eeprom = read(root, "Int/Int_EEPROM.c")
    i2c2_bus = read(root, "Int/Int_I2C2Bus.c")
    cmake = read(root, "CMakeLists.txt")
    main_source = read(root, "App/App_Main.c")
    safety = read(root, "App/App_Safety.c")
    config = read(
        root,
        "bms24v_platform/MDK-ARM/FreeRTOS/include/FreeRTOSConfig.h",
    )
    irq = read(root, "bms24v_platform/Core/Src/stm32g0xx_it.c")
    debug_cli = read(root, "App/App_DebugCli.c")
    buzzer = read(root, "App/App_Buzzer.c")
    int_log = read(root, "Int/Int_Log.c")
    nvm = read(root, "App/App_BatMan_Nvm.c")
    log_users = "\n".join(
        read(root, relative)
        for relative in (
            "App/App_BatMan.c",
            "App/App_BatMan_Config.c",
            "App/App_BatMan_Debug.c",
            "App/App_BatMan_Nvm.c",
            "App/App_DebugCli.c",
            "App/App_Main.c",
            "App/App_Power.c",
        )
    )

    require("HAL_UART_Transmit(" not in retarget, "retarget must never block on UART")
    require("Int_Log_TryWrite" in retarget, "retarget must use bounded log queue")
    require("Com_FormatV" in int_log and "vsnprintf" not in int_log,
            "firmware logging must use the bounded no-FILE/no-heap formatter")
    require("check_forbidden_symbols.py" in cmake,
            "each firmware link must enforce the heap/stdio symbol policy")
    require("printf(" not in log_users,
            "application code must not bypass the bounded log formatter")
    for forbidden_format in ("%f", "%F", "%e", "%E", "%g", "%G", "%a", "%A"):
        require(forbidden_format not in log_users,
                f"unsupported floating log format present: {forbidden_format}")
    require("xTaskCreateStatic" in main_source, "critical tasks must use static memory")
    require("#if BMS_ENGINEERING_BUILD" in main_source, "CLI must be build-gated")
    require(
        buzzer.count("#if (BMS_ENGINEERING_BUILD != 0)") >= 4,
        "demo melody and automatic playback must be engineering-build only",
    )
    pre_hal_init = core_main.split("/* USER CODE BEGIN 1 */", maxsplit=1)[1].split(
        "/* USER CODE END 1 */", maxsplit=1
    )[0]
    require(
        "Int_SC8815_AssertBootSafeGpio();" in pre_hal_init
        and core_main.index("Int_SC8815_AssertBootSafeGpio();")
        < core_main.index("HAL_Init();"),
        "SC PSTOP/#CE must be driven safe before HAL timebase/IRQ initialization",
    )
    sys_init = core_main.split("/* USER CODE BEGIN SysInit */", maxsplit=1)[1].split(
        "/* USER CODE END SysInit */", maxsplit=1
    )[0]
    require(
        sys_init.index("MX_GPIO_Init();")
        < sys_init.index("Int_SC8815_ForceStandby();")
        < sys_init.index("MX_I2C1_Init();")
        < sys_init.index("App_BatMan_EarlySafeOutputs();"),
        "SC/BQ safe outputs must precede Cube's full peripheral startup list",
    )
    require("App_Main(s_early_bq_safe);" in core_main,
            "early BQ all-off evidence must reach the Safety supervisor")
    app_init = main_source.split("static void App_Main_Init", maxsplit=1)[1].split(
        "static bool App_Main_CreateTasks", maxsplit=1
    )[0]
    require(
        app_init.index("Int_SC8815_ForceStandby();")
        < app_init.index("App_BatMan_EarlySafeOutputs()")
        < app_init.index("Int_Log_Printf("),
        "boot must stop SC and confirm BQ FET-off before logging/peripheral bring-up",
    )
    maintenance = main_source.split("static void maintenance_task", maxsplit=1)[1].split(
        "static void buzzer_task", maxsplit=1
    )[0]
    require(
        maintenance.index("App_BatMan_MaintenanceTask")
        < maintenance.index("App_OLED_Task"),
        "EEPROM maintenance and OLED must share one serialized I2C2 task",
    )
    require_define(config, "configCHECK_FOR_STACK_OVERFLOW", "2", "stack hook disabled")
    require_define(config, "INCLUDE_uxTaskGetStackHighWaterMark", "1", "HWM disabled")
    require("Int_Watchdog_Refresh" in safety, "supervisor must own watchdog feed")
    require("App_Safety_AllTasksSeen" in safety, "watchdog feed needs all heartbeats")
    require(
        "APP_SAFETY_INHIBIT_BQ_PROTECTION_LATCHED" in safety,
        "BQ protection needs an independent power latch",
    )
    require(
        "App_Safety_GetBqWakeAuthorization" in safety
        and "s_power_inhibit_mask == APP_SAFETY_INHIBIT_BQ_INIT" in safety
        and "APP_SAFETY_BQ_WAKE_AUTHORIZATION_MS = 500u" in safety,
        "BQ wake needs a typed, bounded authorization capability",
    )
    report_bq_ready = safety.split("void App_Safety_ReportBqReady", maxsplit=1)[1].split(
        "void App_Safety_ReportScReady", maxsplit=1
    )[0]
    require(
        "ready && !s_bq_early_safe_failed" in report_bq_ready,
        "a missing earliest FET-off proof must remain latched for the whole boot",
    )
    update_inhibit = safety.split("static void App_Safety_UpdatePowerInhibit", maxsplit=1)[1].split(
        "static void App_Safety_ExpireBqWakeAuthorization", maxsplit=1
    )[0]
    require(
        "(set_mask != 0u) || s_bq_wake_authorized" in update_inhibit
        and update_inhibit.index("Int_SC8815_ForceStandby")
        < update_inhibit.index("s_power_release_authorized = false"),
        "every newly asserted safety inhibit must stop SC hardware before software authorization changes",
    )
    expire_wake = safety.split(
        "static void App_Safety_ExpireBqWakeAuthorization", maxsplit=1
    )[1].split("static void App_Safety_Trip", maxsplit=1)[0]
    require(
        expire_wake.index("Int_SC8815_ForceStandby();")
        < expire_wake.index("s_bq_wake_authorized = false"),
        "wake expiry must stop hardware before invalidating software state",
    )
    resolve_bq = safety.split("void App_Safety_ResolveBqAlert", maxsplit=1)[1].split(
        "void App_Safety_ReportBqReady", maxsplit=1
    )[0]
    require(
        "App_Safety_Trip" not in resolve_bq,
        "BQ hardware protection must not create a watchdog reset loop",
    )
    require("App_Safety_OnBqAlertFromISR" in irq, "BQ ALERT must enter safety path")
    require("App_BatMan_NotifyAlertFromISR" in irq, "BQ ALERT event must be deferred")
    require("Int_SC8815_ForceStandby();" in irq, "SC ISR must force standby first")
    bq_irq = irq.split("else if (GPIO_Pin == BQ_INT_Pin)", maxsplit=1)[1].split(
        "void HAL_UART_ErrorCallback", maxsplit=1
    )[0]
    require(
        bq_irq.index("Int_SC8815_ForceStandby();")
        < bq_irq.index("App_Safety_OnBqAlertFromISR()")
        < bq_irq.index("App_BatMan_NotifyAlertFromISR()"),
        "BQ ISR must stop power before revoking authorization and deferring diagnosis",
    )
    require(
        "APP_SAFETY_INHIBIT_CAN_INIT" not in main_source,
        "CAN startup failure must not inhibit local power protection",
    )
    require_define(config, "configUSE_RECURSIVE_MUTEXES", "1", "recursive mutexes disabled")
    require_define(config, "configSUPPORT_DYNAMIC_ALLOCATION", "0", "runtime heap enabled")
    require("heap_4.c" not in cmake, "heap implementation must not be linked")
    require(
        "xSemaphoreCreateRecursiveMutexStatic" in i2c2_bus,
        "I2C2 bus ownership must use a static priority-inheritance mutex",
    )
    require(
        "Int_I2C2Bus_Lock" in oled and "Int_I2C2Bus_Lock" in eeprom,
        "OLED and EEPROM must share the I2C2 ownership boundary",
    )
    require(
        "StaticSemaphore_t s_nvm_mutex_buffer" in nvm
        and "xSemaphoreCreateRecursiveMutexStatic" in nvm
        and "xSemaphoreTakeRecursive" in nvm
        and "xSemaphoreGiveRecursive" in nvm,
        "NVM slot/sequence state and write-readback transactions need one static mutex",
    )
    capture = nvm.split("static void App_BatMan_NvmCaptureSnapshot", maxsplit=1)[1].split(
        "static bool App_BatMan_NvmRestoreSoc", maxsplit=1
    )[0]
    require(
        "App_BatMan_NvmSuspendScheduler" in capture
        and "Com_SOH_ExportPersistent" in capture
        and "Com_SOC_GetResult" in capture
        and "Com_SOC_ExportPersistent" in capture
        and "App_BatMan_NvmResumeScheduler" in capture,
        "SOC/SOH persistence fields must come from one task-atomic snapshot",
    )
    require(
        "Int_EEPROM_" not in capture,
        "task switching may be paused only for the in-memory snapshot, never for EEPROM I/O",
    )
    require(
        (irq + debug_cli + int_log).count("void HAL_UART_ErrorCallback") == 1,
        "UART error callback must have exactly one owner",
    )
    uart_error = irq.split("void HAL_UART_ErrorCallback", maxsplit=1)[1]
    require(
        uart_error.index("Int_Log_OnUartError")
        < uart_error.index("App_DebugCli_OnUartError"),
        "UART error recovery must release TX before rearming CLI RX",
    )
    clear_body = oled.split("void Inf_OLED_Clear(void)", maxsplit=1)[1].split(
        "void Inf_OLED_ShowText16", maxsplit=1
    )[0]
    require("Inf_OLED_Refresh" not in clear_body, "OLED clear must not refresh hardware")
    require(
        oled.count("HAL_I2C_Master_Transmit") == 2,
        "OLED driver must keep one bulk command helper and one page transfer site",
    )


if __name__ == "__main__":
    main()
