# GCC/CMake 迁移记录

## 第一阶段目标

- 保留 `bms24v_platform/MDK-ARM` 作为 Keil 对照工程。
- 从 Keil `.uvprojx` 和 scatter 文件抄出 MCU、宏定义、include 路径、源文件和内存布局。
- 新增 CMake 管理的 `arm-none-eabi-gcc` 编译路径，先做到稳定生成 ELF/HEX/BIN。
- GCC 构建稳定、下载验证通过后，再删除 Keil 工程和 Keil 构建产物。

## 从 Keil 抄下来的配置

- Target: `bms24v_platform`
- Device: `STM32G0B1CBTx`
- CPU: `Cortex-M0+`
- Pack: `Keil.STM32G0xx_DFP.2.1.0`
- Defines: `USE_HAL_DRIVER`, `STM32G0B1xx`
- Flash: `0x08000000`, size `0x00020000` / 128 KiB
- RAM: `0x20000000`, size `0x00024000` / 144 KiB
- Keil stack: `0x400`
- Keil heap: `0x200`
- Keil FreeRTOS port: `FreeRTOS/portable/RVDS/ARM_CM0`
- GCC FreeRTOS port: `FreeRTOS/portable/GCC/ARM_CM0`

## CMake 使用方式

```powershell
cmake --preset gcc-debug
cmake --build --preset gcc-debug
```

产物位置：

- `build/gcc-debug/bms24v_platform.elf`
- `build/gcc-debug/bms24v_platform.hex`
- `build/gcc-debug/bms24v_platform.bin`
- `build/gcc-debug/bms24v_platform.map`

## 烧录和串口验证

本次验证使用 CMSIS-DAP，SWD 频率固定为 `100k`。OpenOCD 可以识别芯片，但在 Flash 写入算法阶段掉线；最终使用 pyOCD + Keil `STM32G0xx_DFP.2.1.0` pack 烧录成功。

```powershell
python -m pyocd load --pack <Keil.STM32G0xx_DFP.2.1.0.pack> -t stm32g0b1cbtx -f 100k --format elf build\gcc-debug\bms24v_platform.elf
```

COM10 验证参数：

- `COM10`
- `115200`
- `8N1`
- `UTF-8`

已抓到的闭环日志要点：

- `BMS24V 平台安全启动`
- `进入 App_Main 任务层`
- `BQ通信正常 设备号:0x7695 CRC:0`
- `电池管理初始化成功`
- `RTOS: 创建任务`
- `RTOS: 启动调度器`
- 周期输出电池、电源、BQ FET、SC8815 状态日志

## 暂不删除 Keil 的原因

- 当前阶段需要用 Keil `.uvprojx/.map/.sct` 继续对照源文件清单、符号和内存占用。
- GCC 只能说明编译链路可用，是否运行稳定还需要下载到 STM32G0B1 板子验证外设、RTOS tick、串口打印、FDCAN/I2C/RTC/TIM 等路径。
- 稳定后再清理 `MDK-ARM` 下的 `.uvprojx/.uvoptx/.sct` 和历史构建产物，避免迁移中途丢证据。
