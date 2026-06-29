# Project Overview

## Product Shape

This repository is a 24V/6S lithium BMS bring-up firmware project for an STM32G0B1-based control board. The active code targets a BMS system with BQ76952 AFE monitoring/protection, SC8815 charge control, OLED bring-up display, EEPROM storage, CAN FD, UART debug, LED, buzzer, and button interfaces.

Evidence: `docs/rules/hardware_rules.md:23-29`, `bms24v_platform/bms24v_platform.ioc:43-44`, `bms24v_platform/Core/Src/main.c:31-38`.  
Confidence: High. Human confirmation: Not needed for current repo shape.

## MCU And Toolchain

- MCU: STM32G0B1 family, Keil target device `STM32G0B1CBTx`.
- Package: LQFP48 in CubeMX metadata.
- Toolchain: MDK-ARM V5.32.
- HAL/CubeMX project: `.ioc`, generated `Core/`, `Drivers/`, and Keil `.uvprojx` are present.

Evidence: `bms24v_platform/bms24v_platform.ioc:43-44,207`, `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx:10,17,21`, `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx:395-710`.  
Confidence: High. Human confirmation: Not needed.

## Runtime Model

The active firmware is bare-metal. `main.c` initializes HAL, clock, CubeMX peripherals, then local `Int_*` and `App_*` modules. The main loop uses `HAL_GetTick()` deltas for periodic board IO, BQ sampling, and SC8815 sampling, then sleeps with `HAL_Delay(10u)`.

Evidence: `bms24v_platform/Core/Src/main.c:107,114,121-139,148-169`.  
Confidence: High. Human confirmation: Not needed.

## Main Hardware Roles

| Area | Implementation anchor | Evidence |
| --- | --- | --- |
| BQ76952 AFE | `App/App_BatMan.c`, `Int/Int_BQ76952.c`, `Int/Int_BQ76952_BSP.h` | `App/App_BatMan.c:807-907`; `Int/Int_BQ76952.h:20-122` |
| SC8815 charger | `App/App_SC8815.c`, `Int/Int_SC8815.c`, `Int/Int_SC8815_BSP.h` | `App/App_SC8815.c:13-19,72-144`; `Int/Int_SC8815.c:49-153,235-402` |
| OLED bring-up page | `App/App_OLED.c`, `Int/Int_OLED.c` | `App/App_OLED.c:56-75,81-143`; `Int/Int_OLED.h:8-30` |
| EEPROM raw storage | `Int/Int_EEPROM.c` | `Int/Int_EEPROM.h:7-9,22-65`; `Int/Int_EEPROM.c:117-207` |
| CAN FD transport | `Int/Int_CanFd.c` | `Int/Int_CanFd.c:153-199,232-320` |
| Board IO | `Int_Button`, `Int_Buzzer`, `Int_Led` | `Int/Int_Button.c:63-98`; `Int/Int_Buzzer.c:101-169`; `Int/Int_Led.c:40-113` |

## Most Important Current Risks

1. BQ76952 CRC default conflicts between hardware rule and current code.
   Evidence: `docs/rules/hardware_rules.md:72-73`, `App/App_BatMan.c:28-31`, `Int/Int_BQ76952.c:49`.

2. BQ WAKE pin direction conflicts between generated GPIO input and driver fallback write behavior.
   Evidence: `bms24v_platform/Core/Src/gpio.c:98-102`, `Int/Int_BQ76952.c:14-19,261-267`.

3. SC8815 INT is documented as EXTI-style but configured as input-only in current code.
   Evidence: `docs/logic/hardware_interface_reservation.md:92,133`, `bms24v_platform/Core/Src/gpio.c:64-68`.

4. SC8815 R17/R18 actual solder state is not confirmed.
   Evidence: `official_chip_docs_files/README.md:1`, `official_chip_docs_files/full_netlist (4).csv:166-169`.

See `08_conflicts_and_unknowns.md` for the auditable list.

## Unknowns

- No active RTOS evidence.
- No watchdog evidence.
- No bootloader/OTA partition evidence.
- No final CAN protocol/ICD evidence.
- PDF schematic page-level verification was not performed in this distillation pass.

Evidence: `bms24v_platform/Core/Src/main.c:148-169`, `bms24v_platform/MDK-ARM/bms24v_platform/bms24v_platform.sct:5-12`, `08_conflicts_and_unknowns.md`.  
Confidence: High for absence in scanned evidence; Medium for final absence until full repo/PDF audit.
