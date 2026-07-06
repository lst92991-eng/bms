cmake_minimum_required(VERSION 3.22)

project(bms24v_platform C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(TARGET_NAME bms24v_platform)
set(PLATFORM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bms24v_platform)
set(FREERTOS_DIR ${PLATFORM_DIR}/MDK-ARM/FreeRTOS)
set(LINKER_SCRIPT ${PLATFORM_DIR}/gcc/STM32G0B1CBTx_FLASH.ld)

add_executable(${TARGET_NAME})

target_sources(${TARGET_NAME}
    PRIVATE
        ${PLATFORM_DIR}/gcc/startup_stm32g0b1xx_gcc.s
        ${PLATFORM_DIR}/Core/Src/retarget.c
        ${PLATFORM_DIR}/Core/Src/main.c
        ${PLATFORM_DIR}/Core/Src/gpio.c
        ${PLATFORM_DIR}/Core/Src/fdcan.c
        ${PLATFORM_DIR}/Core/Src/i2c.c
        ${PLATFORM_DIR}/Core/Src/rtc.c
        ${PLATFORM_DIR}/Core/Src/tim.c
        ${PLATFORM_DIR}/Core/Src/usart.c
        ${PLATFORM_DIR}/Core/Src/stm32g0xx_it.c
        ${PLATFORM_DIR}/Core/Src/stm32g0xx_hal_msp.c
        ${PLATFORM_DIR}/Core/Src/stm32g0xx_hal_timebase_tim.c
        ${PLATFORM_DIR}/Core/Src/system_stm32g0xx.c

        ${CMAKE_CURRENT_SOURCE_DIR}/App/config/Bms_Config.c
        ${CMAKE_CURRENT_SOURCE_DIR}/App/core/Bms_App.c
        ${CMAKE_CURRENT_SOURCE_DIR}/App/core/Bms_BatManager.c
        ${CMAKE_CURRENT_SOURCE_DIR}/App/core/Bms_Main.c
        ${CMAKE_CURRENT_SOURCE_DIR}/App/debug/Bms_BqDebug.c
        ${CMAKE_CURRENT_SOURCE_DIR}/App/debug/Bms_Debug.c
        ${CMAKE_CURRENT_SOURCE_DIR}/App/debug/Bms_DebugCli.c
        ${CMAKE_CURRENT_SOURCE_DIR}/App/debug/Bms_OledDebugView.c
        ${CMAKE_CURRENT_SOURCE_DIR}/App/error/Bms_Error.c
        ${CMAKE_CURRENT_SOURCE_DIR}/App/estimate/Bms_Estimate.c
        ${CMAKE_CURRENT_SOURCE_DIR}/App/estimate/Bms_EstimateLegacy.c
        ${CMAKE_CURRENT_SOURCE_DIR}/App/model/Bms_Model.c
        ${CMAKE_CURRENT_SOURCE_DIR}/App/port/Bms_BqConfigPort.c
        ${CMAKE_CURRENT_SOURCE_DIR}/App/port/Bms_BqSamplePort.c
        ${CMAKE_CURRENT_SOURCE_DIR}/App/port/Bms_PowerPort.c
        ${CMAKE_CURRENT_SOURCE_DIR}/App/port/Bms_Sc8815Port.c
        ${CMAKE_CURRENT_SOURCE_DIR}/App/server/Bms_ChargeService.c
        ${CMAKE_CURRENT_SOURCE_DIR}/App/server/Bms_DischargeService.c
        ${CMAKE_CURRENT_SOURCE_DIR}/App/server/Bms_BuzzerService.c
        ${CMAKE_CURRENT_SOURCE_DIR}/App/server/Bms_PowerService.c
        ${CMAKE_CURRENT_SOURCE_DIR}/App/server/Bms_PreDischargeService.c
        ${CMAKE_CURRENT_SOURCE_DIR}/App/server/Bms_ProtectionService.c
        ${CMAKE_CURRENT_SOURCE_DIR}/App/storage/Bms_Storage.c

        ${CMAKE_CURRENT_SOURCE_DIR}/Com/Com_BQ76952.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Com/Com_BatteryParam.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Com/Com_SOC.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Com/Com_SOH.c

        ${CMAKE_CURRENT_SOURCE_DIR}/Int/Int_BQ76952.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Int/Int_OLED.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Int/Int_SC8815.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Int/Int_Button.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Int/Int_Buzzer.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Int/Int_CanFd.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Int/Int_Led.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Int/Int_EEPROM.c

        ${FREERTOS_DIR}/source/tasks.c
        ${FREERTOS_DIR}/source/queue.c
        ${FREERTOS_DIR}/source/list.c
        ${FREERTOS_DIR}/source/timers.c
        ${FREERTOS_DIR}/source/event_groups.c
        ${FREERTOS_DIR}/source/stream_buffer.c
        ${FREERTOS_DIR}/source/croutine.c
        ${FREERTOS_DIR}/portable/GCC/ARM_CM0/port.c
        ${FREERTOS_DIR}/portable/MemMang/heap_4.c

        ${PLATFORM_DIR}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_tim.c
        ${PLATFORM_DIR}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_tim_ex.c
        ${PLATFORM_DIR}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_fdcan.c
        ${PLATFORM_DIR}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_rcc.c
        ${PLATFORM_DIR}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_rcc_ex.c
        ${PLATFORM_DIR}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_ll_rcc.c
        ${PLATFORM_DIR}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_flash.c
        ${PLATFORM_DIR}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_flash_ex.c
        ${PLATFORM_DIR}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_gpio.c
        ${PLATFORM_DIR}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_dma.c
        ${PLATFORM_DIR}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_dma_ex.c
        ${PLATFORM_DIR}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_pwr.c
        ${PLATFORM_DIR}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_pwr_ex.c
        ${PLATFORM_DIR}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_cortex.c
        ${PLATFORM_DIR}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal.c
        ${PLATFORM_DIR}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_exti.c
        ${PLATFORM_DIR}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_i2c.c
        ${PLATFORM_DIR}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_i2c_ex.c
        ${PLATFORM_DIR}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_rtc.c
        ${PLATFORM_DIR}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_rtc_ex.c
        ${PLATFORM_DIR}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_uart.c
        ${PLATFORM_DIR}/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_uart_ex.c
)

target_include_directories(${TARGET_NAME}
    PRIVATE
        ${PLATFORM_DIR}/Core/Inc
        ${PLATFORM_DIR}/Drivers/STM32G0xx_HAL_Driver/Inc
        ${PLATFORM_DIR}/Drivers/STM32G0xx_HAL_Driver/Inc/Legacy
        ${PLATFORM_DIR}/Drivers/CMSIS/Device/ST/STM32G0xx/Include
        ${PLATFORM_DIR}/Drivers/CMSIS/Include
        ${FREERTOS_DIR}/include
        ${FREERTOS_DIR}/portable/GCC/ARM_CM0
        ${CMAKE_CURRENT_SOURCE_DIR}/App
        ${CMAKE_CURRENT_SOURCE_DIR}/App/compat
        ${CMAKE_CURRENT_SOURCE_DIR}/App/config
        ${CMAKE_CURRENT_SOURCE_DIR}/App/core
        ${CMAKE_CURRENT_SOURCE_DIR}/App/debug
        ${CMAKE_CURRENT_SOURCE_DIR}/App/error
        ${CMAKE_CURRENT_SOURCE_DIR}/App/estimate
        ${CMAKE_CURRENT_SOURCE_DIR}/App/model
        ${CMAKE_CURRENT_SOURCE_DIR}/App/port
        ${CMAKE_CURRENT_SOURCE_DIR}/App/server
        ${CMAKE_CURRENT_SOURCE_DIR}/App/storage
        ${CMAKE_CURRENT_SOURCE_DIR}/Com
        ${CMAKE_CURRENT_SOURCE_DIR}/Int
)

target_compile_definitions(${TARGET_NAME}
    PRIVATE
        USE_HAL_DRIVER
        STM32G0B1xx
)

target_compile_options(${TARGET_NAME}
    PRIVATE
        -mcpu=cortex-m0plus
        -mthumb
        -ffunction-sections
        -fdata-sections
        -Wall
        -Wextra
        -Wno-unused-parameter
        $<$<CONFIG:Debug>:-Og -g3>
        $<$<CONFIG:Release>:-Os -g0>
)

target_link_options(${TARGET_NAME}
    PRIVATE
        -mcpu=cortex-m0plus
        -mthumb
        -T${LINKER_SCRIPT}
        -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.map
        -Wl,--gc-sections
        -Wl,--print-memory-usage
        -specs=nano.specs
        -specs=nosys.specs
)

set_target_properties(${TARGET_NAME}
    PROPERTIES
        OUTPUT_NAME ${TARGET_NAME}
        SUFFIX ".elf"
)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${TARGET_NAME}> ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET_NAME}> ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${TARGET_NAME}>
    COMMENT "Generating HEX/BIN and size report"
)
