# Review Checklist

Use this checklist for code generation and review in this repository.

## Evidence

- Every non-trivial conclusion cites file path and line number.
- Missing evidence is marked `Unknown`.
- Conflicting evidence is marked `Conflict`.
- Facts, inferences, and guesses are not mixed.

Evidence rule: `AGENTS.md:9-31`.

## Project Fit

- New code matches the current `App/`, `Com/`, `Int/`, `Core/` layering.
- `main.c` remains focused on init and scheduling.
- Generated/vendor HAL files are not used for product behavior.
- New source files are added to Keil `.uvprojx` when needed.

Evidence: `bms24v_platform/Core/Src/main.c:132-169`; `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx:343,450-710`.

## Naming And Style

- APP APIs use `App_<Feature>_`.
- Interface APIs use `Int_<Device>_`.
- Pure helpers use `Com_<Domain>_`.
- File-scope state uses `s_`.
- Types use module-specific `TypeDef`.
- Register maps and safety masks live in `*_BSP.h`.
- Chinese comments explain hardware timing, protocol fields, calibration, concurrency, and safety intent.

Evidence: `03_coding_conventions.md`.

## Hardware-Software Consistency

Check each changed hardware resource against:

- `.ioc`
- generated init/MSP source
- INT/BSP macros
- APP caller
- hardware rules/docs
- netlist/PDF where available

Must check:

- SC8815 PA6/PA7 software IIC, PA5 INT, PB1 CE_N, PB0 PSTOP.
- BQ I2C1 PB6/PB7, PB4 ALERT, PB3 WAKE.
- OLED/EEPROM I2C2 PA11/PA12.
- FDCAN PB8/PB9 timing.
- USART1 PA9/PA10.
- Flash/RAM/stack/heap.

Evidence: `04_hardware_software_matrix.csv`.

## BQ76952 Safety

- Do not reuse old BQ76930 register tables or formulas.
- Keep direct/subcommand/Data Memory/CRC behavior in `Int_BQ76952`.
- Keep pure conversions in `Com_BQ76952`.
- Keep BQ workflow in `App_BatMan`.
- Confirm ConfigUpdate enter/exit around Data Memory writes.
- Confirm Vcell Mode `0x8923` and six direct commands with hardware.
- Confirm CRC default before hardening.
- Confirm CC2 current sign/gain before SOC integration changes.
- Confirm FET status/request mapping before changing FET behavior.

Evidence: `Int/Int_BQ76952.h:20-122`; `App/App_BatMan.c:807-928`; `official_chip_docs_files/BQ76930_to_BQ76952_逻辑替换设计说明.md:418-438,1150-1156`.

## SC8815 Safety

- Do not use hardware I2C3.
- Do not bypass `Int_SC8815_GuardWrite`.
- Do not release PSTOP except through the APP charge request flow.
- Do not enable reverse/OTG behavior unless explicitly reviewed.
- Confirm R17/R18/FB/ADIN actual hardware before enabling charge.
- Confirm SC8815 INT strategy: polling-only or EXTI flag.

Evidence: `App/App_SC8815.c:72-144,266-274`; `Int/Int_SC8815.c:49-153,235-402`; `docs/rules/hardware_rules.md:31-59`; `08_conflicts_and_unknowns.md`.

## Error Handling

- Check return values from HAL/INT calls that affect safety or required bring-up.
- Distinguish parameter/range/state/timeout/CRC/checksum errors.
- Avoid swallowing init errors unless the module is optional and documented.
- Avoid infinite waits without timeout.

Evidence: `Int/Int_BQ76952.h:7-17`; `Int/Int_SC8815.h:7-17`; `Int/Int_EEPROM.c:59-81,117-207`; conflicts C-007/C-008 in `08_conflicts_and_unknowns.md`.

## Concurrency

- Keep ISRs short.
- Do not call I2C or SC software IIC from ISR.
- Keep task functions bounded and callable from the bare-metal loop.
- If RTOS is added, define bus mutex ownership and task priorities.

Evidence: `05_rtos_concurrency_model.md`.

## Buffers And Bounds

- Check transfer lengths and fixed-size buffers.
- Check CAN DLC conversion.
- Check EEPROM range/page boundaries.
- Check OLED coordinates before adding new drawing paths.

Evidence: `Int/Int_BQ76952.c:104-116,144-230`; `Int/Int_CanFd.c:29-91`; `Int/Int_EEPROM.c:27-45,117-207`; `Int/Int_OLED.c:131-321`.

## Build Hygiene

- Keep `.ioc`, `.uvprojx`, startup, and scatter files as source metadata.
- Do not treat Keil build outputs (`.o/.obj/.axf/.hex/.map/.crf/.d/.dep/.lnp`) as source/training inputs.
- Add or update ignore rules if build outputs appear in source review.

Evidence: `AGENTS.md:54-56`; `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx:385-710`.

## Manual Bring-Up Gates

Before enabling real charging or relying on protection behavior:

1. Confirm SC8815 divider/solder state and PA6/PA7 software IIC waveform.
2. Confirm SC8815 STATUS/ADC readings in standby monitor.
3. Confirm BQ DeviceNumber and CRC mode.
4. Confirm BQ Vcell Mode readback and six cell voltages against a meter.
5. Confirm BQ current direction/gain with known charge/discharge current.
6. Confirm TS/DCHG/DFETOFF temperature readbacks.
7. Confirm FET enable/request/status behavior.
8. Confirm no unexpected heat on balancing channels.

Evidence: `08_conflicts_and_unknowns.md`; `official_chip_docs_files/BQ76930_to_BQ76952_逻辑替换设计说明.md:1056-1119`.
