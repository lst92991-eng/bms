# BMS CAN FD 整车参数统一与实板验证（2026-08-15）

## 1. 目标与基线

- MCU：STM32G0B1CBTx。
- 调试器：CherryUSB CMSIS-DAP，序列号 `39A0ABCD`。
- SWD：10 MHz。
- 整车 CAN FD：11 位标准 ID，仲裁段 500 kbit/s，数据段 1 Mbit/s，BRS 和自动重传开启。
- BMS Node-ID：`0x03`；TPDO1：`0x183`，32 Byte，周期 1000 ms，关键状态变化立即补发。

## 2. 修改内容

- `bms24v_platform/Core/Src/fdcan.c`：启用 BRS 和自动重传；64 MHz 时钟下仲裁段使用 8×16 TQ，数据段使用 4×16 TQ，两段采样点均为 75%。
- `bms24v_platform/bms24v_platform.ioc`：同步 CubeMX 配置，避免重新生成后回退。
- `Int/Int_CanFd.c`、`Int/Int_CanFd.h`：发送和接收统一要求 FD+BRS。
- `App/App_CanBms.c`、`App/App_CanBms.h`：周期状态迁移到 `0x183`；Byte 0～3 改为节点启动后的单调毫秒时间戳；温度改为 0.1 °C；关键状态变化补发相同布局的 TPDO1，不再主动发送旧 `0x100`。
- `docs/protocol/bms_canfd_protocol.md`：同步链路参数、ID 和负载布局。

旧 `0x600/0x580` 查询代码仅作为独立板测兼容保留。整车网络不得发送这些旧私有请求；后续完整对象访问由 CANopen FD USDO 协议栈实现。

## 3. 构建

```powershell
$env:PATH='C:\MounRiver\MounRiver_Studio\toolchain\arm-none-eabi-gcc\bin;' + $env:PATH
cmake -S D:\AGV\new_bms -B D:\AGV\tmp\bms_canfd_unified_build -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=D:\AGV\new_bms\cmake\arm-none-eabi-gcc.cmake `
  -DCMAKE_BUILD_TYPE=Release
cmake --build D:\AGV\tmp\bms_canfd_unified_build --parallel
```

构建结果：

- FLASH：67,704 / 131,072 Byte（51.65%）。
- RAM：37,536 / 147,456 Byte（25.46%）。
- ELF SHA-256：`1BC0EA672D15E948D1C2E832099DE7B7B9CE07DDF8ED8E968BCB1EADE54834D7`。
- HEX SHA-256：`C541F4FFBBA00C865850B94CFA37D77A14A65CEF788E2FFD033C4CAA3802BA40`。
- BIN SHA-256：`1C5F4DA8E08F1477A391546F574671B898316CD38D37C62E4CA52D39AE7EDB0F`。

## 4. 烧录与回读

若本机没有 pyOCD，可安装到临时目录；该目录和 pack 不进入 Git：

```powershell
$py='C:\Users\lst\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $py -m pip install --target D:\AGV\tmp\pyocd_runtime pyocd==0.45.1
$env:PYTHONPATH='D:\AGV\tmp\pyocd_runtime'
```

使用 Keil `STM32G0xx_DFP.2.1.0` pack，以 10 MHz 烧录：

```powershell
& $py -m pyocd load `
  --pack D:\AGV\tmp\Keil.STM32G0xx_DFP.2.1.0.pack `
  -t stm32g0b1cbtx -u 39A0ABCD -f 10m `
  --format elf -e sector `
  D:\AGV\tmp\bms_canfd_1m_build\bms24v_platform.elf
```

实测擦除 69,632 Byte、写入 68,608 Byte，全程未掉线。随后从 `0x08000000` 回读 BIN 长度 67,704 Byte，回读 SHA-256 与构建 BIN 完全一致：

```text
1C5F4DA8E08F1477A391546F574671B898316CD38D37C62E4CA52D39AE7EDB0F
```

## 5. CAN FD 实测

只监听，不发送查询或控制帧：

```powershell
$env:PYTHONPATH='D:\AGV\tmp\pyocd_runtime'
& $py D:\AGV\code_model\mag-track_-ch32\tools\magtrack_canfd_test.py `
  --duration 60 --data-bitrate 1000000 --raw-id 0x183
```

结果：

- 60秒收到60帧 `0x183`，每帧32 Byte。
- USB帧标志为 `0x0E`，包含FD和BRS。
- 相邻时间戳增加1000 ms。
- 数据示例：总压23.75 V、SOC 84%、单体压差3 mV、温度26.0 °C、无活动故障。
- CAN错误帧0，回显帧0。
- 额外监听旧`0x180` 5秒：0帧，CAN错误帧0。

同一60秒窗口内，电机板、磁导航和BMS共收到10,261帧，原始CAN错误帧0。磁导航首次未发报，单独断电5秒再上电后恢复为`0x182/48B/100Hz`，序号跳变和时间戳异常均为0。
