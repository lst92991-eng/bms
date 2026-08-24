# 09 Change Impact Playbook

更新日期：2026-08-22

本手册用于回答“改一个参数/模块后，必须同步检查什么”。任何使能功率、保护阈值或并发模型的变更都应先建立可恢复的实板测试条件。

## 通用变更流程

1. 写明 `目标 / 上下文 / 约束 / 完成条件`，记录 branch/commit、板号、BOM、仪器和负载。
2. 用 `git status --short --branch` 分离用户已有修改和本次范围。
3. 先更新事实源：硬件规则/参数表/协议，再改 BSP、Int、Com、App；HAL 生成文件通过 `.ioc` 修改。
4. 保持默认输出安全：`HAL_Init()` 前直接建立 SC PSTOP/CE_N 安全电平；Core USER SysInit 的 GPIO→SC standby→I2C1→BQ all-off 不得后移到日志/其余外设之后；BQ 与 SC 必须使用同一个 Safety epoch，BQ owner 前/中/后、SC release 前、Power 总提交后均重验，四类 SetOutput 失败均必须 `ForceOutputsOff`。注意 BQ 在 clock/I2C1 可用前没有独立 MCU gate-off，必须保留为硬件/HIL 上限。
5. 运行格式/编译/host tests/尺寸门禁；再做受控 HIL。主机测试不可代替 gate、电流、温升与复位波形。
6. 更新 `04_hardware_software_matrix.csv`、`08_conflicts_and_unknowns.md`、`evidence_index.md` 和对应评分卡状态。
7. 冻结 commit SHA 后归档 build/test/HIL 证据；清理 build/log/cache/binary，不混入源码提交。

证据基线：`AGENTS.md`；`bms24v_platform/Core/Src/main.c:83-128`；`Int/Int_SC8815.c:21-46`；`App/App_Main.c:215-377`；`.gitignore:1-41`；`tests/host/check_repo_hygiene.py:11-105`。

## BQ76952 / 保护参数

### 触发文件

`Int/Int_BQ76952_BSP.h`、`Int/Int_BQ76952.c`、`App/App_BatMan_Config.c`、`App/App_BatMan_Sample.c`、`App/App_Power.c`。

### 必查影响

- 7-bit 地址/CRC/DeviceNumber 与实板 Comm Type；
- 6S sparse VC mapping 和所有未用通道；
- shunt、CC gain/current polarity、容量与单位；
- COV/CUV/OCC/OCD/SCD、温度、delay、recovery、PF masks；
- FET_OPTIONS、PDSG timeout/stop delta、sleep/alert behavior；
- DM manifest 写入值与读回 mask；config invalid 必须阻止 power；
- `desired/commanded/observed` 一致性；不得引入 `ALL_FETS_ON` 中间态；
- transaction 绝对 deadline；嵌套不能延长预算；
- complete-frame publish，任一字段失败不得混帧。
- 任一帧 transport/protocol 失败，或 POR/CFGUPDATE/FET_EN 指纹异常，都必须先以 `App_SC8815_EmergencyStop()` 物理停 SC，再撤销 config proof/Safety ready，最后 best-effort BQ 全关；任一 Safety inhibit 也必须先 PSTOP；
- offline 周期不得只改软件 flag，必须每周期有界尝试 BQ all-off；
- 重连不得复用启动期 config cache：必须在 1500 ms 不可插入的外层 transaction 内完成 all-off 屏障→DeviceNumber→manifest 重写/退出→全量验证→safe FET/all-off→终检，并等下一完整帧才 ready。

代码门禁：`tests/host/test_bq_safety_contract.py`；`platform_contract`；三 GCC preset。
HIL 门禁：单体注入、NACK/SDA stuck、ALERT latency、CHG/DSG/PDSG VGS、包电流、保护恢复。

证据：`Int/Int_BQ76952.c:130-240,585-659,1066-1195`；`App/App_BatMan_Config.c:75-78,193-310,378-811`；`App/App_BatMan_Sample.c:281-336,352-530`；`App/App_BatMan.c:191-346,673-684`；`App/App_Safety.c:67-93`；`App/App_Power.c:206-290,586-657`。

## SC8815 / 充电路径

### 触发文件

`Int/Int_SC8815_BSP.h`、`Int/Int_SC8815.c`、`App/App_SC8815.c`、`App/App_Power.c`、CubeMX GPIO。

### 必查影响

- PA6/PA7 固定物理 line mapping；禁止运行时自动交换；
- PSTOP active-high、CE_N active-low 和 reset 默认电平；
- IBUS/IBAT shunt、ratio、target、hard max 必须与硬件规则一致；
- VBATS 分压、外部分压 mode 与真实截止电压；
- OTG/反向输出必须继续被拒绝；
- 软件 I2C high/low timing、clock stretch、bus-stuck recovery、短原子段；
- register manifest + readback；配置期间 PSTOP 保持 high；
- Safety epoch/generation 在 PSTOP release 前最后复核；
- INT 进入 ISR 即 ForceStandby 并递增单调 sequence；owner 重申 inhibit，连续 2 clean 后在 IRQ-disabled 临界区内原子复验 sequence/clear，序号变化则保持急停。

代码门禁：`tests/host/test_platform_contract.py`、`test_power_soc_contract.py`。
HIL 门禁：logic analyzer、INT-to-PSTOP、充电曲线/截止/温升、限流精度、stuck bus、检查-执行竞态。

证据：`Int/Int_SC8815.c:61-187,557-839,1028-1139`；`App/App_SC8815.c:106-412,435-661`；`tests/host/test_power_soc_contract.py:70-85`。

## Power / Safety / Watchdog

改变状态、阈值、恢复或任务周期时，同时审阅：

- `App_Power` 是否仍是唯一业务授权者；CAN/CLI/OLED 不得直写 actuator；
- 每个 disable 分支是否先停 SC，且不被 target cache 跳过；
- offline BQ wake 是否仍在 Release 禁止、Engineering typed evidence 限时；
- BQ config/frame/cell/temp invalid 是否 fail-safe；
- BQ 通信/配置证明丢失是否立即撤权、全关，且只能通过完整重认证和下一完整帧恢复；
- Safety inhibit 变化是否撤销旧 epoch；
- critical BQ ALERT 是否在同一 BatMan owner 周期完成 Safety latch + BQ 四路 FET 全关；
- critical task heartbeat 是否在实际工作完成后上报；
- deadline 是否大于合理 WCET，又小于安全反应要求；
- stack watermark/grace/IWDG timeout 是否一起评估；
- Assert/stack overflow/HardFault 是否先 ForceStandby、记录后 reset。

代码门禁：四个 Python 结构/卫生 contract + host native algorithm；Host 5/5，三套 ARM 各 4/4，并执行格式、changed-C analyzer、HardFault objdump、Keil XML/member 门禁。
HIL 门禁：task suspend、deadlock、stack fault、HardFault、IWDG、BQ/SC offline、插拔/唤醒，全程示波 PSTOP/CE/CHG/DSG。

证据：`bms24v_platform/Core/Src/main.c:83-128`；`Int/Int_SC8815.c:21-46`；`App/App_Power.c:206-290,316-467,586-816`；`App/App_BatMan_Config.c:378-811`；`App/App_BatMan.c:191-346,673-684`；`App/App_Safety.c:67-117,155-211,301-560`；`Int/Int_Fault.c:176-215,256-272`。

## SOC / SOH / NVM

改变 OCV、容量、current sign、anchor 或 NVM schema 时：

- 同步 `Com_BatteryParam`/SOC/SOH、BQ current gain、Power full/empty 条件；
- 带载启动不得直接用 OCV；rest seed 必须有电流/持续时间/电压可信条件；
- full/empty anchor 必须是 Power 显式事件，不从单个电压样本推断；
- record 版本/magic/长度/CRC/sequence/双槽兼容策略明确；
- NVM slot/sequence/reconnect/flush 必须由同一静态 mutex 串行；SOC/SOH/provenance 必须从同一任务原子快照编码；
- scheduler pause 只允许覆盖内存导出/恢复，禁止跨 EEPROM/I2C2 操作；
- 写后读回、掉电中断、slot wrap、磨损节流必须验证；
- 定义长期断电 freshness；若无 RTC evidence，保持 Unknown/低置信恢复；
- OCV/容量/温漂来自目标电芯标定，不直接把表格单测通过写成产品精度。

代码门禁：`algorithm_unit` + `power_soc_contract`。
HIL 门禁：静置/带载冷启、完整循环、满/空 anchor、掉电 sweep、容量学习与温漂。

证据：`Com/Com_SOC.h:7-40,92-127`；`Com/Com_SOH.h:7-19`；`App/App_BatMan_Nvm.c:17-38,49-175,204-539,713-894`；`App/App_Power.c:488-528,692-814`。

## CAN / UART / 诊断

- CAN V1 保持只读；任何未来控制命令必须经过鉴权、freshness、Safety/Power 请求接口，且单独做 threat/safety review。
- RX 每周期 budget、event queue、TX defer/reinit 不得无界。
- Release 继续编译 CLI stub；Engineering 必须同时满足 build flag、物理 gate、限时解锁。
- 应用格式化日志只用 `Int_Log_Printf + Com_FormatV`，禁止直接 libc `printf/fprintf/snprintf`；`retarget` 只是兼容层。
- 新增日志必须使用固定缓冲、接受可计数截断/整块丢弃，不能让诊断反向影响控制 deadline。
- 每次固件链接必须通过 `check_forbidden_symbols.py`；任何 heap/stdio 符号或缺失 `Com_FormatV/Int_Log_Printf` 都是构建失败。

代码门禁：`platform_contract`；`algorithm_unit` 格式化器用例；post-link symbol gate。
HIL 门禁：CAN flood/bus-off/recovery、UART HAL_BUSY/ERROR、日志饱和、CLI gate/unlock timeout。

证据：`App/App_CanBms.c:23-40,362-550,679-819`；`App/App_DebugCli.c:3,236-264,446-496,569-589`；`Com/Com_Format.c:13-269`；`Int/Int_Log.c:11-24,47-231`；`CMakeLists.txt:194-201`；`tests/host/check_forbidden_symbols.py:10-50`。

## 任务/并发变更

新增任务或改变 period/priority/stack 时必须：

1. 使用静态 TCB/stack；不启用 FreeRTOS dynamic allocation。
2. 明确 single-writer/共享资源/mutex/ISR 边界。
3. 使用绝对周期调度和 overrun 统计。
4. 若任务影响功率安全，把它加入 critical registry、deadline、heartbeat 和 HWM；heartbeat 必须在工作完成后。
5. 测量 WCET、stack watermark、jitter；故意挂起验证 IWDG。
6. 更新 `05_rtos_concurrency_model.md`、CMake、Keil group、CI 测试。

证据：`App/App_Main.c:25-105,269-377`；`App/App_Safety.c:155-211,469-560`；`bms24v_platform/MDK-ARM/FreeRTOS/include/FreeRTOSConfig.h:19-49`。

## 硬件改版触发项

如果安全目标要求 MCU 单点失效立即关主 FET，软件重构不足，需评估：

- MCU 独立 CFETOFF/DFETOFF/BOTHOFF；
- 外部 safety supervisor / latch / contactor / fuse；
- 安全默认上下拉与 MCU reset high-Z 期间 gate；
- 独立于业务 I2C/CPU 的过流/短路切断链。

改版后必须更新 `.ioc`、原理图/网表、硬件规则、04 matrix、故障树和 HIL fixture。
证据：`bms24v_platform/Core/Inc/main.h:60-79`；`official_chip_docs_files/full_netlist (4).csv:29-45`。
