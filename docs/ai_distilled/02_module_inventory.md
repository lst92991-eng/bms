# Module Inventory

## Top-Level Folders

| Path | Role | Evidence | Notes |
| --- | --- | --- | --- |
| `bms24v_platform/Core` | CubeMX generated core sources, `main.c`, peripheral init, IRQ handlers | `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx:395-445` | Avoid broad edits unless changing CubeMX/Core behavior |
| `bms24v_platform/Drivers` | STM32G0 HAL/CMSIS drivers | `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx:600-725` | Vendor/generated; do not put app behavior here |
| `bms24v_platform/MDK-ARM` | Keil startup, scatter, project metadata | `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx:385-390`; `bms24v_platform/MDK-ARM/bms24v_platform/bms24v_platform.sct:5-12` | Keep in sync when adding sources |
| `App` | Application state machines and bring-up workflows | `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx:450-480` | Public APIs use `App_*` |
| `Com` | Pure conversion/shared helpers | `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx:485-495` | Should not access I2C/GPIO |
| `Int` | Interface/driver layer for chips and board IO | `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx:500-595` | Public APIs use `Int_*`; BSP constants in `*_BSP.h` |
| `docs` | Project rules and logic docs | `docs/rules/hardware_rules.md:7-13`; `docs/logic/hardware_interface_reservation.md:1-13` | Authoritative project intent when code/config conflicts are explicit |
| `official_chip_docs_files` | Extracted chip/hardware/migration notes and netlists | `official_chip_docs_files/README.md:1-4` | Treat PDFs as requiring page-level confirmation unless rendered |

## App Modules

| Module | Public API | Responsibility | Evidence | Risks |
| --- | --- | --- | --- | --- |
| `App_BatMan` | `App_BatMan_Init`, `App_BatMan_Task` | BQ76952 init, Data Memory baseline, sampling, SOC/SOH, BQ FET enable/monitor | `App/App_BatMan.h:24-32`; `App/App_BatMan.c:807-928` | CRC default, current sign/gain, temperature config, FET status bits |
| `App_SC8815` | `App_SC8815_Init`, `Task`, `RequestCharge`, `GetState` | SC8815 standby monitor, guarded charge request state machine, STATUS/ADC sample | `App/App_SC8815.h:31-51`; `App/App_SC8815.c:72-144,209-283` | Must not release PSTOP until hardware divider/safety confirmed |
| `App_OLED` | `App_OLED_Init`, `ShowIicStatus`, `ShowBqIicPowerConfig` | Bring-up OLED status page for BQ I2C and power config | `App/App_OLED.h:7-9`; `App/App_OLED.c:56-143` | Display is bring-up only; no full UI state machine |

## Int Modules

| Module | Public API shape | Responsibility | Evidence | Risks |
| --- | --- | --- | --- | --- |
| `Int_BQ76952` | status enum + board, CRC, direct, subcommand, Data Memory, ConfigUpdate functions | Low-level BQ76952 transport/protocol primitives | `Int/Int_BQ76952.h:7-122`; `Int/Int_BQ76952.c:328-614` | CRC default conflict; WAKE direction conflict |
| `Int_BQ76952_BSP` | BQ commands, Data Memory, bit masks, 6S mapping | Detailed BQ76952 register/BSP macro reference | `Int/Int_BQ76952_BSP.h:210-237,297-311` | Must stay aligned with TRM and hardware mapping |
| `Int_SC8815` | status enum + safe state, CE/PSTOP, reg, status, ADC, current limits | Software IIC and guarded SC8815 register access | `Int/Int_SC8815.h:7-109`; `Int/Int_SC8815.c:49-153,235-509` | Software IIC timing and SC8815 INT path need hardware verification |
| `Int_SC8815_BSP` | SC8815 register masks, ratios, project safe/forbidden masks | Detailed SC8815 register/BSP macro reference | `Int/Int_SC8815_BSP.h:497-566` | R17/R18 and divider assumptions need actual-board confirmation |
| `Int_CanFd` | init/send/receive/status with frame struct | FDCAN1 transport wrapper | `Int/Int_CanFd.h:39-75`; `Int/Int_CanFd.c:153-320` | Protocol ownership and init error handling missing |
| `Int_EEPROM` | init/ready/read/write/write-readback | Raw M24C64 page-safe storage access | `Int/Int_EEPROM.h:7-65`; `Int/Int_EEPROM.c:117-207` | No application data schema yet |
| `Int_OLED` | OLED draw/text/init primitives | SSD1315-style I2C display driver | `Int/Int_OLED.h:8-30`; `Int/Int_OLED.c:4-12,548-585` | Address and bounds should be verified before expanding UI |
| `Int_Button` | init/task/state/events/hold | Polling debounce for PD3 button | `Int/Int_Button.h:18-59`; `Int/Int_Button.c:18,63-139` | No EXTI; task must be called regularly |
| `Int_Buzzer` | init/set/start/task/stop/blocking tone | GPIO-driven buzzer control | `Int/Int_Buzzer.h:7-61`; `Int/Int_Buzzer.c:101-201` | Hardware note says TIM3_CH2; code is GPIO toggle |
| `Int_Led` | init/set/toggle/all/read | Active-low red/green LED driver | `Int/Int_Led.h:14-42`; `Int/Int_Led.c:40-113` | Active-low must be preserved |

## Com Modules

| Module | Responsibility | Evidence | Rules |
| --- | --- | --- | --- |
| `Com_BQ76952` | Pack constants, 0.1K-to-C conversion, OCV/SOC interpolation | `Com/Com_BQ76952.h:22-67`; `Com/Com_BQ76952.c:10-80` | Keep pure: no I2C, no GPIO, no Data Memory writes |

## Generated Core Modules

| Module | Responsibility | Evidence | Notes |
| --- | --- | --- | --- |
| `main.c` | Initialization and bare-metal scheduler | `bms24v_platform/Core/Src/main.c:107-169` | Keep behavior minimal |
| `gpio.c` | GPIO init and EXTI4_15 enable | `bms24v_platform/Core/Src/gpio.c:42-118` | Generated-style file; update via CubeMX when possible |
| `i2c.c` | I2C1/I2C2 init and MSP pin setup | `bms24v_platform/Core/Src/i2c.c:31-107,139-179` | BQ on I2C1, OLED/EEPROM on I2C2 |
| `fdcan.c` | FDCAN1 init and PB8/PB9 AF setup | `bms24v_platform/Core/Src/fdcan.c:40-58,81-101` | Transport wrapper is `Int_CanFd` |
| `usart.c` | USART1 115200 debug UART | `bms24v_platform/Core/Src/usart.c:41-52,99-106` | Used by `printf`/retarget |
| `stm32g0xx_it.c` | IRQ handlers | `bms24v_platform/Core/Src/stm32g0xx_it.c:107-126` | BQ INT handler calls HAL only |

## Code Size Guidance

The current style favors one focused `.c/.h` pair per hardware or app responsibility. Large register definition sets belong in `*_BSP.h`, while behavior stays in `*.c`.

Evidence: `Int/Int_BQ76952_BSP.h:210-313`, `Int/Int_SC8815_BSP.h:497-566`, `App/App_BatMan.c:807-928`, `App/App_SC8815.c:72-144`.  
Confidence: High. Human confirmation: Not needed.

Practical target for future additions:

- Small board IO modules: keep public APIs under about 10 functions unless there is a clear state-machine need.
- Chip INT modules: allow more functions, but group by protocol primitive, status read, and config write.
- APP modules: keep one public `Init` and one public `Task` where possible; add explicit request/query APIs only for cross-module behavior.
- Move dense macro/register tables to `*_BSP.h`, with comments next to safety-critical bits.

Evidence basis: `Int/Int_Button.h:18-59`, `Int/Int_Buzzer.h:10-61`, `Int/Int_BQ76952.h:20-122`, `App/App_SC8815.h:31-51`.
