# 01 Architecture

更新日期：2026-08-22

## Architecture Summary

工程采用四层结构，生成层与业务层明确分离：

1. `Core/Drivers`：CubeMX、HAL、CMSIS、startup、IRQ 入口；仅做硬件初始化和回调转发。
2. `Int_*`：BQ76952、SC8815、CAN、EEPROM、OLED、日志、看门狗、故障等芯片/板级接口。
3. `Com_*`：SOC、SOH、电芯参数与 BQ 工程量换算等不直接操作硬件的算法与公共逻辑。
4. `App_*`：任务、功率状态机、采样发布、安全监督、通信和显示。

证据：`CMakeLists.txt:39-118`；`bms24v_platform/Core/Src/main.c:83-128`；`App/App_Main.c:215-377`。
信心：High。人工确认：Not needed。

## 启动流

```text
Reset/Startup
  -> direct PB0/PB1 safe GPIO before HAL_Init
  -> HAL_Init
  -> SystemClock
  -> USER SysInit: GPIO -> SC8815 ForceStandby -> I2C1 -> BQ ALL_FETS_OFF
  -> remaining CubeMX peripherals -> Int_Log_Init
  -> App_Main(boot_early_bq_safe)
       -> repeat SC8815 ForceStandby + BQ early-safe
       -> Safety records the AND of both early-safe confirmations
       -> Safety/I2C2/BQ/SC/Power/CAN/NVM/OLED init
       -> static task creation
       -> vTaskStartScheduler
```

SC 的最早安全动作位于 `main.c` 的 USER 1 区：直接开启 GPIOB、用 BSRR 把 PB0/PB1 置高后才调用 `HAL_Init()` 和配置系统时钟。BQ early-safe 需要时钟和 I2C1，因此在 USER SysInit 执行，仍早于 RTC/FDCAN/I2C2/UART/TIM 和日志；`App_Main()` 在一般模块初始化前再次要求 SC standby 和 BQ early-safe。任一关键任务创建失败进入 `Int_Fault_Panic()`，调度器异常返回也进入 Panic。【事实】

- 证据：`bms24v_platform/Core/Src/main.c:83-128`；`Int/Int_SC8815.c:21-46`；`App/App_Main.c:215-267,269-377`
- 信心：High
- 人工确认：Not needed

## 控制面

### 功率授权

`App_Power` 读取 BatMan/SC/Safety 快照并形成目标状态；实际使能路径必须经过 BQ FET 请求和 SC 的 credentialed start。两路使用同一个 Safety authorization epoch：BQ owner 在动作前、FET 操作后和提交有效状态前复验，Power 在 BQ/SC 操作后再做总复验。任何复验失败都同步 PSTOP 并要求 BQ 全关。所有关闭分支先停止 SC，再关闭或限制 BQ 路径。【事实】

- 证据：`App/App_Power.c:206-290`；`App/App_BatMan_Config.c:463-547`；`App/App_SC8815.c:106-312`
- 信心：High
- 人工确认：Not needed

### 防 TOCTOU 授权

Safety 以 generation/epoch 表示当前安全门状态；任何 gate 变化或 BQ/SC 事件会撤销旧授权。BQ 主 FET 与 SC start 必须携带同一 epoch；SC 在 PSTOP 释放前、BQ 在 owner transaction 的前/中/后分别复验，避免“检查后、执行前”状态变化导致带故障启动。critical BQ ALERT 在同一 BatMan owner 周期锁存 Safety inhibit 并执行 `KeepMainFetsOff()`。【事实】

- 证据：`App/App_Safety.h:85-92`；`App/App_Safety.c:67-117,301-465`；`App/App_SC8815.c:106-312`；`App/App_BatMan_Config.c:463-547`；`App/App_BatMan.c:191-293`；`Int/Int_BQ76952.c:585-635`
- 信心：High
- 人工确认：Not needed

### BQ 唤醒

Release 构建禁用离线 BQ 的软件唤醒；工程构建只接受带类型和来源的短时 evidence，并受 Safety wake epoch/500 ms 时效约束。一般 Power 请求不能隐式唤醒 BQ。【事实】

- 证据：`App/App_Power.c:19-28,177-204,307-480`；`App/App_Safety.h:21-35`；`App/App_Safety.c:387-468`
- 信心：High
- 人工确认：Needed，工程唤醒条件需 HIL

### BQ 运行期配置证明与重认证

任一完整帧失败都会撤销启动期配置缓存；完整帧若观察到 `BatteryStatus.POR/CFGUPDATE` 或 `ManufacturingStatus.FET_EN=0`，同样把状态置为 `CONFIG_RECOVERY_REQUIRED`。BatMan 的第一硬件动作是同步 `App_SC8815_EmergencyStop()`，随后撤销 Safety ready，再 best-effort 全关 BQ；Safety 设置任一 inhibit 也先直接 PSTOP，Power 在 offline 的每个周期继续全关。恢复通信后不能把一次 ACK 当作旧配置仍有效，而要在同一 1500 ms 外层 transaction 的全关屏障下核对 DeviceNumber、重写/退出 manifest、全量读回、重建安全 FET 模式、终检无异常且四路仍全关；之后必须等待下一完整有效帧才恢复 ready。【事实】

- 证据：`App/App_BatMan_Sample.c:411-472`；`App/App_BatMan.c:294-346,673-684`；`App/App_Safety.c:67-93`；`App/App_BatMan_Config.c:75-78,666-811`；`App/App_Power.c:316-467,586-657`
- 信心：High
- 人工确认：Needed，运行期断线/复位/恢复与 gate HIL

## 数据面

### BQ 完整帧发布

BatMan 在一个有绝对截止时间的 BQ transaction 内把所有字段读入 staging frame；任一字段失败即拒绝整帧，只在完整成功后原子发布并递增 sequence。消费者可同时检查 `valid`、age 和 sequence。【事实】

- 证据：`App/App_BatMan_Sample.c:10-65,352-472,505-530`；`Int/Int_BQ76952.c:130-240,636-659`
- 信心：High
- 人工确认：Not needed

### SC 完整帧与单写者

`App_SC8815` 是 SC8815 的运行时单写者；采样以 staged snapshot 发布。高-低-高读取避免多字节 ADC 撕裂；配置写入有读回验证。【事实】

- 证据：`App/App_SC8815.c:20-26,313-434`；`Int/Int_SC8815.c:642-676,771-839`
- 信心：High
- 人工确认：Needed，测量换算与外部仪表对比

### SOC/SOH/NVM

SOC/SOH 算法在 `Com_*` 内运行；SOC 不在带载启动时直接信任 OCV，只有可信静置或显式 full/empty anchor 才提高置信。EEPROM 用双槽、sequence、CRC 和写后读回保存 SOC/SOH。NVM 的 init/task/flush 由独立静态 recursive mutex 串行化；SOC+SOH 持久字段在暂停任务切换的短窗口内一次性抓取，EEPROM I/O 发生在恢复调度后。没有 RTC 断电时长证据，因此长期掉电后的 SOC 新鲜度仍为 Unknown。【事实 + Unknown】

- 证据：`Com/Com_SOC.h:7-40,92-127`；`App/App_BatMan_Nvm.c:17-38,49-175,204-539,713-894`
- 信心：High
- 人工确认：Needed，OCV/容量标定与断电策略

### 日志数据路径

应用模块通过 `Int_Log_Printf()` 而不是 libc `printf()` 输出格式化日志。该 API 在调用者栈上使用固定 384 B 缓冲区，调用无堆依赖的 `Com_FormatV()` 限界整数格式化器，再整块投递到 1024 B 静态 UART ring；超长文本截断并计数，ring 空间不足时整块丢弃，生产者不等待 UART。`retarget.c` 仅保留 libc 兼容入口，不是应用日志主路径；链接后 symbol gate 强制拒绝 heap/stdio 符号并要求 `Com_FormatV`/`Int_Log_Printf` 存在。【事实】

- 证据：`Com/Com_Format.h:1-16`；`Com/Com_Format.c:13-269`；`Int/Int_Log.h:16-41`；`Int/Int_Log.c:11-24,47-173,175-231`；`App/App_Main.c:235-266,367-374`；`CMakeLists.txt:194-201`；`tests/host/check_forbidden_symbols.py:10-50`
- 信心：High
- 人工确认：Needed，串口拥塞/jitter 测试

## 故障面

SC INT、BQ ALERT ISR 先调用直接 BSRR 的 `Int_SC8815_ForceStandby()`，再通知 Safety/BatMan；SC driver 还为每次事件递增单调 sequence。SC owner 重申 inhibit，只有连续 2 个 clean frame 后才在关 IRQ 临界区内一次性复验 sequence 并清除 inhibit；如序号变化则保持 inhibit、立即急停并重置 clean 计数。异常、断言、栈溢出、malloc hook、HardFault 统一进入 `Int_Fault`。Panic/HardFault 保存复位原因与必要上下文到 RTC backup register 后复位。【事实】

- 证据：`bms24v_platform/Core/Src/stm32g0xx_it.c:204-212`；`Int/Int_SC8815.c:731-770`；`App/App_SC8815.c:435-505`；`Int/Int_Fault.c:63-122,176-215,256-272`
- 信心：High
- 人工确认：Needed，断电/复位波形与记录保持性

Safety 仅在启动宽限期结束、所有关键任务 deadline/stack 健康且关键硬件 ready 时刷新 IWDG；IWDG 标称 6 s，但依赖未校准 LSI 32 kHz 假设。【事实】

- 证据：`App/App_Safety.c:9-16,469-560`；`Int/Int_Watchdog.c:17-78`
- 信心：High（软件）；Medium（实际超时）
- 人工确认：Needed

## 业务接口边界

- CAN V1 是只读诊断协议，不提供 MOS、PDSG、故障清除或参数写命令。证据：`docs/protocol/bms_canfd_protocol.md:1-11`；`App/App_CanBms.h:25-51`。
- Release 不编译 CLI 命令实现；Engineering CLI 默认还要求物理使能与 60 s 解锁，命令只提交 Power 请求，不直接越过 actuator。证据：`App/App_DebugCli.h:7-20`；`App/App_DebugCli.c:3,26-35,236-264,322-405,569-589`。
- CAN 初始化失败不会绕过本机功率安全逻辑，只降低诊断能力。证据：`App/App_Main.c:260-265`。这是当前设计决策，不等于通信故障已做 HIL。

## Hardware Safety Limit

SC8815 有 MCU 直接 PSTOP 路径，并在系统时钟配置前建立安全电平；BQ 主 CHG/DSG 没有同等级独立 MCU GPIO 关断路径，BQ early-safe 必须等时钟/I2C/命令确认。IWDG 能把部分卡死转化为复位，但不能替代独立 gate。因此“复位窗口及 MCU 单故障立即切断主 FET”是硬件能力缺口，不应在软件评分中伪装成 Closed。

证据：`bms24v_platform/Core/Inc/main.h:60-79`；`official_chip_docs_files/BQ76930_to_BQ76952_逻辑替换设计说明.md:189-200`；`official_chip_docs_files/full_netlist (4).csv:29-45`。
信心：High。人工确认：Needed。
