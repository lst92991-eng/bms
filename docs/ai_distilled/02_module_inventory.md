# 02 Module Inventory

更新日期：2026-08-22

## Module Inventory

下表描述当前实现，不把历史目录、构建产物或未来规划当作有效模块。

### App 层

| 模块 | 单一职责/边界 | 关键证据 | 信心 | 人工确认 |
| --- | --- | --- | --- | --- |
| `App_Main` | 初始化顺序、静态任务内存、任务创建、绝对周期调度 | `App/App_Main.c:25-105,215-377` | High | Not needed |
| `App_Safety` | inhibit、关键任务 deadline/stack、wake epoch、IWDG 唯一刷新者 | `App/App_Safety.h:12-65,85-94`；`App/App_Safety.c:67-117,155-211,301-609` | High | Needed，watchdog HIL |
| `App_Power` | 充/放/预放/唤醒唯一业务授权；shutdown provenance；offline 每周期全关与重认证后新帧门禁；SC 启动失败同周期回滚 BQ 全关 | `App/App_Power.h:7-69`；`App/App_Power.c:125-304,330-480,600-829` | High | Needed，策略边界 HIL |
| `App_BatMan` | BQ facade、初始化、ALERT 单调序号/保护锁存、PSTOP-first 运行期 proof 撤销、周期/维护入口 | `App/App_BatMan.c:191-346,455-700` | High | Needed，保护/失联实测 |
| `App_BatMan_Config` | BQ DM manifest、写读回、FET desired/commanded/observed、`CONFIG_RECOVERY_REQUIRED` 与 1500 ms 整体重认证 transaction | `App/App_BatMan_Config.c:75-78,193-310,463-811` | High | Needed，TRM/BOM/HIL |
| `App_BatMan_Sample` | 限时完整帧采集、原子发布、age/sequence、原始 POR/CFGUPDATE/FET_EN 快照；不在采样层先撤销 proof | `App/App_BatMan_Sample.c:281-336,352-530` | High | Needed，复位/失联 HIL |
| `App_BatMan_Nvm` | SOC/SOH 双槽 CRC/sequence、写后读回；静态 recursive mutex 串行化 init/task/flush；短暂停调度抓取同一时刻 SOC+SOH snapshot | `App/App_BatMan_Nvm.c:17-38,49-175,204-539,713-894` | High | Needed，掉电/并发测试 |
| `App_BatMan_Estimator` | 把采样帧、anchor 事件与 SOC/SOH 纯算法连接 | `App/App_BatMan_Estimator.c` | High | Needed，标定 |
| `App_BatMan_Debug` | Engineering 诊断输出；不应成为生产授权入口 | `App/App_BatMan_Debug.c`；`App/App_DebugCli.c:3` | High | Not needed |
| `App_SC8815` | SC 单写者、staged sample、配置读回、credentialed start；SC_EVENT sequence + 2-clean 原子清门禁 | `App/App_SC8815.c:20-47,106-412,435-505,508-661` | High | Needed，模拟量/事件竞态 HIL |
| `App_CanBms` | 只读诊断协议、原子快照、有限 RX/event 队列、重初始化 | `App/App_CanBms.h:7-77`；`App/App_CanBms.c:23-40,186-244,362-550,679-819` | High | Needed，总线 HIL |
| `App_DebugCli` | Engineering-only、物理 gate + 60 s 解锁；命令提交到 Power | `App/App_DebugCli.h:7-20`；`App/App_DebugCli.c:3,26-35,236-264,322-405,446-496,569-589` | High | Needed，确认量产宏 |
| `App_OLED` | 低优先级状态显示，只消费快照 | `App/App_OLED.c`；`App/App_Main.c:178-206` | High | Not needed |
| `App_Buzzer` | 蜂鸣器乐谱/报警节奏策略；底层 PWM 在 `Int_Buzzer` | `App/App_Buzzer.c`；`Int/Int_Buzzer.c` | High | Not needed |

### Com 层

| 模块 | 职责/约束 | 证据 | 信心 | 人工确认 |
| --- | --- | --- | --- | --- |
| `Com_SOC` | 纯 SOC 算法；可信静置/持久化/anchor seed；输出 validity/confidence/age | `Com/Com_SOC.h:7-40,92-127`；`Com/Com_SOC.c` | High | Needed，OCV/容量标定 |
| `Com_SOH` | 容量学习、健康度与持久结构，容量有上界保护 | `Com/Com_SOH.h:7-19`；`Com/Com_SOH.c` | High | Needed，循环数据 |
| `Com_BatteryParam` | 目标电芯 OCV/容量等算法参数 | `Com/Com_BatteryParam.c`；`Com/Com_BatteryParam.h` | High | Needed，目标电芯标定 |
| `Com_BQ76952` | BQ 原始量到工程量的纯换算/语义辅助 | `Com/Com_BQ76952.c`；`Com/Com_BQ76952.h` | High | Needed，单位与校准复核 |
| `Com_Format` | 无堆、有界的整数/字符串格式化；不支持的格式失败，超长安全截断 | `Com/Com_Format.h:1-16`；`Com/Com_Format.c:13-269`；`tests/host/test_algorithms.c:376-408` | High | Not needed |

### Int 层

| 模块 | 职责/安全边界 | 证据 | 信心 | 人工确认 |
| --- | --- | --- | --- | --- |
| `Int_BQ76952` | BQ I2C transaction、递归静态 mutex、绝对截止时间、direct/subcommand/DM | `Int/Int_BQ76952.c:1-38,48-61,86-240,524-659,1066-1195` | High | Needed，异常总线 HIL |
| `Int_SC8815` | 确定性软件 I2C、clock-stretch/stuck recovery、读回、直接 PSTOP、单调 IRQ sequence | `Int/Int_SC8815.c:61-187,557-738,743-839,1028-1139` | High | Needed，逻辑分析仪 |
| `Int_I2C2Bus` | OLED/EEPROM 共用 I2C2 的静态递归 mutex | `Int/Int_I2C2Bus.c:7-61` | High | Needed，总线压力 |
| `Int_EEPROM` | M24C64 页边界安全读写，借用 I2C2 总线锁 | `Int/Int_EEPROM.h:7-15`；`Int/Int_EEPROM.c` | High | Needed，掉电/寿命 |
| `Int_OLED` | 8 页批量刷新，共享 I2C2 锁；Clear 不隐式刷屏 | `Int/Int_OLED.c:16-17,114-147` | High | Not needed |
| `Int_CanFd` | FDCAN 过滤、收发、错误恢复的 transport | `Int/Int_CanFd.c:83-199` | High | Needed，bus-off HIL |
| `Int_Log` | `Int_Log_Printf` 用 384 B 固定栈缓冲 + `Com_FormatV`，投递到 1024 B 静态 UART IT ring；截断/满时计数且不等待 | `Int/Int_Log.h:16-41`；`Int/Int_Log.c:11-24,47-173,175-231`；`bms24v_platform/Core/Src/retarget.c:1-52` | High | Needed，拥塞/jitter 测试 |
| `Int_Watchdog` | 直接配置/刷新 IWDG；无业务喂狗决策 | `Int/Int_Watchdog.c:17-78` | High | Needed，实测超时 |
| `Int_Fault` | fault snapshot、backup register、Panic/HardFault/reset hooks | `Int/Int_Fault.h:7-63`；`Int/Int_Fault.c:63-174,176-215,256-272` | High | Needed，复位保持测试 |
| `Int_Buzzer/Int_Led` | 板级定时器/GPIO 薄封装 | `Int/Int_Buzzer.c`；`Int/Int_Led.c` | High | Not needed |

## 生成层与配置

| 对象 | 当前作用 | 证据 | 变更策略 |
| --- | --- | --- | --- |
| `Core/Src/main.c` | HAL 前直接建 SC safe GPIO；时钟后在 USER SysInit 执行 GPIO→SC standby→I2C1→BQ all-off，再初始化其余外设并调用 `App_Main()` | `bms24v_platform/Core/Src/main.c:83-128` | 保持 USER CODE 边界；早期安全顺序不可后移 |
| `stm32g0xx_it.c` | 异常/IRQ 入口，SC/BQ EXTI 先急停 | `bms24v_platform/Core/Src/stm32g0xx_it.c:204-212` | ISR 必须短且可界定 |
| `gpio/i2c/fdcan/usart/tim.c` | 引脚、时序、IRQ、外设参数 | `bms24v_platform/Core/Src/gpio.c:54-111`；`bms24v_platform/Core/Src/i2c.c:41-107` | 由 `.ioc` 管理，不承载业务 |
| `.ioc` | CubeMX 单一生成配置源 | `bms24v_platform/bms24v_platform.ioc` | 修改时同步审阅生成 diff |
| startup/linker | vector、Flash/RAM、初始 stack/heap | `bms24v_platform/MDK-ARM/startup_stm32g0b1xx.s:31-60`；`bms24v_platform/gcc/STM32G0B1CBTx_FLASH.ld:3-9` | 不写业务 |

## 构建与验证模块

| 对象 | 作用 | 证据 | 当前判断 |
| --- | --- | --- | --- |
| `CMakeLists.txt` | GCC 固件源、警告、link、尺寸门禁、heap/stdio 符号门禁、host tests | `CMakeLists.txt:9-15,24-205` | Active |
| `CMakePresets.json` | debug/release/engineering-release 配置 | `CMakePresets.json:5-35` | Active |
| Keil `.uvprojx` | STM32G0B1、App/Com/Int/FreeRTOS 文件清单 | `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx:10-21,343-851` | XML/member gates passed；实际 ARMCC build Unknown |
| `tests/host` | 算法、结构与 repo hygiene 契约 | `tests/host/CMakeLists.txt:1-55`；`tests/host/test_algorithms.c`；`tests/host/test_*_contract.py`；`tests/host/check_repo_hygiene.py` | Host 5/5；三套 ARM 各 4/4 |
| GitHub Actions | 三 preset 构建 + CTest | `.github/workflows/firmware-ci.yml:1-36` | 配置存在；远端运行结果 Unknown |

## 非模块/不应入库对象

`build/`、`logs/`、对象文件、固件二进制、map、pack/cache、`.log.lock` 和临时测试输出不是源代码模块；`.gitignore` 和 `repo_hygiene` CTest 已覆盖这些模式。最终 tracked forbidden=0，本交付版本已退跟踪 1098 项历史产物。【事实】

- 证据：`.gitignore:1-41`；`tests/host/check_repo_hygiene.py:11-105`；`tests/host/CMakeLists.txt:8-13`
- 信心：High
- 人工确认：Not needed，1098 项退跟踪 manifest 已人工审阅；冻结 SHA 的远端 CI 归档仍属于发布流程
