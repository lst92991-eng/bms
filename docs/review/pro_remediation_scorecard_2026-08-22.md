# Pro Review Remediation Scorecard — 2026-08-22

## Scope and status rules

本表把 Pro 评审基线 `main @ 42fff3c7f9fe5b3482e6850cd9009621eca48c8d` 的每个 P0、P1 和“其他中等问题”（本表编号为 P2）映射到本交付版本的冻结实现。它只回答“原问题是否已由当前实现处理、有什么证据、还缺什么”，不把静态验证冒充实板量产结论。

- `Closed`：问题本身可由静态/单元证据完整关闭，不依赖实板物理行为。
- `Software closed`：实现和自动化合同已关闭软件设计缺口，但产品参数/实板结果另列。
- `HIL blocked`：软件整改有证据，但问题涉及真实时序、gate、电流、温度或复位，未做 HIL 不能关闭。
- `Unknown`：缺少可复核证据，或共享工作区/参数冲突尚未冻结。

软件构建、host unit 和结构契约不能替代充电、放电、短路、温度、预放电、唤醒和 IWDG/HardFault 的实板 HIL。当前硬件也没有独立 MCU 主 FET 关断脚；该上限不会因为软件评分提高而消失。

## Executive scorecard

| ID | Pro 评审问题 | 当前结论 | 代码证据 | 测试证据 | 状态 | 信心 | 人工确认 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| P0-1 | BQ 异常阻塞高优先级任务，SC 停充只排队且可能饿死 | 已增加无锁/无 I2C 的直接 PSTOP、BQ 首错拒帧、100 ms 整帧预算、单 HAL 10 ms 上限、绝对周期防补跑、最高优先级 Safety；BQ/SC 同用 Safety epoch，critical ALERT owner 周期立即全关 BQ FET；BQ ALERT 使用单调 sequence 且仅在 pin/sequence 原子复验后清 inhibit；运行期证明丢失先硬件停 SC、再 mutation proof/撤权、最后 best-effort BQ 全关，Safety 任一 inhibit 也先 PSTOP，Power 在 offline 每周期重试 | `Int/Int_SC8815.c:731-770`; `App/App_BatMan_Sample.c:10-16,411-472`; `Int/Int_BQ76952.c:24-38,87-240,585-659`; `App/App_Main.c:86-127`; `App/App_Safety.c:67-93,301-465,469-560`; `App/App_BatMan.c:191-346,673-684`; `App/App_Power.c:586-657` | `tests/host/test_bq_safety_contract.py:35-76,177-224,234-238`; `tests/host/test_platform_contract.py:45-226` | HIL blocked | High | Needed：拉低 SDA/SCL、在 epoch/ALERT resolve 每个窗口注入事件、测 fault→PSTOP/BQ gate、WCET/deadline/IWDG |
| P0-2 | `ALL_FETS_ON -> FET_CONTROL` 非原子中间态 | 选择性 FET 路径不再先发 `ALL_FETS_ON`；pre-reset `commanded_off` 只在成功命令并回读后提交；BQ owner 在动作前、FET write/readback 后及提交有效状态前复验同一 Safety epoch，失效立即收敛到全关；Power 的授权、BQ、SC 和末次复验四类失败统一执行 `ForceOutputsOff` | `App/App_BatMan_Config.c:378-420,423-665`; `Int/Int_BQ76952.c:1066-1103`; `App/App_Power.c:206-290`; `App/App_BatMan.h:57-73,151-157` | `tests/host/test_bq_safety_contract.py:78-128,234-238` | HIL blocked | High | Needed：同步测 CHG/DSG/PCHG/PDSG/PSTOP/VGS/包电流所有转换及 mid-transaction ALERT |
| P0-3 | CLI 绕过 Power 且使缓存与硬件失步 | Release 编译为 stub；Engineering 才编译任务，默认物理 gate 关闭，需 60 s unlock；危险命令只提交 Power 请求，PDSG probe/test 拒绝 | `App/App_DebugCli.h:7-20`; `App/App_DebugCli.c:3,26-35,236-264,322-405,446-496,569-589`; `App/App_Main.c:166-179,313-320` | `tests/host/test_platform_contract.py:45-226`; GCC Release preset macro OFF：`CMakePresets.json:16-24` | Software closed | High | Needed：量产镜像符号/字符串抽查和物理 gate wiring |
| P0-4 | 无独立 watchdog、heartbeat、hooks、fault record | 静态 critical task 注册/deadline/HWM；Safety 唯一喂 IWDG；assert/stack/malloc/HardFault 统一先急停、备份记录、复位；任务创建失败 Panic；Core 在 `HAL_Init()` 前直接建立 SC PSTOP/CE_N safe GPIO，时钟后在其余外设/日志前完成 GPIO→SC Force→I2C1→BQ EarlySafe，并把 BQ 证据传入 App_Main 再次确认 | `bms24v_platform/Core/Src/main.c:83-128`; `Int/Int_SC8815.c:21-46`; `App/App_Main.c:215-225,269-377`; `App/App_Safety.c:155-211,469-560`; `Int/Int_Watchdog.c:17-78`; `Int/Int_Fault.c:63-174,176-215,256-272`; `bms24v_platform/MDK-ARM/FreeRTOS/include/FreeRTOSConfig.h:40-68` | `tests/host/test_platform_contract.py:45-226` | HIL blocked | High | Needed：挂起任务/死锁/栈溢出/HardFault，测实际 reset interval 与复位全程所有输出；BQ 在 clock/I2C1 前无独立 gate |
| P1-1 | 任意 `!cell_ok` 都可能进入 BQ wake-charge | 现在区分 host shutdown、confirmed/offline/wake source；Release 禁止离线软件唤醒，Engineering 仅 typed evidence + 500 ms epoch；普通 comm/config/cell invalid 保持安全并全关。合法 wake 后仍需完整重认证和下一完整帧 | `App/App_Power.h:45-69`; `App/App_Power.c:19-28,316-467,586-657`; `App/App_BatMan_Config.c:666-811`; `App/App_Safety.h:21-35`; `App/App_Safety.c:387-465` | `tests/host/test_power_soc_contract.py:38-112`; `tests/host/test_bq_safety_contract.py:145-178,226-238` | HIL blocked | High | Needed：shutdown/charger insert/offline/NACK/POR provenance 与 gate 矩阵 |
| P1-2 | TS 失效回退 IC 温度仍允许功率 | TS validity 是独立故障条件；invalid/out-of-range 会标记 temp fault，Power 禁止充放电，不以 IC temperature 替代授权 | `App/App_BatMan_Sample.c:228-280,301-336,475-504`; `App/App_Power.c:468-486,586-686` | `tests/host/test_power_soc_contract.py:38-112` | HIL blocked | High（软件）；Medium（传感器模型） | Needed：TS 开短路、NTC/Beta/位置、温箱边界 |
| P1-3 | BQ 关键 Data Memory 无完整 expected/actual 闭环 | DM manifest 集中列出并逐项写/读回，首错失败；DeviceNumber 精确校验。采样层只发布原始 reset fingerprint；Safety facade 先 PSTOP 后撤销 config proof，恢复时完整重写/退出/验证/终检并等下一帧 | `App/App_BatMan_Config.c:99-310,666-811`; `App/App_BatMan_Sample.c:281-336,411-472`; `App/App_BatMan.c:294-346`; `Int/Int_BQ76952_BSP.h:154-228` | `tests/host/test_bq_safety_contract.py:130-197` | Software closed | High | Needed：独立 TRM/参数签字；写读回一致不证明阈值产品正确；POR/recovery HIL |
| P1-4 | SOC 满/空锚点与 Power 立即关断冲突 | Anchor 改为 Power 的显式完成/截止事件，并验证关断后电流、delta、cell voltage 和稳定窗口；SOC API 保留 seed provenance/confidence | `App/App_Power.c:488-528,692-814`; `Com/Com_SOC.h:19-40,92-127`; `Com/Com_SOC.c` | `tests/host/test_algorithms.c:71-429`; `tests/host/test_power_soc_contract.py:38-112` | Software closed | High | Needed：完整充放循环、taper/EOC、回弹、容量学习 HIL |
| P1-5 | BQ indirect window 缺事务级互斥 | 静态 recursive mutex 锁完整 indirect/DM transaction；nested transaction 继承外层绝对 deadline且不可延长；ISR 不访问总线 | `Int/Int_BQ76952.c:48-61,130-240,585-659`; `Int/Int_BQ76952.c:1105-1195` | `tests/host/test_bq_safety_contract.py:35-66,145-175` | Software closed | High | Needed：并发/NACK/timeout 压力 HIL |
| P1-6 | 高优先级任务最长 1 s blocking UART | 应用统一调用 `Int_Log_Printf`；384 B 固定栈缓冲经纯 `Com_FormatV` 写入 1024 B 静态 UART IT ring，满时整块丢弃并统计，不等待串口。每次固件链接强制执行 symbol gate，拒绝 heap/stdio 符号并要求 bounded formatter/log symbols | `Com/Com_Format.c:15-269`; `Int/Int_Log.c:11-24,47-173,175-231`; `CMakeLists.txt:194-201`; `tests/host/check_forbidden_symbols.py:10-50` | `tests/host/test_algorithms.c:376-408`; Release：`cmake --build --preset gcc-release` 输出 `symbol gate: no heap/stdio symbols; bounded log formatter present`，Flash 68,412/122,880 B，RAM 20,704/135,168 B；手工 `nm` 过滤无禁用符号 | Software closed | High | Needed：冻结 SHA/ELF 哈希；HAL_BUSY/ERROR、断线、日志洪泛、栈水位与任务 jitter HIL |
| P2-1 | SC bit-bang 全事务关 IRQ、无 stretch/stuck、运行时换线 | 只保留短 GPIO 原子段；bounded stretch、bus-busy 检查、9-pulse recovery；线序编译期固定，probe 不允许切换。ISR 使用单调 sequence，owner 重申 inhibit/PSTOP、两帧 clean 后在 IRQ-disabled 区复验 sequence 并原子 clear；变化则继续 inhibit 并重置 clean。driver fixed-swapped 后实际 PA6=SCL/PA7=SDA 与生成标签/硬件规则相反，已显式标为 Conflict | `Int/Int_SC8815.c:61-187,557-637,743-770,1105-1139`; `App/App_SC8815.c:435-505`; `Int/Int_SC8815_BSP.h:10-20`; `bms24v_platform/Core/Inc/main.h:63-66`; `docs/rules/hardware_rules.md:37-39` | `tests/host/test_platform_contract.py:45-226`; `tests/host/test_power_soc_contract.py:38-112` | HIL blocked | High | Needed：逻辑分析仪裁决 line order，并在 resolve-window 注入 INT，量化 IRQ-off、INT→PSTOP、rise/fall/stretch/recovery |
| P2-2 | 首次有效电压直接 OCV 播种，带载 SOC 不可信 | 优先持久化；无记录时必须满足可信 rest seed，结果携带 validity/confidence/age；不把第一帧 OCV 当高置信值 | `Com/Com_SOC.h:19-40,92-127`; `Com/Com_SOC.c` | `tests/host/test_algorithms.c:71-429`; `tests/host/test_power_soc_contract.py:38-112` | Software closed | High | Needed：目标电芯 OCV/温度/容量标定和带载冷启 |
| P2-3a | 无 `-Werror`、host unit、CTest、CI、尺寸/符号门禁 | 已加入 `-Werror`、三 GCC preset、五项 host CTest、GitHub host + 三 preset matrix、Flash/RAM budget、每次固件链接强制 heap/stdio symbol gate，并把 repo hygiene 注册为所有配置的 CTest | `CMakeLists.txt:3-16,24-30,142-205`; `tests/host/CMakeLists.txt:1-55`; `.github/workflows/firmware-ci.yml:8-36`; `tests/host/check_forbidden_symbols.py:10-50`; `tests/host/check_repo_hygiene.py:11-105` | Host 5/5；三套 clean ARM build 各 4/4；Release size/symbol gate 已通过 | Software closed | High | Needed：冻结 SHA 的远端绿色 run 与 artifact |
| P2-3b | clang-tidy/cppcheck、MISRA/CERT、覆盖率缺失 | 当前未发现这些门禁或报告 | N/A | N/A | Unknown | High | Needed：定义适用标准、工具、waiver、coverage target |
| P2-3c | HIL 自动化/安全故障注入缺失 | 软件合同不能证明物理保护；当前无可复核自动化 HIL artifact | N/A | 现有 host tests 仅为非 HIL 证据 | HIL blocked | High | Needed：建立 fixture、阈值和冻结 SHA 结果 |
| P2-4 | 仓库交付卫生差 | `.gitignore` 已覆盖 build/cache/log/binary/`.log.lock`；`check_repo_hygiene.py` 对 Git tracked path 做 fail-closed 审计并注册 CTest。最终 tracked forbidden artifact=0；本交付版本已退跟踪 1098 个历史产物，其中仍需本地使用的二进制/lock 只从 Git index 移除、工作区保留并由 ignore 覆盖 | `.gitignore:1-41`; `tests/host/check_repo_hygiene.py:11-105`; `tests/host/CMakeLists.txt:8-13`; 交付提交相对父版本的 deletion manifest | `repo_hygiene` 通过；Host 5/5、三套 ARM 各 4/4 | Software closed | High | Not needed：1098 项退跟踪已纳入本交付版本；冻结 SHA 的远端 CI 仍需单独归档 |

## Final red-team addenda

| ID | 结论 | 代码证据 | 测试证据 | 状态 | 信心 | 人工确认 |
| --- | --- | --- | --- | --- | --- | --- |
| RT-1 NVM concurrency | NVM 自有静态 recursive mutex 已串行 slot/sequence/reconnect/task/flush；SOC/SOH/provenance 从同一任务原子快照编码，scheduler pause 不跨 EEPROM I/O | `App/App_BatMan_Nvm.c:49-175,713-894` | `tests/host/test_platform_contract.py:158-177` | Software closed | High | Needed：并发压力、I2C2 stuck、任意写中掉电和 endurance HIL |
| RT-2 split early-safe | SC 在 `HAL_Init()`/SystemClock 前可直接置 PSTOP/CE_N safe；BQ all-off 在时钟后、其余 Cube 外设/日志前执行并由 App_Main 二次确认。BQ 在 clock/I2C1/command 前仍没有独立 MCU gate-off | `bms24v_platform/Core/Src/main.c:83-128`; `Int/Int_SC8815.c:21-46`; `App/App_Main.c:215-225` | `tests/host/test_platform_contract.py:45-226` | HIL blocked | High | Needed：reset-to-HAL/clock SC waveform、reset-to-BQ-command/gate waveform及硬件安全目标确认 |
| RT-3 runtime BQ reauthentication | 采样层只发布原始指纹；帧失败或 POR/CFGUPDATE/FET_EN 异常由 Safety facade 先停 SC、再 mutation proof/撤权、最后全关；Safety 任一 inhibit 也先 PSTOP；offline 每周期重试；重在线由单一 1500 ms outer transaction 完成 all-off 屏障、身份、manifest 重写/验证、安全 FET 终检，随后仍等下一完整帧 | `App/App_BatMan_Sample.c:281-336,411-472`; `App/App_BatMan.c:294-346,673-684`; `App/App_Safety.c:67-93`; `App/App_BatMan_Config.c:666-811`; `App/App_Power.c:316-467,586-657` | `tests/host/test_bq_safety_contract.py:145-208,234-238` | HIL blocked | High | Needed：通信断开、BQ POR/brownout、恢复中再次掉线和四路 gate 波形 |
| RT-4 SC event/actuator rollback | SC ISR 使用单调 sequence；owner 收到事件后重申 Safety inhibit 并急停，连续两帧 clean 后才在 IRQ-disabled 区复验 sequence、清 inhibit/state；复验变化则保持 inhibit/PSTOP 并重计 clean。Power 在 BQ 已放开后若 SC 启动失败，同周期统一关闭 SC 和所有 BQ 主 FET | `Int/Int_SC8815.c:743-770`; `App/App_SC8815.c:435-505`; `App/App_Power.c:206-290` | `tests/host/test_power_soc_contract.py:70-85` | HIL blocked | High | Needed：SC INT resolve-window 注入、SC start fail、epoch 变化下 PSTOP/四路 gate 同步波形 |
| RT-5 BQ ALERT/proof ordering | BQ ALERT ISR 只锁存单调 sequence；任务先执行 runtime proof，后解析/清 ALERT，最后发布 ready。critical alert 同周期锁 Safety 并全关；非关键 clear 在 IRQ-disabled 区复验 sequence 和 active-low pin，避免旧帧清除新事件 | `Int/Int_BQ76952.c:585-635`; `App/App_BatMan.c:191-346,673-684` | `tests/host/test_bq_safety_contract.py:67-76,177-224` | HIL blocked | High | Needed：ALERT resolve-window、pin bounce、frame fail 与 POR 并发注入及四路 gate 波形 |

## Detailed closure notes

### P0-1 — realtime fail-safe

**事实**：SC 急停直接写 GPIO BSRR；SC/BQ EXTI 和 Fault 均在任何后续处理前调用。BQ outer sample 遇首个错误短路、拒绝整帧；周期调度不会连续补跑。
**推断**：这些设计把停充路径从低优先级队列依赖中移除，并限制 BQ 错误的 CPU 占用。
**Unknown**：实际 SDA/SCL 拉低时 HAL/IRQ/调度的最大 fault→PSTOP 延迟。
因此状态为 `HIL blocked`，不能仅凭代码标 `Closed`。

### P0-2 — FET transitions and hardware ceiling

**事实**：当前 selective FET control 未出现 Pro 基线中的通用 `ALL_FETS_ON` 前导；transaction 序列有互斥；pre-reset `commanded_off` 只在成功命令并回读后更新；Power 的四类执行失败统一回滚到 SC+BQ 全关。
**Unknown**：BQ 内部状态变化、charge pump 和外部门极网络是否产生未请求脉冲。
**硬件上限**：当前 MCU 没有独立 CFETOFF/DFETOFF 主 gate 控制；DFETOFF/DCHG 被 NTC 复用，CFETOFF 未连接。证据：`bms24v_platform/Core/Inc/main.h:60-79`；`official_chip_docs_files/BQ76930_to_BQ76952_逻辑替换设计说明.md:189-200`；`official_chip_docs_files/full_netlist (4).csv:29-45`。

### P0-4 — watchdog is not an independent main-FET gate

**事实**：IWDG/supervisor/fault record 已实现；`HAL_Init()` 前直接把 SC PSTOP/CE_N 拉高，时钟后才初始化 I2C1 并向 BQ 请求 all-off，App 层再复验两次 BQ early-safe 的 AND。
**推断**：该顺序能把部分 CPU hang 转化为 bounded reset，并把 SC 软件可控的停机动作前移到时钟配置之前。
**限制**：从 reset 起始到 GPIOB 接管的 SC 电平仍需示波；BQ 主 FET 不具备 MCU 独立 gate-off，clock/I2C1/command 前存在硬件上限，IWDG 不能等价为第二硬件关断通道。

### P1-3 — readback versus parameter correctness

DM expected/actual readback 关闭的是“写了但不知道是否生效”的软件缺口。它不证明 COV/CUV/OCD/SCD、NTC、FET_OPTIONS、PDSG 参数适合这块板和这组电芯。后者仍是 `08_conflicts_and_unknowns.md` 中的 U-001/U-002 和 HIL gate。

### P1-6 — bounded logging versus runtime timing

**事实**：应用日志已统一到 `Int_Log_Printf`；纯 `Com_FormatV` 不依赖 libc stdio，384 B 固定栈缓冲写入静态 UART IT ring；post-link gate 拒绝 heap/stdio symbols。当前 Release 构建与手工 `nm` 过滤均通过。
**Unknown**：软件证据仍不能量化 UART 断线、`HAL_BUSY`/`HAL_ERROR` 和持续日志洪泛下的 Safety/BatMan jitter、丢弃统计及最坏栈水位。

### P2-1 / RT-4 — SC event resolve race

**事实**：ISR 单调 sequence 与 owner 的 IRQ-disabled recheck/clear 防止“新事件恰逢旧事件清除”被静默覆盖；不一致路径保持 inhibit、再次急停并重置 clean 计数。
**Unknown**：实际 SC INT 电平行为、反复触发和总线故障下的事件闭环及时延仍需故障注入 HIL。

### P2-3 — quality gates

CI 配置文件存在并不等于远端已在当前 commit 通过；结构契约也不是语义证明。最终交付至少要记录 commit SHA、CI run URL、工具版本、Host 5/5、三套 ARM 各 4/4 输出、三个固件尺寸和 Keil build 结果。Repository hygiene 与 1098 项历史产物退跟踪已纳入本交付版本；远端 CI run URL 与 Keil 实际构建结果仍为 `Unknown`。

## HIL evidence required before a 100/100 unattended-production release claim

| HIL test | 最低可观察量 | 通过标准必须量化 |
| --- | --- | --- |
| BQ SDA/SCL stuck/NACK | SCL/SDA、PSTOP、task heartbeat、IWDG reset | fault→PSTOP 最大延迟；无无限补跑/死锁 |
| BQ runtime POR/reconnect | POR/CFGUPDATE/FET_EN、config state、ready epoch、四路 VGS | 立即撤权/全关；重认证任一步失败保持关闭；下一完整帧前不 ready |
| BQ FET transition matrix | CHG/DSG/PCHG/PDSG VGS、PSTOP、pack current | 无未请求 gate pulse；故障/恢复序列符合需求 |
| SC ACK/stuck/stretch/INT | PA6/PA7、INT、PSTOP、CE_N、IBUS/IBAT | line order 正确；恢复有界；INT→PSTOP 最大延迟 |
| Charge curve | 6 cells、pack、IBUS/IBAT、温度、EOC、FET | 限流/截止/回差/温升满足批准参数 |
| Discharge/OCD/SCD/short | cells、pack current、VGS、PSTOP、fault bits | 阈值/delay/关断/锁存/恢复全部满足需求 |
| TS open/short/chamber | TS raw、cell/IC temp、power state、fault | 失效禁止规定路径；温度阈值/回差准确 |
| PDSG | VOUT、PDSG/DSG VGS、R40 current/temp | success/timeout/failure/retry 均安全；热裕量通过 |
| shutdown/wake/hot-plug | provenance、BQ online、SC/BQ outputs | 只有合法 evidence 唤醒；普通通信故障不唤醒 |
| Task/fault/watchdog | task IDs/HWM/deadline、PSTOP、gates、reset record | 每种 hang/fault 都进入安全态并保留正确原因 |
| EEPROM power-cut | slots/CRC/sequence/SOC/SOH | 任意切电点恢复有效槽，不采用半写记录 |

## Release judgment

双轴结论必须分开：

- `软件实现/静态契约：100/100`。冻结工作区可复现的代码级 P0/P1=0；Host 5/5、三套 ARM 各 4/4、Flash/RAM、heap/stdio symbol、repo hygiene、format、changed-C analyzer、HardFault objdump 与 Keil XML/member 门禁均通过。
- `无人值守量产发布：不得宣称 100`。软件证据不能替代充放电、短路、温度、PDSG、唤醒、watchdog 和 BQ POR/reconnect HIL；SC 物理 I2C line order 仍为 `Conflict`，实际限流/保护参数仍待标定，实际 Keil clean build/远端 CI artifact 未归档，且无独立 MCU 主 FET gate 是硬件安全上限。

因此当前软件轴可以按 100 分收口；产品发布轴只能进入冻结 SHA 的受控 HIL、Keil 等价构建和硬件安全目标评审阶段。
