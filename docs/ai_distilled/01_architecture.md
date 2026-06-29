# Architecture

## Layering

The current repository follows a local embedded layering style:

1. `Drivers/` and generated `Core/` provide STM32 HAL/CMSIS, CubeMX init, interrupts, startup, and toolchain metadata.
2. `Int/` owns board/chip/peripheral interfaces and low-level hardware access.
3. `Com/` owns pure calculations or shared conversion helpers.
4. `App/` owns product behavior, state machines, bring-up workflow, and user-visible status.
5. `main.c` wires initialization and periodic scheduling.

Evidence: `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx:395-595`, `bms24v_platform/Core/Src/main.c:31-38,132-169`, `Com/Com_BQ76952.c:10-80`, `App/App_SC8815.c:13-19`, `App/App_BatMan.c:16-23`.  
Confidence: High. Human confirmation: Not needed.

## Boot Flow

Reset starts from the vector table and reset handler in startup assembly, enters C runtime, then `main()` performs HAL/system/peripheral initialization.

Runtime sequence:

1. `HAL_Init()`
2. `SystemClock_Config()`
3. CubeMX peripheral init: GPIO, RTC, FDCAN1, I2C2, USART1, I2C1
4. Local board/app init: LED, buzzer, button, CAN FD, EEPROM, OLED, SC8815, BQ app
5. Bare-metal loop with periodic tasks

Evidence: `bms24v_platform/MDK-ARM/startup_stm32g0b1xx.s:55-60,116-119`, `bms24v_platform/Core/Src/main.c:107,114,121-139,148-169`.  
Confidence: High. Human confirmation: Not needed.

## Application State Machines

### BQ76952

`App_BatMan` is the BQ business flow. It resets state, selects CRC mode, initializes the board interface, resets/probes the BQ, enters ConfigUpdate, writes baseline Data Memory, exits ConfigUpdate, clears startup alarms, enables BQ FET control, then samples battery state and updates SOC/health periodically.

Evidence: `App/App_BatMan.c:807-907,918-928`.  
Confidence: High. Human confirmation: Needed for final Data Memory values and physical calibration.

Boundary rule: APP does not directly force CHG/DSG MOS on; it allows BQ FET logic to run and monitors status.

Evidence: `App/App_BatMan.c:16-23,321-345`.  
Confidence: High. Human confirmation: Needed before production FET policy.

### SC8815

`App_SC8815` keeps SC8815 in standby monitor by default. A charge request is the only public path to leave standby. Before releasing PSTOP, the APP writes guarded project registers and current limits through `Int_SC8815`.

Evidence: `App/App_SC8815.c:33-42,72-144,204-244,266-274`.  
Confidence: High. Human confirmation: Needed before enabling charge on hardware.

Boundary rule: low-level SC8815 driver exposes safe GPIO and guarded register operations; it must not own charging business policy.

Evidence: `Int/Int_SC8815.h:49-109`, `Int/Int_SC8815.c:235-402`, `docs/logic/hardware_interface_reservation.md:98-113`.  
Confidence: High. Human confirmation: Not needed.

## Data And Control Flow

| Flow | Owner | Evidence |
| --- | --- | --- |
| Hardware init | `main.c` + generated `MX_*` | `bms24v_platform/Core/Src/main.c:121-126` |
| Board IO tick | `Int_Button_Task`, `Int_Buzzer_Task` | `bms24v_platform/Core/Src/main.c:155-156` |
| BQ sample/update | `App_BatMan_Task` | `bms24v_platform/Core/Src/main.c:161`; `App/App_BatMan.c:918-928` |
| SC sample/request | `App_SC8815_Task` | `bms24v_platform/Core/Src/main.c:166`; `App/App_SC8815.c:252-261` |
| OLED bring-up output | `App_OLED_Show*` | `App/App_OLED.c:56-75,96-143` |
| EEPROM raw storage | `Int_EEPROM_*` | `Int/Int_EEPROM.h:22-65` |
| CAN FD transport | `Int_CanFd_*` | `Int/Int_CanFd.h:39-75` |

## Interrupt Model

Current active IRQ evidence is minimal:

- BQ INT is configured on PB4 falling edge and routed to `EXTI4_15_IRQHandler()`.
- TIM14 is HAL timebase.
- SC8815 INT is configured as input-only, not EXTI.
- FDCAN, I2C, USART appear polling/blocking from current code evidence.

Evidence: `bms24v_platform/Core/Src/gpio.c:110-118`, `bms24v_platform/Core/Src/stm32g0xx_it.c:107-126`, `bms24v_platform/Core/Src/gpio.c:64-68`, `Int/Int_CanFd.c:232-320`, `Int/Int_BQ76952.c:328-485`.  
Confidence: High. Human confirmation: Needed for intended SC8815 INT and BQ callback design.

## Architecture Rules For Future Code

1. Add BMS product logic to `App_*`, not `main.c`.
   Evidence: `main.c` only wires init/tasks (`bms24v_platform/Core/Src/main.c:132-169`); existing behavior is in `App_BatMan` and `App_SC8815`.

2. Add low-level chip register/transport access to `Int_*`.
   Evidence: BQ direct/subcommand/Data Memory APIs live in `Int/Int_BQ76952.h:73-122`; SC register/status/ADC APIs live in `Int/Int_SC8815.h:65-109`.

3. Put pure conversion helpers in `Com_*`.
   Evidence: `Com/Com_BQ76952.c:10-80` has OCV/SOC and temperature conversion without I2C access.

4. Keep generated HAL files focused on CubeMX/HAL setup.
   Evidence: generated `Core/Src/gpio.c`, `i2c.c`, `fdcan.c`, `usart.c` own pin/peripheral setup; local behavior is grouped separately in `.uvprojx:450-595`.

5. Treat power-path changes as safety work.
   Evidence: BQ FET and SC PSTOP/CE logic are explicitly commented as safety-sensitive in `App/App_BatMan.c:16-23,321-345` and `App/App_SC8815.c:33-42,64-144`.
