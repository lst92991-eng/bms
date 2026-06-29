# Build And Config Matrix

## Build Metadata

| Item | Value | Evidence | Confidence |
| --- | --- | --- | --- |
| IDE/toolchain | MDK-ARM V5.32 | `bms24v_platform/bms24v_platform.ioc:207` | High |
| Keil target | `bms24v_platform` | `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx:10` | High |
| Device | `STM32G0B1CBTx` | `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx:17` | High |
| MCU metadata | `STM32G0B1C(B-C-E)Tx`, LQFP48 | `bms24v_platform/bms24v_platform.ioc:43-44` | High |
| Include paths | Core, HAL, CMSIS, `App`, `Com`, `Int` | `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx:343` | High |
| Source grouping | `Application/User/App`, `Com`, `Int`, HAL groups | `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx:450-710` | High |

## CubeMX Configuration

| Peripheral | Config | Evidence | Notes |
| --- | --- | --- | --- |
| GPIO | SC8815 PA5/PA6/PA7/PB0/PB1, BQ PB3/PB4, LEDs, button | `bms24v_platform/bms24v_platform.ioc:101-174` | SC8815 INT input-only conflict |
| FDCAN1 | Normal mode, FD no BRS, nominal 500 kbit/s calculated, std filters 1 | `bms24v_platform/bms24v_platform.ioc:5-23`; `bms24v_platform/Core/Src/fdcan.c:40-58` | No IRQ path evidence |
| I2C1 | Standard mode, timing `0x10B17DB5`, PB6/PB7 | `bms24v_platform/bms24v_platform.ioc:26-28,157-160`; `bms24v_platform/Core/Src/i2c.c:31-64,139-146` | BQ bus |
| I2C2 | Timing `0x10B17DB5`, PA11/PA12 | `bms24v_platform/bms24v_platform.ioc:29-30`; `bms24v_platform/Core/Src/i2c.c:74-107,172-179` | OLED/EEPROM bus |
| USART1 | Async, PA9/PA10, generated 115200 8N1 | `bms24v_platform/bms24v_platform.ioc:90,123,261-262`; `bms24v_platform/Core/Src/usart.c:41-52` | Debug UART |
| RTC | Enabled in init order | `bms24v_platform/Core/Src/main.c:122`; `bms24v_platform/bms24v_platform.ioc:246-248` | Runtime use not analyzed |
| TIM14 | HAL timebase | `bms24v_platform/bms24v_platform.ioc:85-87`; `bms24v_platform/Core/Src/stm32g0xx_it.c:121-126` | Do not repurpose casually |

## Clock Configuration

| Clock | Value | Evidence |
| --- | ---: | --- |
| SYSCLK | 64 MHz | `bms24v_platform/bms24v_platform.ioc:249-250` |
| HCLK/AHB | 64 MHz | `bms24v_platform/bms24v_platform.ioc:213-223` |
| FDCAN clock | 64 MHz | `bms24v_platform/bms24v_platform.ioc:221`; `bms24v_platform/Core/Src/fdcan.c:81-82` |
| I2C1/I2C2 clocks | 64 MHz | `bms24v_platform/bms24v_platform.ioc:227-228` |
| USART1 clock | 64 MHz | `bms24v_platform/bms24v_platform.ioc:253` |
| RTC clock | LSE 32768 Hz | `bms24v_platform/bms24v_platform.ioc:246-248` |

## Config Constants To Treat Carefully

| Constant | Location | Meaning | Evidence / Risk |
| --- | --- | --- | --- |
| `APP_BATMAN_CRC_BOOT_ENABLE` | `App/App_BatMan.c` | BQ boot CRC assumption | `App/App_BatMan.c:28-31`; conflicts with `docs/rules/hardware_rules.md:72-73` |
| `BQ76952_CELL_MASK_6S_HW_CONFIRMED` | `Int/Int_BQ76952_BSP.h` | 6S sparse Vcell mask | `Int/Int_BQ76952_BSP.h:297-311`; verify on hardware |
| `SC8815_PROJECT_*` masks/ratios | `Int/Int_SC8815_BSP.h` | SC project safety values | `Int/Int_SC8815_BSP.h:497-566`; depends on resistor/solder state |
| `INT_EEPROM_SIZE_BYTES/PAGE/ADDR` | `Int/Int_EEPROM.h` | M24C64 geometry | `Int/Int_EEPROM.h:7-9`; scan bus to confirm address |
| FDCAN timing fields | `.ioc` and `fdcan.c` | CAN FD nominal/data timing | `bms24v_platform/bms24v_platform.ioc:5-23`; `bms24v_platform/Core/Src/fdcan.c:47-57` |

## Build Verification

No command-line build script was found in this distillation. Use Keil/MDK project metadata for source membership and include path verification.

Manual check before committing code:

1. Confirm new `.c/.h` files are added to `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx`.
2. Confirm include paths still cover `App`, `Com`, `Int`.
3. Confirm generated CubeMX changes do not overwrite local `USER CODE` sections unexpectedly.
4. Confirm build outputs are not treated as source/training evidence.

Evidence: `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx:343,450-710`; corpus hygiene rule in `AGENTS.md:54-56`.
