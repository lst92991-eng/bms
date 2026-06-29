# Change Impact Playbook

Use this playbook when asking Codex to modify the project. Every answer should cite the affected files and relevant evidence.

## General Workflow

1. Identify whether the change is hardware, protocol, app behavior, or build-only.
2. Read the relevant hardware docs, `.ioc`, generated init, INT layer, APP caller, and Keil project membership.
3. Mark conflicts and unknowns before editing.
4. Change the smallest owning layer first.
5. Update headers, source, init calls, build metadata, and docs together when needed.
6. Verify by build, static review, or a manual bring-up checklist.

Evidence rule: follow `AGENTS.md:9-31`.

## If A Pin Changes

Check and potentially update:

| Area | Files | Evidence examples |
| --- | --- | --- |
| CubeMX pinmux | `bms24v_platform/bms24v_platform.ioc` | SC/BQ pin lines at `.ioc:101-174` |
| Generated init/MSP | `bms24v_platform/Core/Src/gpio.c`, `i2c.c`, `fdcan.c`, `usart.c` | `gpio.c:42-118`, `i2c.c:139-179`, `fdcan.c:93-101`, `usart.c:99-106` |
| BSP macros | `Int/*_BSP.h` or module fallback macros | `Int/Int_BQ76952.c:10-27`; `Int/Int_SC8815.c:6-35` |
| APP assumptions | `App_*` files | `App/App_SC8815.c:33-42`; `App/App_BatMan.c:807-907` |
| Docs | `docs/rules/hardware_rules.md`, `04_hardware_software_matrix.csv` | `docs/rules/hardware_rules.md:31-109` |

Special rule: SC8815 PA6/PA7 must remain software IIC unless hardware evidence changes.

Evidence: `official_chip_docs_files/README.md:1`; `docs/rules/hardware_rules.md:35-37`; `Int/Int_SC8815.c:49-153`.

## If BQ76952 Logic Changes

Check:

- `Int/Int_BQ76952_BSP.h` for direct commands, subcommands, Data Memory addresses, masks.
- `Int/Int_BQ76952.c` for CRC, checksum, length, ConfigUpdate, direct/subcommand transport.
- `App/App_BatMan.c` for initialization order, sampling, SOC, health, FET enable.
- `Com/Com_BQ76952.c` only for pure conversions.
- Migration doc for old-to-new BQ constraints.

Evidence: `Int/Int_BQ76952.h:20-122`, `Int/Int_BQ76952.c:57-104,144-230,328-614,639-698`, `App/App_BatMan.c:243-345,417-528,807-928`, `official_chip_docs_files/BQ76930_to_BQ76952_逻辑替换设计说明.md:316-438`.

Must confirm before production:

- CRC default.
- Vcell Mode readback.
- CC2 current sign/gain.
- TS1/TS3/DCHG/DFETOFF temperature model.
- FET status/request mapping.
- Protection thresholds and delays.

Evidence: `08_conflicts_and_unknowns.md`.

## If SC8815 Logic Changes

Check:

- `Int/Int_SC8815_BSP.h` for register masks, ratios, safe/forbidden project values.
- `Int/Int_SC8815.c` for software IIC and guard writes.
- `App/App_SC8815.c` for standby monitor and charge request state machine.
- `docs/rules/hardware_rules.md` for SC limits and forbidden behaviors.

Evidence: `Int/Int_SC8815_BSP.h:497-566`, `Int/Int_SC8815.c:49-153,235-509`, `App/App_SC8815.c:33-42,72-144,209-274`, `docs/rules/hardware_rules.md:31-59`.

Do not:

- Use hardware I2C3.
- Release PSTOP outside `App_SC8815_RequestCharge()` flow.
- Disable OVP/short-foldback or enable reverse/OTG behavior unless explicitly reviewed.
- Trust divider/current values until R17/R18/FB/ADIN are confirmed.

## If CAN Protocol Is Added

Check:

- `Int/Int_CanFd.h/.c` for transport framing and DLC limits.
- `fdcan.c` and `.ioc` for timing and mode.
- Add a `Com_*` or protocol module for message IDs, endian, scaling, and checksum if needed.
- Add APP calls only after protocol ownership exists.

Evidence: `Int/Int_CanFd.h:39-75`, `Int/Int_CanFd.c:29-91,153-320`, `bms24v_platform/Core/Src/fdcan.c:40-58`.

Unknown: CAN ICD/message database not found.

## If EEPROM Storage Is Added

Check:

- `Int/Int_EEPROM.h/.c` for size, page, address, read/write/readback.
- Define an APP/COM-level schema with magic, version, length, CRC, and migration.
- Account for I2C2 sharing with OLED.

Evidence: `Int/Int_EEPROM.h:7-65`, `Int/Int_EEPROM.c:27-45,117-207`, `Int/Int_OLED.c:12`.

## If OLED UI Expands

Check:

- `App/App_OLED.c` for app page ownership.
- `Int/Int_OLED.c` for GRAM, coordinate bounds, refresh cost.
- I2C2 sharing with EEPROM.

Evidence: `App/App_OLED.c:56-143`, `Int/Int_OLED.c:4,86-118,548-585`.

Risk: current OLED code ignores HAL transmit return in byte writes (`Int/Int_OLED.c:7-12`).

## If RTOS Is Added

Required updates:

- Document task list, priorities, stack sizes, queue/mutex ownership.
- Protect I2C1/I2C2 and shared state.
- Remove blocking or long loops from time-critical contexts.
- Replace main loop periodic scheduler with task/timer design.
- Update `05_rtos_concurrency_model.md`.

Evidence for current non-RTOS baseline: `bms24v_platform/Core/Src/main.c:148-169`.

## Golden Review Questions

Use these to test whether Codex understood the project:

1. Which BQ direct commands map to the six physical cells?
2. Why must SC8815 use PA6/PA7 software IIC instead of hardware I2C3?
3. What must happen before `PSTOP` is released?
4. Which file owns BQ Data Memory addresses and bit masks?
5. Which file owns pure SOC/temperature conversion helpers?
6. Is the project currently RTOS-based?
7. What is the Flash/RAM layout?
8. Which init return values are currently ignored in `main.c`?
9. Which conflicts block safe BQ communication?
10. Which unknowns block safe charging?

Every answer must cite file paths and line numbers.
