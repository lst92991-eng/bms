# 08 Conflicts and Unknowns

更新日期：2026-08-22

## 判定规则

- **Fact**：当前源码、生成配置、网表或人工确认直接支持。
- **Inference**：由多个事实推导，仍需实测或设计评审确认。
- **Unknown**：证据不存在或不足，不用经验填空。
- **Conflict**：两个有效来源给出不同值；保留双方，直到 BOM/实测/用户决策裁决。
- **Software closed**：代码与主机测试可证明设计路径已整改，但不能替代 HIL。
- **HIL blocked**：必须依赖目标板、电源/负载、示波器/逻辑分析仪/温箱等外部证据。

## Conflicts

| ID | 冲突 | 来源 A | 来源 B | 当前处理 | 风险 | 信心 | 人工确认 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| C-001 | SC 软件 I2C 物理线序 | 生成宏/硬件规则标注 PA7=SCL、PA6=SDA | driver 以 `LINE_SWAPPED=1` 实际映射 PA6=SCL、PA7=SDA，并注释为实板确认 | 保留固定 swapped 实现，不允许运行时切换；上电功率测试前用逻辑分析仪裁决并同步规则/生成命名 | 线序错误将导致通信失败；错误探测不得误启动功率级 | High | Needed |
| C-002 | SC VBATS 分压历史值 | `official_chip_docs_files/README.md` 写未来 100 k/5.1 k | 2026-07-10 人工确认与规则/源码为 200 k/10 k | 以后者作为当前软件基线；旧 README 保留为历史冲突 | 截止电压设置错误 | High | Needed：板上 DMM 与充电截止确认 |
| C-003 | BQ shunt | 迁移说明/旧网表写 0.5 mOhm | 人工确认与硬件规则写 5 mOhm | 软件按 5 mOhm 基线；物理阻值/电流增益仍需标定 | 电流、保护阈值、SOC 积分产生 10 倍误差 | High | Needed |
| C-004 | Watchdog 文档状态 | `manual_confirmations.md` 2026-07-12 写 Deferred | 当前源码已实现 IWDG、安全 supervisor 和 fault hooks | 源码事实覆盖旧计划；实测复位仍 Unknown | 文档误导或错误发布判断 | High | Needed：更新规则源或注明历史 |

证据：

- C-001：`bms24v_platform/Core/Inc/main.h:63-66`；`docs/rules/hardware_rules.md:37-39`；`Int/Int_SC8815_BSP.h:19-20`；`Int/Int_SC8815.c:61-89`
- C-002：`official_chip_docs_files/README.md:1`；`docs/wordflow/manual_confirmations.md:8`；`Int/Int_SC8815_BSP.h:132-161`
- C-003：`docs/rules/hardware_rules.md:81-86`；`docs/wordflow/manual_confirmations.md:7`；`official_chip_docs_files/BQ76930_to_BQ76952_逻辑替换设计说明.md:189`
- C-004：`docs/wordflow/manual_confirmations.md:12`；`App/App_Safety.c:469-560`；`Int/Int_Watchdog.c:17-78`

Pro 评审后曾出现的 SC ratio/default-limit 文档冲突现已在源码层对齐：IBUS ratio=3、IBAT ratio=6、IBUS default/max=1.5/3 A、25.2 V Release IBAT default/max=1/5 A。该项从 `Conflict` 降为“软件值一致、标定/HIL pending”。证据：`docs/rules/hardware_rules.md:46-60`；`Int/Int_SC8815_BSP.h:21-31`；`tests/host/test_power_soc_contract.py:58-67`。

## Software-closed but HIL-blocked

| ID | 事实结论 | 软件证据 | 为什么仍不是硬件闭环 | 状态 | 人工确认 |
| --- | --- | --- | --- | --- | --- |
| H-001 | BQ 多步事务有 recursive mutex + 绝对 deadline，完整帧失败即拒绝 | `Int/Int_BQ76952.c:130-240,636-659`；`App/App_BatMan_Sample.c:352-530` | 需 NACK/stuck/clock 异常下测任务 deadline、PSTOP 和 IWDG | HIL blocked | Needed |
| H-002 | BQ FET 不再通过 `ALL_FETS_ON` 中间态；desired/commanded/observed 可审计 | `App/App_BatMan_Config.c:314-665`；`Int/Int_BQ76952.c:1066-1103` | 需同时示波 CHG/DSG/PCHG/PDSG VGS 与包电流，排除瞬态 gate pulse | HIL blocked | Needed |
| H-003 | BQ 主 FET 与 SC start 同用 Safety epoch；BQ owner 前/中/后、SC release 前和 Power 总提交后复验；SC start 失败同周期全关 BQ；critical ALERT owner 周期全关 FET；BQ ALERT 仅在单调 sequence/pin 原子复验后清 inhibit | `App/App_Safety.h:85-92`；`App/App_Safety.c:301-465`；`App/App_BatMan_Config.c:463-547`；`App/App_BatMan.c:191-293`；`Int/Int_BQ76952.c:585-635`；`App/App_Power.c:206-290`；`App/App_SC8815.c:106-312` | 需把 INT/ALERT/任务超时注入每个“检查-执行窗口”并同步示波两路 actuator | HIL blocked | Needed |
| H-004 | IWDG 仅由健康 supervisor 刷新，异常先 ForceStandby/记录/复位；Core 在 HAL/时钟前直接把 SC PSTOP/CE_N 拉到安全电平，时钟后在其余外设/日志前以 I2C1 请求 BQ all-off | `bms24v_platform/Core/Src/main.c:83-128`；`Int/Int_SC8815.c:21-46`；`App/App_Main.c:215-225`；`App/App_Safety.c:469-560`；`Int/Int_Fault.c:176-215` | SC pre-HAL 软件路径已闭环；BQ 在 clock/I2C1 可用前无独立 MCU gate，仍需测 LSI 与 reset 全程 PSTOP/CHG/DSG 波形 | HIL blocked | Needed |
| H-005 | Release 不含可执行 CLI，Engineering 有物理 gate、解锁、Power 请求边界 | `App/App_DebugCli.c:3,236-264,322-405,569-589` | 需验证量产宏、烧录镜像和物理 gate wiring | HIL blocked | Needed |
| H-006 | 温度无效会使 power fail-safe，不回退到 IC 温度继续运行 | `App/App_BatMan_Sample.c:228-280,301-336,475-504`；`App/App_Power.c:468-486,586-686` | NTC 型号/Beta/位置和温箱阈值未确认 | HIL blocked | Needed |
| H-007 | 软件 I2C 不再整笔关中断，支持 bounded stretch 和 9-pulse recovery；SC_EVENT 有单调 sequence、owner 重申 inhibit、2-clean 与 IRQ-disabled 原子复验/clear | `Int/Int_SC8815.c:107-187,557-637,743-770`；`App/App_SC8815.c:435-505`；`tests/host/test_power_soc_contract.py:70-85` | 需逻辑分析仪验证真实 line swap、rise time、stretch、stuck recovery，并在 resolve 窗口注入新 IRQ | HIL blocked | Needed |
| H-008 | SOC anchor 是显式事件并有电流/电压/稳定时间约束 | `Com/Com_SOC.h:19-40`；`App/App_Power.c:456-503` | OCV 表、容量、温漂、循环学习未标定 | HIL blocked | Needed |
| H-009 | NVM slot/sequence/reconnect/flush 由静态 recursive mutex 串行；SOC/SOH/provenance 在短 scheduler pause 中形成一致快照，EEPROM I/O 不在 pause 内 | `App/App_BatMan_Nvm.c:49-175,713-894`；`tests/host/test_platform_contract.py:155-186` | 仍需真实并发压力、I2C2 stuck、任意写中掉电和 endurance 验证 | HIL blocked | Needed |
| H-010 | BQ 任一完整帧失败或 POR/CFGUPDATE/FET_EN 指纹异常时第一硬件动作停 SC，再撤 config proof/ready 并全关；任一 Safety inhibit 也先 PSTOP；offline 每周期重试 all-off；恢复在 1500 ms 整体 transaction 内重认证并等下一完整帧 | `App/App_BatMan_Sample.c:281-336,411-472`；`App/App_BatMan.c:294-346,673-684`；`App/App_Safety.c:67-93`；`App/App_BatMan_Config.c:75-78,666-811`；`App/App_Power.c:316-467,586-657`；`tests/host/test_bq_safety_contract.py:178-241` | 需真实掉线、BQ brownout/POR、复位中断、重连、1500 ms 预算适用性和 gate 波形验证 | HIL blocked | Needed |
| H-011 | 应用日志改用 `Int_Log_Printf + Com_FormatV`，固定 384 B 栈缓冲投递 1024 B 静态 UART ring；post-link gate 拒绝 heap/stdio；当前 Release gate 通过 | `Com/Com_Format.c:13-269`；`Int/Int_Log.c:11-24,47-173,175-231`；`CMakeLists.txt:194-201`；`tests/host/check_forbidden_symbols.py:10-50`；`tests/host/test_algorithms.c:376-408` | 仍需 HAL_BUSY/ERROR、断线、日志洪泛下测丢弃计数和高优先级任务 jitter | HIL blocked | Needed |

## Open Unknowns

| ID | Unknown | 已知边界/证据 | 风险 | 关闭方法 | 优先级 |
| --- | --- | --- | --- | --- | --- |
| U-001 | TS1/TS3/DCHG/DFETOFF 的实际 NTC 型号、Beta、分压与通道对应 | 规则明确仍需确认：`docs/rules/hardware_rules.md:83-86` | 温度换算和保护错误 | BOM + 电阻测量 + 温箱多点标定 | P0 release gate |
| U-002 | BQ 保护/DM manifest 的最终量产阈值、delay、FET option 是否与安全需求/BOM一致 | 软件写读回只证明“写入一致”，不证明“值正确”：`App/App_BatMan_Config.c:99-295` | COV/CUV/OCD/SCD/温度保护失配 | TRM 独立复核 + 参数表签字 + 故障注入 | P0 release gate |
| U-003 | 当前固件的充电曲线、截止、电芯均衡、温升 | 历史记录不覆盖当前重构 | 过充/充电异常 | 可编程电源 + 单体记录 + 温升曲线 | P0 release gate |
| U-004 | 当前固件的 1/3/5/7/10 A 放电、OCD/SCD/短路响应 | 2026-07-12 仅是旧版本“当前阶段”确认 | 新架构回归未证明 | 电子负载/短路夹具 + gate/current scope | P0 release gate |
| U-005 | PDSG 外部电容量、成功判据、超时与 R40 脉冲热 | R40=100 Ohm/2 W，实际负载电容未知：`official_chip_docs_files/full_netlist (4).csv:264-268` | 重试/短路可过热 | 多档电容/负载，VOUT/VGS/current/temp 同步测量 | P0 release gate |
| U-006 | shutdown、充电器插入、BQ offline wake 的真实来源/时序 | Release 禁止软件 wake，Engineering 仅有 typed evidence | 无法唤醒或误唤醒 | 冷启动/掉电/插拔矩阵 + provenance log | P0 release gate |
| U-007 | reset/时钟初始化/MCU 卡死期间主 FET 的独立关断能力 | MCU 无独立 CFETOFF/DFETOFF gate；BQ EarlySafe 必须等待 clock + I2C1 + command，见 Hardware Limit | 上电早期或单点故障期间 CHG/DSG 可能保持到 BQ 自主保护或软件命令成功 | reset-to-command gate waveform + 安全目标评审；必要时硬件改版 | Hardware blocker |
| U-008 | IWDG 实际超时与 backup fault record 保持 | 代码按 nominal 32 kHz LSI | 超时窗口与诊断可靠性不明 | 停 heartbeat/挂起任务/HardFault/断电测试 | P0 release gate |
| U-009 | OLED 与 EEPROM 实际 7-bit 地址、I2C2 stuck-bus 行为 | 规则写待扫描：`docs/rules/hardware_rules.md:90-97` | 维护任务阻塞/持久化失败 | I2C scan、拔设备、SDA stuck、写断电 | P1 |
| U-010 | 冻结 commit 的 Keil/ARMCC build、map、warning、尺寸 | Keil XML/member gates 已通过，但没有实际 ARMCC clean build artifact | 双工具链漂移 | clean Keil rebuild + artifact/hash | P1 |
| U-011 | 远端 CI run、标准合规分析、覆盖率 | workflow 存在；30 个 changed-C clang analyzer 已通过；无远端 SHA、MISRA/CERT、全量 analyzer 或 coverage 报告 | 质量门禁仍有可追溯性/覆盖缺口 | 归档 CI URL/SHA/当前 analyzer 日志；引入全量 analyzer/coverage | P1 |
| U-012 | SOC NVM 跨长时间掉电的新鲜度 | record 未见 RTC 时间戳/断电时长 | 陈旧 SOC 被高置信使用 | 定义 freshness 或可信静置恢复策略 | P1 |
| U-013 | EEPROM endurance/掉电原子性在真实器件上的结果 | 双槽/CRC/读回、NVM mutex 和任务原子快照已实现：`App/App_BatMan_Nvm.c:49-175,204-539,713-894` | 并发撕裂的软件缺口已收口，但器件写中断电、寿命和 stuck-bus 仍未证明 | power-cut sweep + 竞争/stuck + 写次数/节流验证 | P1 |
| U-014 | CAN bus-off、拥塞、RX flood、UART HAL_BUSY/ERROR/日志洪泛 | transport 有预算/统计；Release 已无 heap/stdio 符号 | 诊断丢失、统计错误或任务 jitter 超标 | 总线/串口故障注入 + task trace | P2 |

## Historical evidence boundary

`docs/review/board_regression_2026-07-07.md` 和 2026-07-12 的放电确认可证明旧固件/旧提交在当时的板级现象；它们早于当前安全架构重构，不能作为当前镜像的回归通过证据。【事实 + 推断】

- 信心：High
- 人工确认：Needed，记录固件 SHA/板号/仪器/环境后重跑必要回归

## Hardware limit that software cannot close

当前 MCU 在 `HAL_Init()` 前即可通过 PB0/PB1 直接建立 SC8815 standby/disable，但没有独立 MCU 主 FET gate-off 脚；DFETOFF/DCHG 被 NTC 资源占用，CFETOFF 未连接。BQ all-off 仍需时钟、I2C1 和命令成功。这意味着 watchdog、异常处理和启动全关只能缩短 BQ 软件失效暴露窗口，不能证明“reset 起始或 MCU 单点故障下主 FET 立即独立断开”。【事实 + 推断】

证据：`bms24v_platform/Core/Src/main.c:83-110`；`Int/Int_SC8815.c:21-46`；`bms24v_platform/Core/Inc/main.h:60-79`；`official_chip_docs_files/BQ76930_to_BQ76952_逻辑替换设计说明.md:189-200`；`official_chip_docs_files/full_netlist (4).csv:29-45`。
信心：High。人工确认：Needed。
