# Evidence Index

This index records the main evidence used by the distilled project rules. Paths are relative to `C:\Users\lst\Desktop\bms`.

## Project And Build

| Evidence | What it supports |
| --- | --- |
| `bms24v_platform/bms24v_platform.ioc:43-44` | MCU family `STM32G0B1C(B-C-E)Tx`, package `LQFP48`. |
| `bms24v_platform/bms24v_platform.ioc:194,206-207` | Heap `0x200`, stack `0x400`, target toolchain `MDK-ARM V5.32`. |
| `bms24v_platform/bms24v_platform.ioc:212` | CubeMX init order: clock, GPIO, RTC, FDCAN1, I2C2, USART1, I2C1. |
| `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx:10,17,21` | Keil target name, device `STM32G0B1CBTx`, flash/RAM ranges. |
| `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx:343` | Include path includes generated Core, HAL/CMSIS, and local `App/Com/Int`. |
| `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx:395-445` | Core generated/source group. |
| `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx:450-595` | Local `App`, `Com`, and `Int` groups and file list. |
| `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx:600-710` | STM32G0 HAL driver source group. |
| `bms24v_platform/MDK-ARM/startup_stm32g0b1xx.s:31,42,55-60,116-119` | Startup stack/heap, vector table, reset handler imports `SystemInit` and `__main`. |
| `bms24v_platform/MDK-ARM/bms24v_platform/bms24v_platform.sct:5-12` | Flash `0x08000000/0x20000`, RAM `0x20000000/0x24000`. |

## Boot And Runtime Flow

| Evidence | What it supports |
| --- | --- |
| `bms24v_platform/Core/Src/main.c:20-38` | Main includes generated HAL headers plus `App_*` and `Int_*` modules. |
| `bms24v_platform/Core/Src/main.c:107,114,121-126` | HAL/system/peripheral init sequence. |
| `bms24v_platform/Core/Src/main.c:132-139` | Local module initialization order: LED, buzzer, button, CAN FD, EEPROM, OLED, SC8815, BQ app. |
| `bms24v_platform/Core/Src/main.c:148-169` | Bare-metal loop scheduler with `HAL_GetTick()` deltas and `HAL_Delay(10u)`. |
| `bms24v_platform/Core/Src/main.c:254-259` | `Error_Handler()` disables IRQ and loops forever. |
| `bms24v_platform/Core/Src/stm32g0xx_it.c:107-112` | `EXTI4_15_IRQHandler()` handles `BQ_INT_Pin`. |
| `bms24v_platform/Core/Src/stm32g0xx_it.c:121-126` | `TIM14_IRQHandler()` handles HAL timebase timer. |

## Hardware Configuration

| Evidence | What it supports |
| --- | --- |
| `bms24v_platform/bms24v_platform.ioc:5-23` | FDCAN1 nominal/data timing and frame mode. |
| `bms24v_platform/Core/Src/fdcan.c:40-58` | Generated FDCAN1 runtime config matches `.ioc`. |
| `bms24v_platform/Core/Src/fdcan.c:81-101` | FDCAN clock source and PB8/PB9 alternate function. |
| `bms24v_platform/bms24v_platform.ioc:26-30` | I2C1/I2C2 timing `0x10B17DB5`. |
| `bms24v_platform/Core/Src/i2c.c:31-64,74-107` | I2C1/I2C2 handles, timing, filter setup. |
| `bms24v_platform/Core/Src/i2c.c:139-146` | I2C1 pins PB6/PB7. |
| `bms24v_platform/Core/Src/i2c.c:172-179` | I2C2 pins PA11/PA12. |
| `bms24v_platform/Core/Src/usart.c:41-52,99-106` | USART1 115200 8N1 on PA9/PA10. |
| `bms24v_platform/Core/Src/gpio.c:55-61,70-89` | SC8815 software I2C pins, PSTOP, CE_N initial levels and GPIO modes. |
| `bms24v_platform/Core/Src/gpio.c:98-114` | BMS WAKE input and BQ INT falling-edge EXTI with pull-up. |
| `bms24v_platform/Core/Src/gpio.c:117-118` | EXTI4_15 priority and enable. |
| `docs/rules/hardware_rules.md:23-29` | Board-level hardware summary: MCU, SC8815, BQ76952, OLED, EEPROM, CAN-FD transceiver. |
| `docs/rules/hardware_rules.md:31-55` | SC8815 constraints: software IIC PA6/PA7, INT/CE/PSTOP, limits, forbidden behavior. |
| `docs/rules/hardware_rules.md:69-84` | BQ76952 constraints: I2C address, PB6/PB7, PB4 ALERT, PB3 WAKE, 6S VC mapping, current shunt. |
| `docs/rules/hardware_rules.md:88-109` | OLED/EEPROM, FDCAN, USART, LED, buzzer, button pin mapping. |
| `official_chip_docs_files/README.md:1-4` | Hardware notes: R17/R18 planned values, SC8815 software IIC, old project style reference, 6S 21700 target. |
| `official_chip_docs_files/full_netlist (4).csv:166-169` | Netlist evidence for R17/R18 current listed values and shunt position. |
| `official_chip_docs_files/full_netlist (5).csv:7-12` | SC8815 net names for CE_N, PSTOP, PA7 SCL, PA6 SDA, PA5 INT. |

## BQ76952

| Evidence | What it supports |
| --- | --- |
| `Int/Int_BQ76952.h:7-17` | Status enum and error categories. |
| `Int/Int_BQ76952.h:20-122` | Public API covers board init, wake, alert, reset, CRC, direct/subcommand/Data Memory, ConfigUpdate. |
| `Int/Int_BQ76952.c:10-45` | I2C handle defaults, WAKE/ALERT pins, timeout and buffer limits. |
| `Int/Int_BQ76952.c:57-104` | CRC8 and transfer-buffer checksum helpers. |
| `Int/Int_BQ76952.c:144-230` | Transfer buffer read with length and checksum validation. |
| `Int/Int_BQ76952.c:328-485` | Direct command read/write with optional CRC framing. |
| `Int/Int_BQ76952.c:495-614` | Subcommand and Data Memory transfer paths. |
| `Int/Int_BQ76952.c:639-698` | ConfigUpdate enter/exit workflow. |
| `Int/Int_BQ76952_BSP.h:210-237` | Data Memory addresses and comments for power, comm, Vcell, protection, FET, balancing. |
| `Int/Int_BQ76952_BSP.h:297-311` | 6S sparse cell mask: Cell1/2/6/9/12/16. |
| `App/App_BatMan.h:11-21` | APP-level BMS constants: 6 cells, capacity, voltage bounds, current polarity. |
| `App/App_BatMan.c:16-23` | APP policy: BQ owns FET decisions after FET_ENABLE; APP monitors and estimates. |
| `App/App_BatMan.c:243-317` | BQ baseline Data Memory configuration function. |
| `App/App_BatMan.c:327-345` | Manufacturing status check and `FET_ENABLE`. |
| `App/App_BatMan.c:417-439` | Cell voltage direct-command list uses Cell1/2/6/9/12/16. |
| `App/App_BatMan.c:492-528` | Current and temperature read paths. |
| `App/App_BatMan.c:807-907` | BQ initialization flow: CRC mode, reset, DeviceNumber, ConfigUpdate, Data Memory, FET enable, initial sample. |
| `official_chip_docs_files/BQ76930_to_BQ76952_逻辑替换设计说明.md:418-438` | 6S Vcell mapping and `Vcell Mode=0x8923` rationale. |
| `official_chip_docs_files/BQ76930_to_BQ76952_逻辑替换设计说明.md:1150-1156` | Bring-up unknowns: CC2 sign/gain, NTC resources, WAKE/SHUT timing, FET status bits. |

## SC8815

| Evidence | What it supports |
| --- | --- |
| `App/App_SC8815.h:8-17` | APP state snapshot fields and standby/charge request semantics. |
| `App/App_SC8815.h:31-51` | Public API: init, task, request charge, get state. |
| `App/App_SC8815.c:13-19` | SC8815 app boundary: MCU controls CE_N/PSTOP, writes guarded registers, reads status/ADC. |
| `App/App_SC8815.c:72-144` | Charge request state machine and safety checks. |
| `App/App_SC8815.c:153-175` | STATUS/ADC sampling. |
| `App/App_SC8815.c:209-244` | Safe init enters standby monitor, enables ADC for observation. |
| `App/App_SC8815.c:266-274` | `App_SC8815_RequestCharge()` is the only public path to leave standby. |
| `Int/Int_SC8815.h:7-17` | Status enum and guard/state/range errors. |
| `Int/Int_SC8815.h:49-109` | Public INT API for safe state, CE/PSTOP, register access, status, ADC, current limits. |
| `Int/Int_SC8815.c:6-39` | GPIO defaults for software IIC, CE_N, PSTOP, delay cycles. |
| `Int/Int_SC8815.c:49-153` | Bit-banged IIC primitive functions. |
| `Int/Int_SC8815.c:235-402` | Guarded register write policy. |
| `Int/Int_SC8815.c:480-509` | PSTOP/CE_N writes. |
| `Int/Int_SC8815_BSP.h:497-566` | Project ratios, external divider values, safe/forbidden register masks. |
| `docs/logic/hardware_interface_reservation.md:90-113` | SC8815 interface and boundary: software IIC, INT, CE, PSTOP, hardware-managed charging. |

## Other Modules

| Evidence | What it supports |
| --- | --- |
| `Com/Com_BQ76952.h:22-39` | Pack/cell constants and charge/discharge targets. |
| `Com/Com_BQ76952.c:10-32,50-80` | Pure OCV/SOC conversion table and voltage interpolation. |
| `Com/Com_BQ76952.c:32-47` | 0.1K to Celsius conversion. |
| `App/App_OLED.c:6-13` | OLED APP scope: bring-up status page. |
| `App/App_OLED.c:56-75` | OLED page render content. |
| `App/App_OLED.c:81-143` | OLED init and update APIs avoid duplicate refresh. |
| `Int/Int_OLED.h:8-30` | OLED I2C address `0x78` and display API. |
| `Int/Int_OLED.c:4-12` | OLED GRAM and I2C write implementation on `hi2c2`. |
| `Int/Int_OLED.c:86-118,548-585` | OLED refresh, clear, and init sequence. |
| `Int/Int_EEPROM.h:7-9,22-65` | EEPROM size/page/address and API responsibilities. |
| `Int/Int_EEPROM.c:27-45,84-207` | Range/page handling, read/write, page split, readback verification. |
| `Int/Int_CanFd.h:39-75` | CAN FD init/send/receive/status API. |
| `Int/Int_CanFd.c:153-199` | CAN FD filter/global filter/start. |
| `Int/Int_CanFd.c:232-320` | CAN FD send/receive validation and HAL bridge. |
| `Int/Int_Button.c:18,63-98` | Button debounce period and task-driven debounce. |
| `Int/Int_Buzzer.c:101-169` | Non-blocking buzzer start/task behavior. |
| `Int/Int_Led.h:14-17` | LED active-low hardware note. |
| `Int/Int_Led.c:40-113` | LED init and active-low set/read implementation. |

## Corpus Skill Baseline

| Evidence | What it supports |
| --- | --- |
| `AGENTS.md:1-71` | Project-level Codex evidence, scope, output, and style rules. |
| `C:/Users/lst/.codex/skills/sgg-embedded-project-style-guide/SKILL.md` | User corpus is a style/reference set, not fine-tuning. |
| `C:/Users/lst/.codex/skills/sgg-embedded-project-style-guide/references/code-style.md` | Corpus style: preserve local layout, pair `.c/.h`, keep vendor code separate, use fixed-width types, Chinese comments for hardware intent. |
| `C:/Users/lst/.codex/skills/sgg-embedded-project-style-guide/references/architecture-rules.md` | Corpus architecture: HAL/vendor bottom, driver/interface middle, APP/state-machine top. |
| `C:/Users/lst/.codex/skills/sgg-embedded-project-style-guide/references/review-checklist.md` | Corpus review checklist for embedded projects. |
