# RTOS And Concurrency Model

## Current Model

The active firmware is bare-metal. There is no active evidence of FreeRTOS task creation, scheduler start, queues, mutexes, or timers in the scanned current project path. The main loop uses `HAL_GetTick()` and elapsed-time checks to call module task functions.

Evidence: `bms24v_platform/Core/Src/main.c:148-169`.  
Confidence: High for active code scanned. Human confirmation: Not needed unless another build target exists.

## Scheduler

| Periodic work | Trigger | Evidence | Notes |
| --- | --- | --- | --- |
| Button and buzzer tasks | `BOARD_IO_TASK_PERIOD_MS` elapsed | `bms24v_platform/Core/Src/main.c:153-156` | Polling debounce and non-blocking buzzer timing depend on call cadence |
| BQ battery manager | `APP_BATMAN_TASK_PERIOD_MS` elapsed | `bms24v_platform/Core/Src/main.c:159-162`; `App/App_BatMan.c:918-928` | Samples BQ, updates SOC and health |
| SC8815 app | `APP_SC8815_TASK_PERIOD_MS` elapsed | `bms24v_platform/Core/Src/main.c:164-166`; `App/App_SC8815.c:252-261` | Samples SC status/ADC and applies charge request |
| Idle delay | Every loop | `bms24v_platform/Core/Src/main.c:169` | `HAL_Delay(10u)` defines coarse responsiveness |

## Interrupts

| Interrupt | Current handler | Evidence | Ownership |
| --- | --- | --- | --- |
| EXTI4_15 | Calls `HAL_GPIO_EXTI_IRQHandler(BQ_INT_Pin)` | `bms24v_platform/Core/Src/stm32g0xx_it.c:107-112` | BQ ALERT line is configured, but no app callback/flag evidence was found |
| TIM14 | Calls `HAL_TIM_IRQHandler(&htim14)` | `bms24v_platform/Core/Src/stm32g0xx_it.c:121-126` | HAL timebase |
| SC8815 INT PA5 | Input-only GPIO | `bms24v_platform/Core/Src/gpio.c:64-68` | Conflict with docs expecting EXTI flag handoff |
| FDCAN/I2C/USART | No active IRQ use found | `Int/Int_CanFd.c:232-320`; `Int/Int_BQ76952.c:328-485`; `bms24v_platform/Core/Src/usart.c:41-52` | Current usage appears polling/blocking |

## Shared State

| State | Owner | Access pattern | Evidence | Risk |
| --- | --- | --- | --- | --- |
| `s_sc` | `App_SC8815.c` | File-static, exposed as read-only pointer | `App/App_SC8815.c:25-29,281-283` | Caller must not modify through cast |
| BQ CRC/HAL error state | `Int_BQ76952.c` | File-static, accessors for CRC and HAL error | `Int/Int_BQ76952.c:49-54,286-323` | If ISR access is added, protect access |
| SC8815 standby state | `Int_SC8815.c` | File-static, used by guard checks | `Int/Int_SC8815.c:47,235-402` | Must stay consistent with actual PSTOP |
| OLED GRAM | `Int_OLED.c` | Global buffer | `Int/Int_OLED.c:4` | No lock; only safe in current single-thread loop |
| Button state | `Int_Button.c` | File-static debounce state | `Int/Int_Button.c:18-29,63-139` | Task cadence affects event capture |
| Buzzer state | `Int_Buzzer.c` | File-static state toggled from task | `Int/Int_Buzzer.c:101-169` | Blocking tone API can stall loop |

## Concurrency Rules

1. Do not block in interrupt handlers.
   Evidence: project docs require ISR only set flags for SC/BQ (`docs/rules/hardware_rules.md:41`; `docs/logic/hardware_interface_reservation.md:133`), and current EXTI handler only delegates to HAL (`bms24v_platform/Core/Src/stm32g0xx_it.c:107-112`).

2. Do not call I2C/SC software IIC from ISR.
   Evidence: `docs/rules/hardware_rules.md:41`; `docs/logic/hardware_interface_reservation.md:133`; BQ/SC drivers are blocking/polling style (`Int/Int_BQ76952.c:328-485`; `Int/Int_SC8815.c:49-153`).

3. Keep task APIs idempotent and bounded.
   Evidence: `App_BatMan_Task` samples/updates once per call (`App/App_BatMan.c:918-928`); `App_SC8815_Task` samples/applies/prints once per call (`App/App_SC8815.c:252-261`).

4. If RTOS is later added, document task ownership of each bus.
   Evidence: current I2C2 is shared by OLED and EEPROM (`bms24v_platform/Core/Src/i2c.c:74-107`; `Int/Int_OLED.c:12`; `Int/Int_EEPROM.c:7-23`), with no mutex evidence.

## Unknowns

- No mutex/critical-section design for shared I2C2 if RTOS is added.
- No ISR callback body for BQ ALERT was found.
- No SC8815 INT flag path exists yet.
- No watchdog feed path exists.

See `08_conflicts_and_unknowns.md`.
