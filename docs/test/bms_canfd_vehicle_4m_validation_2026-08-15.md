# BMS CAN FD 4 Mbit/s 构建、烧录与整车验证

日期：2026-08-15

## 结果

BMS 已从 500 kbit/s / 1 Mbit/s 改为 500 kbit/s / 4 Mbit/s、ISO CAN FD、
BRS，并与电机板、磁导航共同通过 60 秒整车静态监听。BMS 的 `0x183/32B`
收到 60 帧，约 1 Hz；全网 CAN 错误帧为 0。本轮未发送查询或控制命令。

## 参数依据与源码位置

- FDCAN 时钟 64 MHz。
- 仲裁段：`64 MHz / (8 * (1+11+4)) = 500 kbit/s`，采样点 75%。
- 数据段：`64 MHz / (1 * (1+11+4)) = 4 Mbit/s`，采样点 75%。
- `bms24v_platform/Core/Src/fdcan.c` 设置数据预分频为 1。
- `Int/Int_CanFd.c` 在启动 FDCAN 前设置 TDC offset 11、filter 0，并检查
  两个 HAL 调用的返回值。
- `bms24v_platform/bms24v_platform.ioc` 同步为 4 Mbit/s，防止 CubeMX 再生
  成 1 Mbit/s。

64 MHz 不能用整数总时间量精确得到 5 Mbit/s，所以在不改 BMS 时钟树时，
4 Mbit/s 是三板共同可精确配置的最高数据位率。1 Mbit/s 固件仍是回退基线。

## 可复现构建

```powershell
$env:PATH='C:\MounRiver\MounRiver_Studio\toolchain\arm-none-eabi-gcc\bin;' + $env:PATH
cmake -S D:\AGV\new_bms -B D:\AGV\tmp\bms_canfd_4m_build -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=D:\AGV\new_bms\cmake\arm-none-eabi-gcc.cmake `
  -DCMAKE_BUILD_TYPE=Release
cmake --build D:\AGV\tmp\bms_canfd_4m_build --parallel --clean-first
Get-FileHash -Algorithm SHA256 D:\AGV\tmp\bms_canfd_4m_build\bms24v_platform.bin
Get-FileHash -Algorithm SHA256 D:\AGV\tmp\bms_canfd_4m_build\bms24v_platform.elf
```

- BIN：67,800 Byte，SHA-256
  `B09D240D59AF90363C4E762CF9C81CE55EC9174DD0F995156E84752514A7C00A`。
- ELF：248,132 Byte，SHA-256
  `42F81262512AA5B5F7BB7C91A0F6F55C942B2594A31B98B6BC05C250376614D0`。

## DAPLink 烧录和回读

调试器序列号 `39A0ABCD`，目标 `stm32g0b1cbtx`，SWD 10 MHz，pyOCD
0.45.1，pack 为 `STM32G0xx_DFP.2.1.0.pack`：

```powershell
$py='C:\Users\lst\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $py -m pip install --target D:\AGV\tmp\pyocd_runtime pyocd==0.45.1
$env:PYTHONPATH='D:\AGV\tmp\pyocd_runtime'
& $py -m pyocd load `
  --pack D:\AGV\tmp\Keil.STM32G0xx_DFP.2.1.0.pack `
  -t stm32g0b1cbtx -u 39A0ABCD -f 10m --format elf -e sector `
  D:\AGV\tmp\bms_canfd_4m_build\bms24v_platform.elf
& $py -m pyocd commander `
  --pack D:\AGV\tmp\Keil.STM32G0xx_DFP.2.1.0.pack `
  -t stm32g0b1cbtx -u 39A0ABCD -f 10m `
  -c "savemem 0x08000000 67800 D:\AGV\tmp\bms_canfd_4m_readback.bin"
Get-FileHash -Algorithm SHA256 D:\AGV\tmp\bms_canfd_4m_readback.bin
```

本次擦除 69,632 Byte、写入 68,608 Byte，回读 67,800 Byte 的 SHA-256 与
构建 BIN 完全相同；烧录结束后 CPU 状态为 Running。

## 整车复验

拓扑为 `USB2CANFD(120 Ω) -> 电机板 -> BMS -> 磁导航(120 Ω)`，全部信号
GND 共地，断电测 CAN-H/CAN-L 约 60 Ω。统一烧录三板后执行：

```powershell
$env:PYTHONPATH='D:\AGV\tmp\canfd_monitor_runtime'
& $py D:\AGV\code_model\mag-track_-ch32\tools\canfd_vehicle_monitor.py `
  --duration 60 --data-bitrate 4000000
```

最终结果为 `0x183=60 帧/32B/1.000 Hz`，全网 10,264 帧、五个 PDO 完整、
FD/BRS 和长度异常 0、CAN 错误帧 0，命令判定 `VERDICT PASS`。完整三板构建、
烧录、监听判据和磁导航首次缺帧的已知问题见磁导航仓库的
`docs/canfd_vehicle_4m_validation_2026-08-15.md`。
