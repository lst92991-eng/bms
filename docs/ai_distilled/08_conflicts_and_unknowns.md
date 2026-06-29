# Conflicts And Unknowns

This file separates confirmed conflicts from missing evidence. Do not silently merge these items during code generation.

## Conflicts

| ID | Type | Source A | Source B | Impact | Risk | Manual confirmation |
| --- | --- | --- | --- | --- | --- | --- |
| C-001 | SC8815 bus naming vs implementation | Netlist names SC8815 lines as `MCU_I2C3_SCL_PA7_SC8815` and `MCU_I2C3_SDA_PA6_SC8815` (`official_chip_docs_files/full_netlist (5).csv:10-11`) | Project notes require GPIO software IIC because hardware IIC is reversed/unusable (`official_chip_docs_files/README.md:1`; `docs/rules/hardware_rules.md:35-37`); code uses bit-bang GPIO (`Int/Int_SC8815.c:49-153`) | Any generated SC8815 code must not use hardware I2C3 | High | Scope PA6/PA7 waveform and keep `Int_SC8815` software IIC as authoritative unless hardware revision changes |
| C-002 | BQ76952 CRC default | Hardware rule says BQ76952 default I2C CRC is enabled (`docs/rules/hardware_rules.md:72-73`) | Current app and driver default CRC disabled: `APP_BATMAN_CRC_BOOT_ENABLE (0u)` and `s_bq76952_crc_enabled=false` (`App/App_BatMan.c:28-31`; `Int/Int_BQ76952.c:49`) | Wrong CRC assumption can make BQ communication fail or silently parse wrong frames | High | On board, read/write with both modes and record final default; then update hardware rule or app constant |
| C-003 | BQ WAKE pin direction | Hardware docs describe PB3/BMS_WAKE as BQ wake/TS2 path (`docs/rules/hardware_rules.md:75-76`; `docs/logic/hardware_interface_reservation.md:41`) | CubeMX config makes PB3 `GPIO_Input` (`bms24v_platform/bms24v_platform.ioc:146-150`; `bms24v_platform/Core/Src/gpio.c:98-102`), while `Int_BQ76952_WakeUp()` writes the fallback wake GPIO (`Int/Int_BQ76952.c:14-19,261-267`) | Wake/reset behavior may not work as intended | High | Confirm circuit direction and whether MCU should drive PB3 or only sample it; align GPIO config and BSP |
| C-004 | SC8815 INT expected EXTI vs current input-only | Interface doc says SC8815 INT should be EXTI and ISR only sets flag (`docs/logic/hardware_interface_reservation.md:92,133`) | Current `.ioc`/GPIO configure PA5 as input only (`bms24v_platform/bms24v_platform.ioc:101-104`; `bms24v_platform/Core/Src/gpio.c:64-68`) | SC8815 fault/status events will not wake a handler; task can only poll status if implemented | Medium | Decide whether Phase 1 uses polling only or add EXTI flag path |
| C-005 | Buzzer hardware timer note vs GPIO implementation | Hardware rules note `PB5 / TIM3_CH2` (`docs/rules/hardware_rules.md:108`) | Current code uses GPIO toggling and approximate delay/task timing (`Int/Int_Buzzer.c:5-18,101-169`) | Tone frequency accuracy and CPU load differ from timer PWM assumption | Medium | Confirm if PB5 is available as TIM3_CH2 in current pinmux and whether PWM is required |
| C-006 | Netlist R17/R18 values vs later hardware note | Netlist shows `R17=100Ω` and `R18=0.5mΩ` shunt (`official_chip_docs_files/full_netlist (4).csv:166-169`) | README says `R17,R18` later will be soldered `100K,5.1K` for SC8815 path (`official_chip_docs_files/README.md:1`) | SC8815 voltage/current sensing and safety limits may be wrong if solder state differs | High | Inspect BOM/actual board before enabling charging; record final resistor identities and which circuit each R17/R18 belongs to |
| C-007 | CAN init return ignored | `Int_CanFd_Init()` returns detailed status (`Int/Int_CanFd.h:39-43`; `Int/Int_CanFd.c:153-199`) | `main.c` discards return with `(void)Int_CanFd_Init()` (`bms24v_platform/Core/Src/main.c:135`) | CAN startup failure is invisible except later behavior | Medium | Decide bring-up policy: print error, show OLED/LED fault, or keep best-effort |
| C-008 | EEPROM init return ignored | `Int_EEPROM_Init()` reports online/offline (`Int/Int_EEPROM.c:48-51`) | `main.c` discards return with `(void)Int_EEPROM_Init()` (`bms24v_platform/Core/Src/main.c:136`) | EEPROM absence/failure is not surfaced | Low | Decide whether EEPROM is optional in Phase 1 |

## Unknowns

| ID | Unknown | Evidence gap | Why it matters | Suggested confirmation |
| --- | --- | --- | --- | --- |
| U-001 | RTOS presence | No task creation, scheduler start, queue/mutex, or FreeRTOS project group evidence in active code; main loop is bare-metal (`bms24v_platform/Core/Src/main.c:148-169`) | Concurrency rules differ greatly between bare-metal and RTOS | Treat current project as bare-metal until FreeRTOS files and init path appear |
| U-002 | Watchdog policy | No IWDG/WWDG initialization found in scanned sources; startup only exports weak `WWDG_IRQHandler` (`bms24v_platform/MDK-ARM/startup_stm32g0b1xx.s:152`) | BMS safety and fault recovery should define watchdog feeding | Decide watchdog requirement and add evidence before generating watchdog code |
| U-003 | DMA ownership | `.ioc` has `NVIC.ForceEnableDMAVector=true` (`bms24v_platform/bms24v_platform.ioc:79`), but no active DMA init/use was identified | DMA affects buffers, ISR, memory, and cache assumptions | Search/review CubeMX if DMA later enabled; document channel ownership |
| U-004 | OLED hardware address | Code uses `OLED_I2C_ADDRESS 0x78` (`Int/Int_OLED.h:8-9`); hardware rule identifies SSD1315 but does not independently confirm address (`docs/rules/hardware_rules.md:27`) | Wrong address blocks bring-up display | Scan I2C2 bus or verify SSD1315 module address strap |
| U-005 | SC8815 `FB/ADIN` external connection | Hardware rules list it as open item (`docs/rules/hardware_rules.md:122`) | Charging voltage/current interpretation depends on feedback path | Confirm schematic/PDF and actual resistor network before enabling charge loop |
| U-006 | SC8815 R17/R18 solder state | README says later `100K/5.1K`; current netlist line numbers show different R17/R18 context (`official_chip_docs_files/README.md:1`; `official_chip_docs_files/full_netlist (4).csv:166-169`) | Wrong divider can cause unsafe voltage target | Inspect actual board and update `Int_SC8815_BSP.h` constants |
| U-007 | BQ CC2 current sign, zero, gain | Migration doc explicitly marks CC2 current sign/gain unknown (`official_chip_docs_files/BQ76930_to_BQ76952_逻辑替换设计说明.md:539,691,1150`) | SOC integration and charge/discharge decisions can invert | Apply known small charge/discharge current and log raw/direct values |
| U-008 | BQ TS1/TS3 NTC model and placement | Hardware docs list TS1/TS3 resources but parameter/placement unknown (`official_chip_docs_files/BQ76930_to_BQ76952_逻辑替换设计说明.md:190-193,1152`) | Temperature protection cannot be safely encoded | Confirm BOM, placement, and readback against thermometer |
| U-009 | BQ DCHG/DFETOFF pin configuration | Hardware NTC resources exist but Data Memory config is not final (`official_chip_docs_files/BQ76930_to_BQ76952_逻辑替换设计说明.md:193-194,1153`) | Board/FET/shunt thermal monitoring may be unused | Confirm TRM pin config and direct command units |
| U-010 | BQ RST_SHUT timing | Migration doc marks `BMS_CHIP_SHUT` short/long timing unknown (`official_chip_docs_files/BQ76930_to_BQ76952_逻辑替换设计说明.md:198,1155`) | Wrong timing may reset or ship-mode the pack unexpectedly | Scope `RST_SHUT` and verify TI timing |
| U-011 | BQ FET status bit mapping | Migration doc marks FET status bit parse unknown (`official_chip_docs_files/BQ76930_to_BQ76952_逻辑替换设计说明.md:543-544,1156`) | APP must distinguish request from actual FET state | Toggle safe FET requests and record `FET Status(0x7F)` |
| U-012 | Protection thresholds and delays | Current APP writes a minimal baseline but final protection encoding is not proven (`App/App_BatMan.c:243-317`; migration doc `official_chip_docs_files/BQ76930_to_BQ76952_逻辑替换设计说明.md:536`) | BMS safety behavior depends on exact thresholds | Derive from cell spec, shunt calibration, TRM units, and bench tests |
| U-013 | Cell balancing thermal result | Mapping is defined, but actual heat channel verification is required (`official_chip_docs_files/BQ76930_to_BQ76952_逻辑替换设计说明.md:1106-1109`) | Wrong mask could heat wrong resistor/cell path | Enable one channel at a time with thermal camera or temperature probe |
| U-014 | CI/build command availability | Keil project exists, but no command-line build script evidence was found | Codex verification may be manual only | Add documented build command or MDK batch instructions |
| U-015 | PDF schematic page-level evidence | Current distillation used Markdown/CSV/docs; PDF schematics were not rendered/page-parsed here | Some hardware facts may be second-hand from extracted docs | Render and page-index PDFs for final production audit |
| U-016 | Error reporting policy | `Error_Handler()` loops forever (`bms24v_platform/Core/Src/main.c:254-259`), while many init returns are ignored or printed | Fault visibility is inconsistent | Define Phase 1 policy for OLED/LED/USART fault reporting |
| U-017 | Build artifact hygiene | Keil output folders and generated artifacts are present in workspace state per prior scan, but no `.gitignore` evidence was available | Training/source corpus should not ingest `.obj/.axf/.map` outputs | Add `.gitignore` or clean exported training corpus |
| U-018 | EEPROM data layout | INT layer provides raw bytes/pages only (`Int/Int_EEPROM.h:22-65`) | No parameter schema, CRC, version migration | Define application-level storage schema before use |
| U-019 | CAN protocol | CAN FD transport exists, but no message database/protocol ownership was found (`Int/Int_CanFd.h:39-75`) | IDs, endian, and scaling are unspecified | Add CAN ICD or `Com_*` protocol module before app messages |
| U-020 | Power-path measurable states | Hardware rules list power nets but no ADC/control implementation evidence (`docs/rules/hardware_rules.md:114`) | Software may not know input/output power validity | Add measured signals or explicitly mark power path as hardware-only |

## Working Rule

When generating code, use the following precedence:

1. Current source code and `.ioc` for what is implemented now.
2. `docs/rules/hardware_rules.md` and `docs/logic/hardware_interface_reservation.md` for intended hardware constraints.
3. Official chip docs and migration notes for BQ/SC behavior.
4. Old project/corpus style for naming, layout, and teaching shape only.

If a source conflicts, keep the conflict visible and implement the smallest reversible change.
