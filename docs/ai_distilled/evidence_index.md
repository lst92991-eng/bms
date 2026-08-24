# Evidence Index

更新日期：2026-08-22
所有相对路径均以 `C:\Users\lst\Desktop\new_bms` 为根。行号在本轮文档完成前按共享工作区重新抓取；若后续源码继续改动，应以冻结 commit 重新生成本索引。

## Evidence semantics

- 源码行证明“当前实现写了什么”，不自动证明真实硬件行为。
- `.ioc`/生成文件证明 MCU 配置；硬件连接仍需原理图/网表/实物交叉确认。
- 规则/人工确认证明项目约定；与源码冲突时记录 `Conflict`，不能静默选边。
- 主机单测和 Python contract 证明算法输入输出或结构约束；不能证明电气时序、gate、电流、温升、EMC、掉电和器件容差。
- 历史板测只证明当时的板号/镜像/条件；没有 commit SHA 对应时不能覆盖当前重构。

## E-BOOT — startup and scheduler

| ID | Evidence | Supports | Confidence | Human confirmation |
| --- | --- | --- | --- | --- |
| E-BOOT-01 | `bms24v_platform/Core/Src/main.c:83-128` | `HAL_Init()` 前直接建立 SC 安全 GPIO；时钟后 USER SysInit 先 GPIO→SC ForceStandby→I2C1→BQ all-off，再执行其余 Cube 外设/日志并把 BQ early-safe evidence 传给 `App_Main()` | High | Needed，reset GPIO/gate waveform |
| E-BOOT-01A | `Int/Int_SC8815.c:21-46` | 不依赖 HAL/SystemClock/Cube GPIO 初始化，直接把 PB0=PSTOP、PB1=CE_N 配为高电平安全输出 | High | Needed，reset-to-HAL/SystemClock waveform |
| E-BOOT-02 | `App/App_Main.c:215-267` | 再次 SC ForceStandby/BQ early-safe；Safety 记录两次确认的 AND；各模块初始化顺序 | High | Needed，reset GPIO waveform |
| E-BOOT-03 | `App/App_Main.c:269-377` | 静态创建 Safety/BatMan/SC/CAN/maintenance/buzzer/CLI（工程）并启动 scheduler；失败 Panic | High | Not needed |
| E-BOOT-04 | `App/App_Main.c:25-78` | task period/deadline/priority-related constants、静态 TCB/stack 大小 | High | Needed，实际 stack HWM |
| E-BOOT-05 | `App/App_Main.c:86-105` | absolute release / missed-period catch-up prevention | High | Needed，WCET trace |
| E-BOOT-06 | `bms24v_platform/MDK-ARM/FreeRTOS/include/FreeRTOSConfig.h:19-49` | preemptive/time slicing、mutex、static allocation on、dynamic allocation off | High | Not needed |
| E-BOOT-07 | `bms24v_platform/MDK-ARM/FreeRTOS/include/FreeRTOSConfig.h:58-68` | scheduler state/HWM API、assert routed to `Int_Fault` | High | Not needed |

## E-SAFE — safety supervisor, watchdog, faults

| ID | Evidence | Supports | Confidence | Human confirmation |
| --- | --- | --- | --- | --- |
| E-SAFE-01 | `App/App_Safety.h:12-19,44-65` | critical task IDs and auditable safety snapshot | High | Not needed |
| E-SAFE-02 | `App/App_Safety.h:21-35,85-94` | typed BQ wake evidence and generation/epoch APIs | High | Not needed |
| E-SAFE-03 | `App/App_Safety.c:9-16` | 10 ms supervisor、3 s grace、nominal 6 s watchdog、100 ms stack check、500 ms wake evidence | High | Needed，actual LSI timeout |
| E-SAFE-04 | `App/App_Safety.c:67-112` | setting any inhibit or revoking active wake forces SC PSTOP before changing software credential/epoch | High | Needed，race/latency injection |
| E-SAFE-05 | `App/App_Safety.c:155-211` | deadline and stack high-watermark checks | High | Needed，task/stack fault injection |
| E-SAFE-06 | `App/App_Safety.c:301-386` | BQ/SC events revoke epoch and inhibit power; generic clear cannot erase latched critical BQ/software protection | High | Needed，fault recovery HIL |
| E-SAFE-07 | `App/App_Safety.c:387-465` | one epoch authorizes both BQ main FET and SC start; typed BQ wake provenance/expiry | High | Needed，wake/race HIL |
| E-SAFE-08 | `App/App_Safety.c:469-560` | IWDG starts and refreshes only when supervised health is good | High | Needed，IWDG reset HIL |
| E-SAFE-09 | `Int/Int_Watchdog.c:17-78` | direct IWDG configuration/refresh/status, nominal 32 kHz assumption | High/Medium | Needed，frequency/timing |
| E-SAFE-10 | `Int/Int_Fault.h:7-63` | fault enum/snapshot/reset cause/PC/LR and Panic/HardFault contract | High | Not needed |
| E-SAFE-11 | `Int/Int_Fault.c:63-174` | RTC backup record/checksum and reset/previous record recovery | High | Needed，retention test |
| E-SAFE-12 | `Int/Int_Fault.c:176-215,256-272` | Trip/Panic/HardFault and assert/stack/malloc hooks first request safe output then reset | High | Needed，fault waveform |
| E-SAFE-13 | `bms24v_platform/Core/Src/stm32g0xx_it.c:204-212` | SC/BQ EXTI both force SC standby before event dispatch | High | Needed，IRQ latency |

## E-BQ — BQ76952 transport, config, sampling

| ID | Evidence | Supports | Confidence | Human confirmation |
| --- | --- | --- | --- | --- |
| E-BQ-01 | `Int/Int_BQ76952.c:1-38` | blocking/task-only contract and bounded HAL/direct/command/indirect/config budgets | High | Needed，bus fault HIL |
| E-BQ-02 | `Int/Int_BQ76952.c:48-61,130-240` | static recursive mutex, priority inheritance, absolute deadline and HAL timeout capped by remaining budget | High | Needed，concurrency stress |
| E-BQ-03 | `Int/Int_BQ76952.c:524-539,636-659` | mutex initialization and outer transaction API | High | Not needed |
| E-BQ-04 | `Int/Int_BQ76952.c:585-635` | ALERT ISR side only latches pending/monotonic sequence; task side can acknowledge only the observed sequence; no I2C in ISR | High | Not needed |
| E-BQ-05 | `Int/Int_BQ76952.c:1066-1103` | FET_CONTROL + FET_STATUS readback inside one transaction | High | Needed，gate waveform |
| E-BQ-06 | `Int/Int_BQ76952.c:1105-1195` | ConfigUpdate bounded enter/exit workflow | High | Needed，NACK recovery |
| E-BQ-07 | `Int/Int_BQ76952_BSP.h:12-23,200-208` | address/default non-CRC/expected DeviceNumber/alert configuration | High | Needed，actual Comm Type |
| E-BQ-08 | `Int/Int_BQ76952_BSP.h:64-124` | safety/PF/battery/FET status masks | High | Needed，TRM peer review |
| E-BQ-09 | `Int/Int_BQ76952_BSP.h:130-198,210-249` | subcommands/DM addresses/protection/FET options/6S sparse mapping | High | Needed，parameter approval |
| E-BQ-10 | `App/App_BatMan_Sample.c:10-65` | 100 ms sample budget and staged frame | High | Not needed |
| E-BQ-11 | `App/App_BatMan_Sample.c:301-336` | cell/safety/PF/temp/config/FET invalid fault aggregation | High | Needed，fault injection |
| E-BQ-12 | `App/App_BatMan_Sample.c:352-472` | atomic complete-frame publish and first-error reject; sampling does not mutate config proof | High | Needed，bus fault HIL |
| E-BQ-13 | `App/App_BatMan_Sample.c:228-280,475-530` | TS/thermistor acquisition, temperature validity and frame age/sequence | High | Needed，NTC calibration |
| E-BQ-14 | `App/App_BatMan.c:191-346` | ALERT/protection latch/clear conditions/readiness and PSTOP-first runtime-proof interpretation/enforcement | High | Needed，ALERT/clear HIL |
| E-BQ-15 | `App/App_BatMan.c:455-664` | reset/DeviceNumber/config manifest/readback/alarm check/safe FET-off/first complete frame | High | Needed，cold/warm boot HIL |
| E-BQ-16 | `App/App_BatMan.c:673-695` | periodic vs lower-priority maintenance split | High | Needed，WCET |
| E-BQ-17 | `App/App_BatMan_Config.c:99-310` | DM manifest, config state/fault and expected/actual verification | High | Needed，TRM/BOM approval |
| E-BQ-18 | `App/App_BatMan_Config.c:242-310` | write then expected/actual verify, first-error fail | High | Needed，corruption injection |
| E-BQ-19 | `App/App_BatMan_Config.c:314-665` | desired/commanded/observed FET state; pre-reset commanded-off is committed only after successful command/readback; selective control; BQ owner pre/mid/post Safety-epoch revalidation and all-off fallback | High | Needed，all gate transitions/race injection |
| E-BQ-20 | `App/App_BatMan.c:206-293` | critical ALERT is latched in Safety and closes all BQ FETs in the same BatMan owner cycle; noncritical clear requires stable sequence and deasserted pin inside an IRQ-disabled check/clear section | High | Needed，ALERT-to-gate/resolve-window waveform |
| E-BQ-21 | `App/App_BatMan_Sample.c:281-336,411-472`；`App/App_BatMan.c:294-346,673-684`；`App/App_Safety.c:67-93` | sampler publishes raw reset fingerprint without mutating proof; transport/protocol frame loss or POR/CFGUPDATE/FET_EN first stops SC, then revokes config proof/ready and best-effort closes BQ FETs; any inhibit is PSTOP-first | High | Needed，disconnect/POR HIL |
| E-BQ-22 | `App/App_BatMan_Config.c:75-78,666-811`；`App/App_Power.c:316-467` | one non-interleavable 1500 ms transaction performs all-off→DeviceNumber→manifest rewrite/exit→full verify→safe FET/all-off→final checks, followed by a new complete-frame gate | High | Needed，brownout/reconnect/gate HIL |
| E-BQ-23 | `Int/Int_BQ76952.c:585-635`；`App/App_BatMan.c:191-293`；`tests/host/test_bq_safety_contract.py:67-76,199-224` | BQ ALERT provenance/clear race contract: monotonic ISR sequence, owner sampling, critical same-cycle all-off, and noncritical atomic sequence/pin recheck before clearing inhibit | High（software structure） | Needed，ALERT resolve-window injection and pin/gate waveform |

## E-SC — SC8815 charger

| ID | Evidence | Supports | Confidence | Human confirmation |
| --- | --- | --- | --- | --- |
| E-SC-01 | `Int/Int_SC8815_BSP.h:10-31` | compile-time line swap, recovery/stretch, shunts; aligned IBUS/IBAT ratios=3/6 and default/max=1.5/3 A, 1/5 A | High | Needed，line-order conflict and current calibration |
| E-SC-02 | `Int/Int_SC8815_BSP.h:132-161` | R17/R18=200 k/10 k, 25.2 V nominal external divider and guard masks | High | Needed，DMM/charge cutoff |
| E-SC-03 | `Int/Int_SC8815.c:61-89` | fixed physical SCL/SDA mapping | High | Needed，logic analyzer |
| E-SC-04 | `Int/Int_SC8815.c:107-187` | bounded clock stretch, bus busy check, short GPIO atomic section, 9-pulse recovery | High | Needed，stuck-bus HIL |
| E-SC-05 | `Int/Int_SC8815.c:557-637` | raw transactions do not mask interrupts for entire frame | High | Needed，IRQ timing |
| E-SC-06 | `Int/Int_SC8815.c:642-676` | high-low-high multi-byte ADC coherency | High | Needed，measurement comparison |
| E-SC-07 | `Int/Int_SC8815.c:677-738` | safe init/standby and direct BSRR ForceStandby | High | Needed，reset waveform |
| E-SC-08 | `Int/Int_SC8815.c:771-839,1028-1103` | write/update readback and bounded current limit | High | Needed，limit accuracy |
| E-SC-09 | `Int/Int_SC8815.c:1105-1139` | line probe rejects unexpected order and does not switch runtime mapping | High | Needed，board verification |
| E-SC-10 | `App/App_SC8815.h:7-36` | desired/commanded/observed/emergency/age snapshot | High | Not needed |
| E-SC-11 | `App/App_SC8815.c:10-45` | runtime single-writer and readiness after clean samples | High | Not needed |
| E-SC-12 | `App/App_SC8815.c:105-311` | release manifest and final Safety epoch recheck before PSTOP low | High | Needed，TOCTOU HIL |
| E-SC-13 | `App/App_SC8815.c:313-434` | staged/atomic sample publish and initialization | High | Needed，ADC calibration |
| E-SC-14 | `App/App_SC8815.c:435-620` | INT handling/clean-frame recovery/uncredentialed start denial/emergency stop | High | Needed，INT HIL |
| E-SC-15 | `App/App_SC8815.c:508-661` | runtime current-limit change forces safe state and exposes snapshot | High | Needed，current-step HIL |
| E-SC-16 | `Int/Int_SC8815.c:743-770`；`App/App_SC8815.c:435-505`；`tests/host/test_power_soc_contract.py:70-85` | ISR monotonic event sequence; owner reasserts inhibit/PSTOP, requires two clean samples, then rechecks sequence and clears inhibit/state atomically with IRQ disabled; a changed sequence keeps inhibit and resets clean count | High（software structure） | Needed，resolve-window interrupt injection and INT→PSTOP waveform |

## E-POWER — power state machine and wake

| ID | Evidence | Supports | Confidence | Human confirmation |
| --- | --- | --- | --- | --- |
| E-POWER-01 | `App/App_Power.h:7-69` | state/stop reasons/shutdown provenance/request model | High | Not needed |
| E-POWER-02 | `App/App_Power.c:19-60` | Release BQ wake policy and charge/discharge/temp/anchor thresholds | High | Needed，product approval |
| E-POWER-03 | `App/App_Power.c:62-92,160-205` | desired/commanded/observed/provenance and request APIs | High | Not needed |
| E-POWER-04 | `App/App_Power.c:206-290` | all disable paths stop SC first; one Safety epoch gates BQ/SC; all four authorization/BQ/SC/final-recheck failures call unified all-off rollback; Power performs final post-actuator revalidation | High | Needed，scope/race injection |
| E-POWER-05 | `App/App_Power.c:316-467` | recovery requires full reauthentication/new frame; Release offline wake off; Engineering bounded one-shot | High | Needed，wake HIL |
| E-POWER-06 | `App/App_Power.c:468-528` | temperature hysteresis and event-driven full/empty anchor confirmation | High | Needed，chamber/cycle HIL |
| E-POWER-07 | `App/App_Power.c:586-686` | invalid BQ/config/cell/temp fail-safe and temperature/charge-completion policy | High | Needed，fault injection |
| E-POWER-08 | `App/App_Power.c:672-816` | fault/low/run state transitions | High | Needed，full state-matrix HIL |
| E-POWER-09 | `App/App_Power.c:586-657` | BQ offline/config invalid branches synchronously stop SC, retry BQ all-off each cycle, and never continue release in the recovery cycle | High | Needed，offline/stuck-bus HIL |

## E-SOC — SOC, SOH and NVM

| ID | Evidence | Supports | Confidence | Human confirmation |
| --- | --- | --- | --- | --- |
| E-SOC-01 | `Com/Com_SOC.h:7-40` | algorithm scope, seed sources, explicit full/empty anchors | High | Needed，cell characterization |
| E-SOC-02 | `Com/Com_SOC.h:92-127` | validity/confidence/age and persistence API | High | Not needed |
| E-SOC-03 | `Com/Com_SOC.c` | no unconditional first-frame OCV trust; rest/persistence/coulomb implementation | High | Needed，algorithm validation |
| E-SOH-01 | `Com/Com_SOH.h:7-19` | capacity bounds and pure SOH contract | High | Needed，cycle data |
| E-NVM-01 | `App/App_BatMan_Nvm.c:17-38` | SOH/SOC double-slot addresses, format versions, periods and record magic | High | Needed，EEPROM address |
| E-NVM-02 | `App/App_BatMan_Nvm.c:204-353` | CRC、SOH/SOC encode/decode and wrap-safe sequence comparison | High | Needed，power-cut |
| E-NVM-03 | `App/App_BatMan_Nvm.c:354-539` | slot recovery plus write/readback for SOC/SOH | High | Needed，power-cut/endurance |
| E-NVM-04 | `App/App_BatMan_Nvm.c:540-724,726-894` | bounded reconnect/startup restore, low-priority maintenance and explicit flush | High | Needed，timing/endurance |
| E-NVM-05 | `App/App_BatMan_Nvm.c:49-125,713-894` | static recursive mutex serializes slot/sequence/reconnect/task/flush state | High | Needed，contention/stuck-bus stress |
| E-NVM-06 | `App/App_BatMan_Nvm.c:128-175,726-894` | SOC/SOH/provenance is captured/restored under a short task-scheduling pause; EEPROM I/O occurs after resume | High | Needed，WCET/power-cut HIL |

## E-IO — shared I2C, CAN, UART and UI

| ID | Evidence | Supports | Confidence | Human confirmation |
| --- | --- | --- | --- | --- |
| E-IO-01 | `Int/Int_I2C2Bus.c:7-61` | static recursive I2C2 mutex and pre-scheduler behavior | High | Needed，stuck-bus stress |
| E-IO-02 | `Int/Int_EEPROM.h:7-15` | M24C64 address/page/timing assumptions | High（software） | Needed，part/scan |
| E-IO-03 | `Int/Int_OLED.c:16-17,114-147` | bounded transfer timeout and 8-page bulk refresh under I2C2 lock | High | Needed，bus load |
| E-IO-04 | `App/App_Main.c:178-206` | maintenance serializes BatMan NVM/debug before OLED | High | Needed，WCET |
| E-CAN-01 | `App/App_CanBms.h:7-77` | read-only protocol IDs/commands/flags/API | High | Needed，ICD peer review |
| E-CAN-02 | `App/App_CanBms.c:23-40,186-244` | periods/budgets and atomic snapshot | High | Needed，load test |
| E-CAN-03 | `App/App_CanBms.c:362-550,679-819` | bounded event queue/TX defer/RX budget/reinit/task | High | Needed，bus-off/flood |
| E-CAN-04 | `Int/Int_CanFd.c:83-199` | driver init/filter/recovery/send/receive | High | Needed，transceiver HIL |
| E-FORMAT-01 | `Com/Com_Format.c:15-269`；`Com/Com_Format.h:1-16` | libc-independent bounded formatter; field width/string length capped at 384 and only the project-required integer/string conversion subset is accepted | High | Not needed |
| E-FORMAT-02 | `tests/host/test_algorithms.c:376-408` | formatter success, truncation, invalid-format and bound cases | High（host logic） | Not needed |
| E-LOG-01 | `Int/Int_Log.c:11-24,47-173` | fixed 384-byte stack formatter buffer, static 1024-byte UART IT ring, whole-block drop on full; application calls format through `Com_FormatV` | High | Needed，saturation/stack watermark |
| E-LOG-02 | `Int/Int_Log.c:175-231` | error/drop stats, UART error recovery and Tx completion | High | Needed，HAL_BUSY/HAL_ERROR injection |
| E-LOG-03 | `Int/Int_Log.h:16-41`；`bms24v_platform/Core/Src/retarget.c:1-52` | application logging API is `Int_Log_Printf`; retarget remains only as a compatibility source path and Release symbol policy rejects stdio/heap linkage | High | Needed，frozen-ELF symbol audit |
| E-CLI-01 | `App/App_DebugCli.h:7-20` | Release/Engineering macro and physical enable default false | High | Needed，build override audit |
| E-CLI-02 | `App/App_DebugCli.c:3,26-35,236-264` | Engineering compile gate, 60 s unlock and physical gate | High | Needed，physical gate HIL |
| E-CLI-03 | `App/App_DebugCli.c:322-405` | shutdown/fault/charge commands go through restricted Power interface; probes denied | High | Needed，command HIL |
| E-CLI-04 | `App/App_DebugCli.c:446-496,569-589` | task/gate timeout and Release stubs | High | Not needed |

## E-HWCFG — CubeMX and hardware-software pins

| ID | Evidence | Supports | Confidence | Human confirmation |
| --- | --- | --- | --- | --- |
| E-HW-01 | `bms24v_platform/Core/Inc/main.h:60-79` | SC INT/software I2C/PSTOP/CE_N/buzzer/button/BQ INT pin macros; no independent BQ main-FET GPIO | High | Needed，schematic cross-check |
| E-HW-02 | `bms24v_platform/Core/Src/gpio.c:54-111` | startup output levels、SC EXTI/open-drain lines、button/BQ EXTI and IRQ priority | High | Needed，reset waveform |
| E-HW-03 | `bms24v_platform/Core/Src/i2c.c:41-107,139-179` | I2C1/I2C2 timing/filter and pin alternate functions | High | Needed，rise time |
| E-HW-04 | `bms24v_platform/Core/Src/fdcan.c:40-58,93-101` | FD no BRS timing and PB8/PB9 | High | Needed，bus timing |
| E-HW-05 | `bms24v_platform/Core/Src/usart.c:41-52,99-111` | USART1 115200 8N1 PA9/PA10 and IRQ | High | Not needed |
| E-HW-06 | `bms24v_platform/Core/Src/tim.c:44-47,110-117` | TIM3 CH2 buzzer timing and PB5 | High | Needed，audio/PWM check |
| E-HW-07 | `bms24v_platform/bms24v_platform.ioc:5-30,104-156,221-268` | CubeMX bus/pin/clock labels and timing source | High | Needed，regenerate diff |
| E-HW-08 | `docs/rules/hardware_rules.md:20-30,32-87,90-114` | product/BQ/SC/OLED/EEPROM/CAN/UART hardware rules and known unknowns | High | Needed，BOM approval |
| E-HW-09 | `docs/wordflow/manual_confirmations.md:7-12` | dated shunt/divider/CRC/wake/history decisions; watchdog row conflicts current implementation | High | Needed，supersede stale row |
| E-HW-10 | `official_chip_docs_files/BQ76930_to_BQ76952_逻辑替换设计说明.md:182-200` | 6S VC/current/temperature/FET/I2C board mapping | High | Needed，final schematic |
| E-HW-11 | `official_chip_docs_files/full_netlist (4).csv:29-45,186-193` | CFETOFF/unconnected and DFETOFF/DCHG NTC nets; BQ gate nets | Medium/High | Needed，symbol pin/ERC |
| E-HW-12 | `official_chip_docs_files/full_netlist (4).csv:234-241,264-268,302-313` | PDSG/R40 100 Ohm 2 W physical path | High | Needed，thermal HIL |

## E-BUILD — build, tests, CI and memory

| ID | Evidence | Supports | Confidence | Human confirmation |
| --- | --- | --- | --- | --- |
| E-BUILD-01 | `CMakeLists.txt:3-16` | host-test-only configure path | High | Not needed |
| E-BUILD-02 | `CMakeLists.txt:24-35,39-119` | build options, budgets, explicit generated/App/Com/Int/FreeRTOS/HAL sources; includes bounded formatter and no heap implementation | High | Needed，Keil list sync |
| E-BUILD-03 | `CMakeLists.txt:134-165` | Release/Engineering macro, Cortex-M0+, warnings-as-errors and linker map | High | Not needed |
| E-BUILD-04 | `CMakeLists.txt:174-205` | hex/bin, Flash/RAM budget, forbidden-symbol gate and CTest subdirectory | High | Not needed |
| E-BUILD-05 | `CMakePresets.json:5-35,38-75` | three ARM GCC presets and host MSVC preset/test | High | Not needed |
| E-BUILD-06 | `tests/host/CMakeLists.txt:1-55` | four Python structure/repository contracts plus native algorithm/formatter unit registration; ARM configurations run four Python tests, Host runs all five | High | Not needed |
| E-BUILD-07 | `tests/host/test_platform_contract.py:45-226` | platform/static RTOS/early-safe/fault/CLI/build/NVM mutex and snapshot/log-symbol contract assertions | High（structure only） | Not needed |
| E-BUILD-08 | `tests/host/test_bq_safety_contract.py:35-242` | BQ transaction/frame/FET epoch、monotonic ALERT closure、PSTOP-first runtime proof invalidation、all-output rollback、1500 ms full reauthentication and offline all-off contract assertions | High（structure only） | Not needed |
| E-BUILD-09 | `tests/host/test_power_soc_contract.py:38-112` | SC event-sequence closure、Power/wake/SOC/SOH/NVM and current-limit contract assertions | High（structure only） | Not needed |
| E-BUILD-10 | `tests/host/test_algorithms.c:71-429` | OCV/rest seed/coulomb/NVM/anchor/SOH/formatter unit cases | High（host logic） | Needed，target/HIL validation |
| E-BUILD-11 | `.github/workflows/firmware-ci.yml:8-36` | host build/test and three ARM GCC build/test matrix | High（configuration） | Needed，remote green run/SHA |
| E-BUILD-12 | `tests/host/check_size_budget.py:10-34` | ELF Flash/RAM size parsing and fail-on-over-budget | High | Not needed |
| E-MEM-01 | `bms24v_platform/gcc/STM32G0B1CBTx_FLASH.ld:3-9` | 128 KiB Flash, 144 KiB RAM, 0x200 heap, 0x400 stack reservation | High | Needed，map/watermark |
| E-MEM-02 | `bms24v_platform/MDK-ARM/startup_stm32g0b1xx.s:31-60` | Keil startup stack/heap/vector definitions | High | Needed，Keil map |
| E-BUILD-13 | `.gitignore:1-41` | ignored build/log/`.log.lock`/binary/cache/IDE-user classes | High | Not needed |
| E-BUILD-14 | `CMakeLists.txt:194-201`；`tests/host/check_forbidden_symbols.py:10-50` | every firmware link runs `nm` policy: reject heap/stdio symbols and require `Com_FormatV`/`Int_Log_Printf`. Final 2026-08-22 Release reported `symbol gate: no heap/stdio symbols; bounded log formatter present`; Flash 68,412/122,880 B, RAM 20,704/135,168 B | High（frozen workspace artifact） | Needed，freeze commit SHA and archive build log/ELF hash |
| E-BUILD-15 | `.github/workflows/firmware-ci.yml:8-36`；`CMakeLists.txt:142-205`；`tests/host/CMakeLists.txt:1-55` | final local gate result: three clean ARM configurations built and each ran 4/4; Host 5/5; 60 C/H formatting checks; 30 changed-C clang-analyzer checks; HardFault objdump and Keil XML/member gates passed; final platform red-team found no code-level P0/P1 | High（frozen workspace run） | Needed，archive exact commands/tool versions/logs and remote CI URL |
| E-BUILD-16 | `tests/host/check_repo_hygiene.py:11-105`；`tests/host/CMakeLists.txt:8-13`；`.gitignore:1-41`；交付提交相对父版本的 deletion manifest | fail-closed Git tracked-path audit is a CTest. Final result: tracked forbidden artifact=0; this delivery version untracks 1098 historical artifacts, while locally needed binaries/`.log.lock` were removed only from the index and remain ignored | High（delivery snapshot） | Not needed；冻结 SHA 的远端 CI 归档仍属于发布流程 |

## E-REVIEW — external review and historical evidence

| ID | Evidence | Supports | Confidence | Human confirmation |
| --- | --- | --- | --- | --- |
| E-REVIEW-01 | `C:\Users\lst\.codex\attachments\1e7520e9-0c7a-4b6e-8a65-b47e7d9a3b9f\pasted-text.txt` | Pro baseline findings P0-1..P0-4, P1-1..P1-6 and four medium areas | High | Not needed |
| E-REVIEW-02 | `docs/review/pro_remediation_scorecard_2026-08-22.md` | current per-finding conclusion/code/test/status/confidence/human-confirmation mapping | High | Needed，freeze SHA |
| E-REVIEW-03 | `docs/review/board_regression_2026-07-07.md` | historical board observations only; predates current refactor | Medium | Needed，current regression |
| E-REVIEW-04 | `docs/review/bq_transaction_blocking_analysis.md` | current bounded BQ transaction, PSTOP-first runtime proof loss, full reauthentication barrier and residual end-to-end WCET analysis | High | Needed，freeze SHA/HIL |

## Conflicts

| Conflict | Evidence A | Evidence B | Current disposition |
| --- | --- | --- | --- |
| SC physical I2C line order | generated labels/rule: `bms24v_platform/Core/Inc/main.h:63-66`; `docs/rules/hardware_rules.md:37-39` | fixed-swapped driver: `Int/Int_SC8815_BSP.h:19-20`; `Int/Int_SC8815.c:61-89` | Open; logic-analyzer capture then synchronize labels/rules |
| SC divider history | `official_chip_docs_files/README.md:1` | `docs/wordflow/manual_confirmations.md:8`; `Int/Int_SC8815_BSP.h:132-161` | Later confirmation is software baseline; physical DMM still needed |
| BQ shunt | migration/netlist 0.5 mOhm references | `docs/wordflow/manual_confirmations.md:7`; `docs/rules/hardware_rules.md:84` | Software uses 5 mOhm baseline; calibration still needed |
| Watchdog plan | `docs/wordflow/manual_confirmations.md:12` says Deferred | `App/App_Safety.c:469-560`; `Int/Int_Watchdog.c:17-78` | Old plan is stale; implementation exists, HIL pending |

SC ratio/default-limit is no longer a source conflict: `docs/rules/hardware_rules.md:46-60`、`Int/Int_SC8815_BSP.h:21-31` and `tests/host/test_power_soc_contract.py:58-67` now agree. Physical current accuracy remains a calibration/HIL Unknown.

## Unknowns not satisfiable by this evidence index

1. Target-board NTC/Beta/channel mapping and temperature calibration.
2. Final approved BQ/SC protection and current-limit parameter set.
3. Current firmware charge/discharge/short/temperature/pre-discharge/wake/watchdog HIL.
4. Actual IWDG interval, all-task WCET/jitter/stack watermark and fault-to-safe-output latency.
5. Independent MCU main-FET shutdown capability is absent: SC is forced safe before SystemClock, but BQ all-off still waits for clock/I2C1/command; accepting that reset/single-fault exposure or changing hardware requires human decision.
6. Freeze-SHA Keil build/artifacts、remote CI run、MISRA/CERT/coverage reports；当前 changed-C clang analyzer 已通过但未替代全量/标准合规分析。
7. EEPROM/OLED addresses and real power-cut/endurance/stuck-bus behavior.
8. SOC/SOH target-cell characterization and long-off persistence freshness.
9. SC INT 在两帧 clean 到原子 clear 的 resolve-window 中反复触发时，真实 INT→PSTOP 延迟与事件不丢失行为。
10. UART `HAL_BUSY`/`HAL_ERROR`、断线与持续日志洪泛下的丢弃统计、栈水位和 Safety/BatMan task jitter。
