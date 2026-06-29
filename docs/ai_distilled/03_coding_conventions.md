# Coding Conventions

These conventions are inferred from the current repository plus the local embedded corpus skill. Current repository evidence wins over generic rules.

## Naming

| Item | Rule | Evidence | Confidence |
| --- | --- | --- | --- |
| Application modules | Use `App_<Feature>.c/.h` and public functions prefixed `App_<Feature>_` | `App/App_BatMan.h:24-32`; `App/App_SC8815.h:31-51`; `App/App_OLED.h:7-9` | High |
| Interface/driver modules | Use `Int_<Device>.c/.h` and public functions prefixed `Int_<Device>_` | `Int/Int_BQ76952.h:20-122`; `Int/Int_SC8815.h:49-109`; `Int/Int_CanFd.h:39-75` | High |
| BSP/register headers | Put chip register addresses, masks, hardware constants, and project safe masks in `Int_<Device>_BSP.h` | `Int/Int_BQ76952_BSP.h:210-313`; `Int/Int_SC8815_BSP.h:497-566` | High |
| Common helpers | Use `Com_<Domain>_` for pure conversion/lookups | `Com/Com_BQ76952.h:56-67`; `Com/Com_BQ76952.c:32-80` | High |
| Static helpers | Prefix static helpers with the owning module name | `App/App_BatMan.c:129-149,206-243`; `Int/Int_CanFd.c:9-91`; `Int/Int_SC8815.c:49-235` | High |
| Local static state | Use `s_` prefix for file-scope state | `App/App_SC8815.c:29`; `Int/Int_BQ76952.c:49-50`; `Int/Int_SC8815.c:47` | High |
| Types | Use `typedef enum/struct` with module-specific `TypeDef` suffix | `Int/Int_BQ76952.h:7-17`; `App/App_SC8815.h:12-28`; `Int/Int_CanFd.h:20-36` | High |
| Macros | Use uppercase module prefixes and explicit units when possible | `App/App_BatMan.h:11-21`; `Int/Int_EEPROM.h:7-9`; `Int/Int_SC8815_BSP.h:459-499` | High |

## File And Directory Organization

1. New product behavior belongs in `App/`.
   Evidence: `App/App_BatMan.c:807-928`, `App/App_SC8815.c:72-144`.

2. New chip/peripheral low-level access belongs in `Int/`, with a paired `.h`.
   Evidence: `Int/Int_BQ76952.h:20-122`, `Int/Int_SC8815.h:49-109`, `Int/Int_EEPROM.h:22-65`.

3. Register maps and safety-critical bit definitions belong in `*_BSP.h`.
   Evidence: `Int/Int_BQ76952_BSP.h:210-313`, `Int/Int_SC8815_BSP.h:497-566`.

4. Pure lookup/conversion code belongs in `Com/`.
   Evidence: `Com/Com_BQ76952.c:10-80`.

5. Generated HAL/CubeMX files should stay focused on initialization and MSP wiring.
   Evidence: `bms24v_platform/Core/Src/gpio.c:42-118`, `bms24v_platform/Core/Src/i2c.c:31-179`, `bms24v_platform/Core/Src/fdcan.c:40-101`.

## Include Style

- `main.c` includes generated peripheral headers first, then local app/int headers.
  Evidence: `bms24v_platform/Core/Src/main.c:20-38`.

- APP modules include their own header, then needed lower-layer headers.
  Evidence: `App/App_SC8815.c:1-6`, `App/App_BatMan.c:1-8`.

- INT modules include their own header and BSP/header dependencies.
  Evidence: `Int/Int_SC8815.c:1-3`, `Int/Int_BQ76952.c:1-6`.

Confidence: High.

## Error Handling

1. INT modules should return explicit module status enums, not raw HAL statuses.
   Evidence: `Int/Int_BQ76952.h:7-17`, `Int/Int_SC8815.h:7-17`, `Int/Int_CanFd.h:14-20`, `Int/Int_EEPROM.h:13-19`.

2. Parameter, range, state, CRC/checksum, timeout, busy, and length failures should be distinct when meaningful.
   Evidence: `Int/Int_BQ76952.h:9-16`, `Int/Int_SC8815.h:9-16`, `Int/Int_CanFd.h:14-19`.

3. APP may convert lower-level status to boolean state or fault flags, but should preserve logs for bring-up.
   Evidence: `App/App_SC8815.c:52-60`, `App/App_BatMan.c:827-847`.

4. Do not ignore safety-critical init returns without an explicit decision.
   Current exceptions: `main.c` ignores CAN FD and EEPROM init returns (`bms24v_platform/Core/Src/main.c:135-136`), listed as conflicts C-007/C-008.

## Logging And Diagnostics

- Current bring-up style uses direct `printf`.
  Evidence: `App/App_SC8815.c:244`, `App/App_BatMan.c:827-847,776-797`, `bms24v_platform/Core/Src/main.c:131`.

- OLED is used as a compact bring-up status page for BQ I2C/Power Config, not a full UI.
  Evidence: `App/App_OLED.c:6-13,56-75`.

- No general logging abstraction or assert policy was found.
  Evidence: absence in scanned modules; `Error_Handler()` loops forever at `bms24v_platform/Core/Src/main.c:254-259`.

Confidence: Medium for absence until a full repository-wide audit.

## Memory And Allocation

- The current code uses static/global state and stack locals; no heap allocation pattern was identified in scanned active modules.
  Evidence: `App/App_SC8815.c:29`, `Int/Int_BQ76952.c:49-50`, `Int/Int_OLED.c:4`, `Com/Com_BQ76952.c:16`, startup heap exists at `bms24v_platform/MDK-ARM/startup_stm32g0b1xx.s:42`.

- For protocols/register transfers, use fixed-size bounded buffers and length checks.
  Evidence: `Int/Int_BQ76952.c:44-45,104-116,144-230`, `Int/Int_CanFd.c:29-91,232-320`, `Int/Int_EEPROM.c:27-45,166-207`.

Confidence: High for current scanned code.

## Concurrency

- Current project is bare-metal and task-like functions are called from the main loop.
  Evidence: `bms24v_platform/Core/Src/main.c:148-169`.

- Interrupt handlers should stay short. Current BQ EXTI handler only calls HAL GPIO EXTI handler.
  Evidence: `bms24v_platform/Core/Src/stm32g0xx_it.c:107-112`.

- Hardware docs require ISR-to-task handoff for BQ/SC interrupts.
  Evidence: `docs/rules/hardware_rules.md:41`, `docs/logic/hardware_interface_reservation.md:133`.

## State Machine Rules

1. Safety defaults should be explicit and conservative.
   Evidence: SC8815 enters standby monitor and keeps PSTOP high (`App/App_SC8815.c:33-42,209-244`).

2. Leaving a safe state should happen through a single public request API.
   Evidence: `App_SC8815_RequestCharge()` is documented as the only public path to leave standby (`App/App_SC8815.c:266-274`).

3. BQ Data Memory writes must be inside ConfigUpdate and followed by exit/readback where needed.
   Evidence: `App/App_BatMan.c:857-884`, `Int/Int_BQ76952.c:639-698`.

4. FET enable/request must distinguish "allow BQ logic" from "force MOS on".
   Evidence: `App/App_BatMan.c:321-345`.

## Comments

- Use concise Chinese comments for hardware behavior, timing, safety, protocol/register meanings, and bring-up assumptions.
  Evidence: `App/App_BatMan.c:16-23,237-243,321-345,797-805`, `App/App_SC8815.c:33-42,64-70,103-139`, `Int/Int_BQ76952_BSP.h:210-237`, `Int/Int_SC8815_BSP.h:497-566`.

- Avoid comments that only restate trivial assignments; prefer comments that explain why a sequence is safe or required.
  Evidence: BQ and SC APP comments focus on FET ownership, ConfigUpdate ordering, PSTOP safety, guarded register writes.

## Forbidden Or High-Risk Patterns

| Pattern | Reason | Evidence |
| --- | --- | --- |
| Using hardware I2C3 for SC8815 | Hardware notes say IIC3 lines are reversed/unusable; current code bit-bangs PA6/PA7 | `official_chip_docs_files/README.md:1`; `docs/rules/hardware_rules.md:35-37`; `Int/Int_SC8815.c:49-153` |
| APP directly forcing BQ CHG/DSG MOS on | Current APP delegates decisions to BQ FET logic after FET_ENABLE | `App/App_BatMan.c:16-23,321-345` |
| Writing SC8815 power-loop registers outside guarded path | `Int_SC8815_GuardWrite` blocks readonly/reserved/forbidden/state-sensitive writes | `Int/Int_SC8815.c:235-402` |
| Reusing old BQ76930 register maps/formulas | Migration doc forbids old BQ76930 register/formula inheritance | `official_chip_docs_files/BQ76930_to_BQ76952_逻辑替换设计说明.md:17-22,66-78` |
| Adding behavior to vendor HAL driver files | Local project separates generated Core/Drivers from App/Com/Int | `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx:395-710` |
| Silent source/config/doc conflict resolution | `AGENTS.md` requires `Conflict` and separate sources | `AGENTS.md:9-17` |
