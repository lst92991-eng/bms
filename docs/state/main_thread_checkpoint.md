# 主线程状态检查点

更新时间：2026-06-23（G6，已按 G5 交接包完成接班确认）

本文档是主线程发生上下文压缩、恢复、中断或被用户指出跑偏后的第一读取入口。主线程不能依赖压缩前记忆继续执行，必须先读取本文件和 `docs/rules/parallel_work_mechanism.md`。

## 0.9 G5 Replacement Freeze：2026-06-23 长线程提示交接包

- 触发原因：Codex 再次提示长线程和多次压缩会降低准确性，符合 `docs/rules/parallel_work_mechanism.md` 中正式主线程替代/交接硬触发条件。
- 冻结原则：停止签发新任务卡；不做代码编辑；只更新本状态交接包并向用户确认当前应当换代。
- 主线程世代：`G5-freeze`。后续新主线程接班时应登记为 `G6`，并执行 Replacement Gate。
- 最近用户目标：用户刚询问“旧项目低功耗怎么做的？”，本轮已按多 agent 只读方式核对并在聊天中回答；该问题不落业务文档。
- 当前模块/门禁：无正在推进的代码模块；当前是项目状态交接，不进入 INT/COM/APP。
- 本轮已读取：`README.md`、`docs/rules/parallel_work_mechanism.md`、`docs/state/main_thread_checkpoint.md`、`docs/rules/hardware_rules.md`。
- 最近只读 agent：
  - `OLD_LOW_POWER_001` / Hume / `019ef288-b713-7bb1-aea2-d96494ed7e34`：已返回并关闭；无写权限；结论为旧项目没有使用 STM32 STOP/STANDBY/SLEEP，低功耗/关机设计主要是拉低 `STM32_POWER_EN` 断外部自保持供电；`upsman_task` 在当前可见旧代码中未创建，因此该逻辑写了但未实际调度。
- 最近聊天结论：
  - 旧项目欠压关机入口为 `App_UpsMan_McuPowerOffOnUV()`，任意单体低于 `TLB_SHUTDOWN_VOLTAGE = 2.70V` 时拉低 `STM32_POWER_EN`。
  - `POWER_24V` 仅用于掉电告警，不直接关机。
  - `Int_BQ769_Ship()` 主要用于 BQ 初始化复位流程，旧项目欠压关机时没有先让 BQ 进入 ship。
  - 旧项目事实只能作为分层风格参考；新项目不能继承旧 GPIO、旧 BQ76930 寄存器或旧 MCU 外设。
- 当前写权限锁：无活动代码写锁。
- 当前活动 agent：无需要等待的活动 agent；所有本轮相关 agent 均已返回。
- 未合并 worker 输出：无。
- 禁止下一步：
  - 禁止在 G5 freeze 后继续签发 worker 或改代码。
  - 禁止直接进入 COM/APP。
  - 禁止把旧项目低功耗实现当作新项目硬件事实。
  - 禁止未重新读取当前模块 pre 就修改 `Int/*.c/.h`。
- 新主线程唯一下一步：
  - 开新线程或压缩恢复后，先执行 Replacement Gate：重新读取 `README.md`、`docs/rules/parallel_work_mechanism.md`、`docs/rules/hardware_rules.md`、本文件，并按用户最新消息确认下一步。
  - 若用户要求继续硬件电源/关机方向，建议先进入 `docs/pre/int_gpio_board_pre.md` 或板级电源/GPIO pre，定义 `BMS_WAKE`、`BMS_CHIP_SHUT`、`BMS_CHIP_ONLINE`、`BMS_INT/ALERT`、LED、蜂鸣器、按键等 INT 边界；不得把 APP 关机策略写进 BQ 通信 INT。

## 0.10 G6 接班确认：按 G5 交接包继续

- 主线程世代：`G6`。
- 接班时间：2026-06-23。
- 接班触发：用户明确要求“读取状态文件，按 G5 交接包继续”。
- Replacement Gate 已读取：`README.md`、`docs/rules/parallel_work_mechanism.md`、`docs/rules/hardware_rules.md`、`docs/state/main_thread_checkpoint.md`。
- 当前工作区检查：`docs/pre/int_bq76952_comm_pre.md`、`docs/pre/int_sc8815_comm_pre.md`、`docs/state/main_thread_checkpoint.md` 存在已修改内容，`docs/logic/` 为未跟踪目录；这些视为当前工作区事实，不回滚。
- 当前活动 agent：无。
- 当前写权限锁：无活动写锁。
- 未合并 worker 输出：无。
- 当前模块/门禁：无正在推进的代码模块；G6 当前只完成接班确认。
- 最近可用结论：旧项目低功耗/关机不是 STM32 STOP/STANDBY/SLEEP，而是通过 `STM32_POWER_EN` 断外部自保持供电；该结论仅作旧项目风格/策略参考，不能作为新项目硬件事实。
- 接班后唯一动作：等待用户明确下一步；若用户继续硬件电源/关机方向，优先进入板级 GPIO/电源控制 `pre`，覆盖 `BMS_WAKE`、`BMS_CHIP_SHUT`、`BMS_CHIP_ONLINE`、`BMS_INT/ALERT`、LED、蜂鸣器、按键等 INT 边界。
- 禁止下一步：禁止直接进入 COM/APP；禁止未读取对应 `pre` 就修改 `Int/*.c/.h`；禁止把 APP 关机策略塞入 BQ/SC 通信 INT；禁止把旧项目 GPIO 或 BQ76930 寄存器当作新项目事实。

## 0.11 G6 BQ INT 用户接口提案只读评估

- 当前用户目标：评估用户提出的 BQ76952 INT public API：`Wake`、`Shutdown`、`Reset`、direct/subcommand/Data Memory 读写、`Enter/ExitConfigUpdate`、`ReadStatus`、`ClearAlarm` 是否合理。
- 当前门禁：API Boundary Review，只读评估；不改 `Int/*.c/.h`，不改 BSP，不进入 COM/APP。
- 本轮执行前已重新读取：`README.md`、`docs/rules/parallel_work_mechanism.md`、`docs/state/main_thread_checkpoint.md`、`docs/rules/hardware_rules.md`、`docs/pre/int_bq76952_comm_pre.md`。
- 当前补充读取：`docs/logic/hardware_interface_reservation.md`、`Int/Int_BQ76952.h`、`Int/Int_BQ76952_BSP.h` 中 status/alarm/reset/shutdown/CRC/subcommand 相关符号。
- 只读任务卡：
  - `BQ_INT_API_EVAL_001` / Schrodinger / `019ef30d-0ca0-7493-8a51-8a3fcb64d9cc`：已返回并关闭，只读，无写权限；结论为用户提案中 direct/subcommand/Data Memory/ConfigUpdate 方向合理，但缺少 CRC 开关，且 `Wake/Shutdown/Reset` 与 `ReadStatus/ClearAlarm` 需要按“板级硬件动作”和“语义化状态/告警”重新拆边界。
- 当前写权限锁：无代码写锁。
- 禁止下一步：禁止未获用户确认就改 `docs/pre/int_bq76952_comm_pre.md`；禁止未签发代码 worker 就改 `Int/Int_BQ76952.h/.c`；禁止把 BQ 板级 GPIO 与通信原语混写成不可拆的实现。
- 本轮主线程合并判断：
  - 建议保留：`ReadDirect`、`WriteDirect`、`ReadSubcommand`、command-only `SendSubcommand`、`ReadDataMemory`、`WriteDataMemory`、`EnterConfigUpdate`、`ExitConfigUpdate`。
  - 建议补回：`SetCrcEnabled`、`IsCrcEnabled`，因为 CRC/PEC 是通信格式能力，属于 INT 通信层。
  - 建议暂不放入通信 INT：`Wake`、`Shutdown`、`Reset`，除非先明确它们是板级 GPIO 动作还是 BQ subcommand 语义封装。
  - 建议上移或改成原语：`ReadStatus`、`ClearAlarm`，因为状态聚合和告警清除时机容易跨入 COM/APP。
- 本轮唯一下一步：在聊天中给用户评估；若用户确认采纳，再更新 `docs/pre/int_bq76952_comm_pre.md`，随后才签发代码 worker 任务卡。

## 0.12 G6 BQ INT 通信与板级信号混合 API 评估

- 当前用户目标：根据硬件信息评估新版 BQ76952 INT API，其中包含 CRC、BQ 板级生命周期 GPIO/命令 wrapper、在线/告警信号读取、direct/subcommand/Data Memory 和 ConfigUpdate。
- 当前门禁：API Boundary Review，只读评估；不改 `Int/*.c/.h`，不改 BSP，不进入 COM/APP。
- 本轮执行前已重新读取：`README.md`、`docs/rules/parallel_work_mechanism.md`、`docs/state/main_thread_checkpoint.md`、`docs/rules/hardware_rules.md`、`docs/pre/int_bq76952_comm_pre.md`。
- 本轮补充读取/检索：`docs/logic/hardware_interface_reservation.md`、`full_netlist (4).csv`、`full_netlist (5).csv`、`Int/Int_BQ76952_BSP.h`、`Int/Int_BQ76952.h/.c` 中 BQ GPIO、CRC、SHUTDOWN/RESET、ALARM 相关符号。
- 只读任务卡：
  - `BQ_INT_API_BOARD_MIX_EVAL_002` / Parfit / `019ef314-6de4-7ab0-b811-889b70607b8e`：已返回并关闭，只读，无写权限；结论为通信原语部分合理，`Wake/Shutdown/Reset/IsOnline/IsAlertAsserted` 属板级 BQ 控制/信号，应优先拆到板级 GPIO/电源控制 pre，若强行留在 `Int_BQ76952` 必须明确为 board-specific wrapper 并等 GPIO 宏名/电平冻结。
- 本轮硬件事实：
  - 网表确认 U26 跨板接口含 `BMS_CHIP_ONLINE`、`BMS_WAKE`、`MUC_EXTI4_PB4_BMS_INT`、`MCU_I2C1_SCL_PB6_BMS`、`MCU_I2C1_SDA_PB7_BMS`、`BMS_CHIP_SHUT`。
  - `PB3 = BMS_WAKE`，`PB4 = BMS_INT/ALERT`，`PB6/PB7 = BQ I2C1`，`REG18 -> BMS_CHIP_ONLINE`。
  - `Int_BQ76952_BSP.h` 已定义 `BQ76952_SUBCMD_SHUTDOWN` 与 `BQ76952_SUBCMD_RESET`，因此裸 `Shutdown/Reset` 命名存在“板级 GPIO 动作”和“BQ subcommand 动作”歧义。
- 当前写权限锁：无代码写锁。
- 本轮主线程合并判断：
  - `SetCrcEnabled/IsCrcEnabled`、direct/subcommand/Data Memory、`Enter/ExitConfigUpdate` 可作为 BQ 通信 INT 正式 API。
  - `Wake/IsOnline/IsAlertAsserted` 是板级 BQ 信号能力，应归 `Int_GpioBoard` 或单独 `Int_BQ76952_Board`；不建议混入 `Int_BQ76952` 通信 pre。
  - `Shutdown/Reset` 必须拆名：若是 BQ 子命令，用 `SendSubcommand(BQ76952_SUBCMD_*)` 或后续危险 wrapper；若是板级 `BMS_CHIP_SHUT`，应带 `Board/ChipShut/Gpio` 语义。
  - `IsAlertAsserted()` 若只读 MCU GPIO 或 ISR flag，不违反 ISR 规则；若内部访问 I2C 读 BQ 状态，则违反 ISR 只置标志和 INT 通信边界。
- 本轮唯一下一步：向用户给出评估；若用户接受拆分，先更新 BQ 通信 pre 或新建板级 GPIO/BQ board-control pre，再进入代码任务卡。

## 0.13 G6 BQ INT 板级硬件操作覆盖评估

- 当前用户纠偏：板级适配本质是操作硬件引脚，因此放在 INT 层没有问题；本轮重点不是反对板级接口进 INT，而是评估接口能否覆盖整个项目需求。
- 当前门禁：API Boundary Review，只读评估；不改 `Int/*.c/.h`，不改 BSP，不进入 COM/APP，不更新 `docs/pre/*`。
- 本轮执行前已重新读取：`README.md`、`docs/rules/parallel_work_mechanism.md`、`docs/state/main_thread_checkpoint.md`、`docs/rules/hardware_rules.md`、`docs/pre/int_bq76952_comm_pre.md`。
- 只读任务卡：
  - `BQ_INT_BOARD_COVERAGE_003` / Galileo / `019ef31f-ff72-7c83-94ec-9b8ae5c21028`：已返回并关闭，只读，无写权限；结论为当前 10 个通信 API 覆盖 BQ I2C1/direct/subcommand/Data Memory/ConfigUpdate，但不覆盖 `BMS_WAKE`、`BMS_CHIP_SHUT`、`BMS_CHIP_ONLINE`、`BMS_INT/ALERT` 等板级线。
  - `BQ_INT_API_LAYER_004` / Kant / `019ef320-458c-76c2-aa2f-a4abe748d54d`：已返回并关闭，只读，无写权限；结论为板级接口属于 INT，但应与通信原语拆命名/拆边界，避免 `Shutdown/Reset` 同时表示 BQ subcommand 和板级 GPIO 动作。
- 本轮主线程合并判断：
  - `SetCrcEnabled/IsCrcEnabled`、`ReadDirect/WriteDirect`、`SendSubcommand/ReadSubcommand`、`ReadDataMemory/WriteDataMemory`、`EnterConfigUpdate/ExitConfigUpdate` 足以支撑后续 COM/APP 访问 BQ76952 内部寄存器、子命令、Data Memory 和配置窗口。
  - 仅上述通信 API 不能覆盖整个项目的 BQ 相关硬件需求；还必须在 INT 层预留板级硬件操作接口，覆盖 `BMS_WAKE`、`BMS_CHIP_SHUT`、`BMS_CHIP_ONLINE`、`BMS_INT/ALERT`。
  - 板级适配可以属于 INT，但建议单独形成 `Int_BQ76952_Board` 或 `Int_GpioBoard` 边界；如果用户要求放在同一 `Int_BQ76952.h`，也必须用分组和命名区分通信原语与板级 GPIO/信号。
  - 不建议使用裸 `Shutdown()`/`Reset()` 名字作为最终接口，因为它们会和 `BQ76952_SUBCMD_SHUTDOWN`、`BQ76952_SUBCMD_RESET` 产生歧义。
  - 不建议新增 `ReadStatus/ClearAlarm` 到 INT；状态聚合和清告警时机属于 COM/APP，COM 可基于 direct/subcommand 和 BSP 常量实现语义化接口。
- 当前推荐方向：BQ INT 应分两组覆盖需求：通信访问组负责 BQ 官方访问模型；板级控制组负责 BMS_WAKE、CHIP_SHUT、ONLINE、ALERT 这些引脚/信号。这样既满足“板级适配属于硬件操作”，也避免 INT 变成业务服务层。
- 当前写权限锁：无代码写锁；本轮只读评估，无文件代码改动。
- 本轮唯一下一步：在聊天中给用户接口覆盖评估；若用户认可，下一步先更新或新建板级 BQ/GPIO `pre`，再签发代码 worker 任务卡。

## 0.14 G6 先写 BQ、暂缓 SC 的硬件解耦评估

- 当前用户问题：严格根据硬件信息，若现在先写 BQ76952 芯片逻辑、暂时不做 SC8815 相关逻辑，是否冲突，硬件上是否解耦。
- 当前门禁：Hardware/Bring-up Order Review，只读评估；不改 `Int/*.c/.h`，不改 BSP，不进入 COM/APP，不更新 `docs/pre/*`。
- 本轮执行前已重新读取：`README.md`、`docs/rules/parallel_work_mechanism.md`、`docs/state/main_thread_checkpoint.md`、`docs/rules/hardware_rules.md`、`docs/logic/hardware_interface_reservation.md`、`docs/pre/int_bq76952_comm_pre.md`、`docs/pre/int_sc8815_comm_pre.md`，并检索 `full_netlist (4).csv`、`full_netlist (5).csv`。
- 只读任务卡：
  - `BQ_FIRST_HW_DECOUPLE_001` / Planck / `019ef325-97ca-7ed2-b574-6cf551c17fd8`：已返回并关闭，只读，无写权限；结论为 BQ 与 SC 在 MCU 接口层不共享 I2C、控制脚或中断脚，先写 BQ 通信逻辑不依赖 SC 通信层。
  - `BQ_FIRST_BRINGUP_ORDER_002` / Dalton / `019ef325-d7c4-76c2-80ac-85ecb2dbe1eb`：已返回并关闭，只读，无写权限；结论为先 BQ 后 SC 可行，但不能完全忽略 SC 控制脚默认态，必须确保 `PB1/#CE=high`、`PB0/PSTOP=high`。
- 本轮直接硬件事实：
  - BQ 接口：`PB6/PB7 = I2C1`，`PB4 = BMS_INT/ALERT`，`PB3 = BMS_WAKE`，`BMS_CHIP_SHUT`、`BMS_CHIP_ONLINE` 经 U26 跨板接口连接。
  - SC 接口：`PA7/PA6` 软件 IIC，`PA5` INT，`PB1=#CE`，`PB0=PSTOP`。
  - SC 安全态：`#CE` 高电平 disable，`PSTOP` 高电平 standby；`R19/R46` 均上拉到 `SYS_3V3`。
  - BQ 与 SC 共享系统级电源/电池功率路径，但 MCU 通信/控制接口相互独立。
- 本轮主线程合并判断：
  - 可以先写 BQ76952 INT 通信/底层访问逻辑，不必等待 SC8815 通信、ADC、guard、状态读取实现完成。
  - 该结论只适用于 BQ INT 通信和低层能力预留；不代表现在可以直接写 BQ APP 均衡策略。
  - 暂缓 SC 逻辑时，仍必须在板级安全初始化或 GPIO 默认配置中保证 `PB1/#CE=high`、`PB0/PSTOP=high`，PA6/PA7 释放为高阻/开漏高，避免 SC 自动进入充电路径。
  - SC 充电链路 bring-up 前仍需确认 `R17/R18=100kΩ/5.1kΩ` 实物改焊/DNP、软件 IIC、`#CE/PSTOP`、guard 等；这些不阻塞先写 BQ 通信层，但阻塞启动充电。
- 当前推荐顺序：先推进 BQ 通信层可以；紧随其后应补最小板级 GPIO/安全初始化 pre，覆盖 SC `#CE/PSTOP` 安全态和 BQ `WAKE/CHIP_SHUT/ONLINE/ALERT`；完整 SC 通信层可后置到充电链路 bring-up 前。
- 当前写权限锁：无代码写锁；本轮只读评估，无文件代码改动。
- 本轮唯一下一步：在聊天中回答用户；若用户确认先 BQ，则下一步按 BQ 当前 pre 签发代码 worker 或先补板级 GPIO 安全态 pre，二者不能混成一个无边界任务。

## 0.15 G6 BQ76952 INT 教学可读性改写任务登记

- 当前用户目标：用户确认现在可以改写 BQ INT 层；参考旧项目，但为了教学目的，代码要精简、易读。
- 当前门禁：Implementation Gate，当前只推进 `Int_BQ76952` 一个模块；不改 SC，不进入 COM/APP。
- 本轮执行前已重新读取：`README.md`、`docs/rules/parallel_work_mechanism.md`、`docs/state/main_thread_checkpoint.md`、`docs/rules/hardware_rules.md`、`docs/pre/int_bq76952_comm_pre.md`。
- 本轮补充读取：`Int/Int_BQ76952.h`、`Int/Int_BQ76952.c`、`Int/Int_BQ76952_BSP.h`、旧项目 `Int_BQ769.h/.c`、`docs/review/int_bq76952_comm_review.md`。
- 只读学习/审查任务：
  - `BQ_INT_OLD_STYLE_LEARN_001` / Hubble / `019ef32a-9485-7332-833f-8f603ef91fed`：已返回并关闭，只读，无写权限；结论为可继承旧项目“薄封装、短注释、上层组合业务”的风格，不能继承 FreeRTOS、printf、旧寄存器、旧硬件。
  - `BQ_INT_BOUNDARY_REVIEW_001` / Kierkegaard / `019ef32a-d57c-7082-a92d-edd1763ffec9`：已返回并关闭，只读，无写权限；结论为当前 BQ public API 越过最新 pre，需收敛到 10 个正式 API，`ReadDeviceNumber` 仅可作为默认关闭的 debug API。
- 本轮主线程合并判断：
  - 保留 pre 的 10 个正式 API：CRC 开关、direct read/write、subcommand send/read、Data Memory read/write、Enter/ExitConfigUpdate。
  - 移除正式 public API：`ReadBatteryStatus`、`ReadAlarmStatus`、`ClearAlarmStatus`、`ReadFetStatus`、`ReadCellVoltage`。
  - `ReadDeviceNumber` 若保留，必须受 `INT_BQ76952_ENABLE_BRINGUP_API` 保护且默认关闭。
  - `EnterConfigUpdate`/`ExitConfigUpdate` 不得再依赖 public `ReadBatteryStatus`，应改为内部 `static` CFGUPDATE 读取 helper。
  - 私有 CRC/checksum/echo/transfer helper 有协议边界价值，原则上保留；目标是收 public API 和注释，不是机械合并私有函数。
- 代码 worker 任务卡：
  - 任务卡编号：`BQ76952_INT_REWRITE_001`
  - 模块：`Int_BQ76952`
  - 当前门禁：Implementation Gate
  - 输入文件：`docs/pre/int_bq76952_comm_pre.md`、`docs/rules/hardware_rules.md`、`Int/Int_BQ76952_BSP.h`、旧项目 `Int_BQ769.h/.c`
  - 允许写入：`Int/Int_BQ76952.h`、`Int/Int_BQ76952.c`
  - 禁止修改：`Int/Int_BQ76952_BSP.h`、`Int/Int_SC8815*`、`docs/pre/*`、`docs/rules/*`、旧项目、COM/APP
  - 验收标准：`.h` 只暴露 10 个正式 API，debug `ReadDeviceNumber` 条件编译默认关闭；无语义化 status/alarm/FET/cell public API；`.c` 无 FreeRTOS/printf/动态内存；不写 `BQ76952_CELL_MASK_6S_HW_DRAFT`；不新增业务策略。
- 当前写权限锁：
  - `BQ76952_INT_REWRITE_001` / `019ef32c-fb46-7230-bf52-c49e51fc44c1` Goodall / `Int_BQ76952` / 文件级锁 / 允许写入 `Int/Int_BQ76952.h`, `Int/Int_BQ76952.c` / 状态 `revoked` / 429 失败，无可合并输出，迟到输出不得直接合并。
- 禁止下一步：禁止并行给其他 worker 分配 `Int/Int_BQ76952.h/.c`；禁止顺手改 BSP、pre、SC、COM/APP；禁止新增板级 GPIO API 到本任务。

## 0.16 G6 BQ76952 INT 改写重试任务登记

- 触发原因：`BQ76952_INT_REWRITE_001` 代码 worker 因 429 失败，未返回可合并输出；原写锁已显式撤销。
- 当前门禁：Implementation Gate，仍只推进 `Int_BQ76952` 一个模块。
- 代码 worker 任务卡：
  - 任务卡编号：`BQ76952_INT_REWRITE_002`
  - 模块：`Int_BQ76952`
  - 当前门禁：Implementation Gate
  - 输入文件：`docs/pre/int_bq76952_comm_pre.md`、`docs/rules/hardware_rules.md`、`Int/Int_BQ76952_BSP.h`、旧项目 `Int_BQ769.h/.c`
  - 允许写入：`Int/Int_BQ76952.h`、`Int/Int_BQ76952.c`
  - 禁止修改：`Int/Int_BQ76952_BSP.h`、`Int/Int_SC8815*`、`docs/pre/*`、`docs/rules/*`、`docs/state/*`、旧项目、COM/APP
  - 验收标准：`.h` 只暴露 10 个正式 API，debug `ReadDeviceNumber` 条件编译默认关闭；无语义化 status/alarm/FET/cell public API；`.c` 无 FreeRTOS/printf/动态内存；不写 `BQ76952_CELL_MASK_6S_HW_DRAFT`；不新增业务策略；`Enter/ExitConfigUpdate` 使用私有 CFGUPDATE 读取 helper。
- 当前写权限锁：
  - `BQ76952_INT_REWRITE_002` / worker 待派发 / `Int_BQ76952` / 文件级锁 / 允许写入 `Int/Int_BQ76952.h`, `Int/Int_BQ76952.c` / 状态 `requested` / 释放条件：worker 返回后经主线程越权检查和只读复核。
- 禁止下一步：禁止并行给其他 worker 分配 `Int/Int_BQ76952.h/.c`；禁止顺手改 BSP、pre、SC、COM/APP。

## 0.17 G7 Replacement Gate 与 BQ76952 INT 改写接班确认

- 接班触发：当前对话出现 handoff summary，符合 `docs/rules/parallel_work_mechanism.md` 的正式接班触发条件。
- 主线程世代：`G7`。
- 接班时间：2026-06-23。
- 用户最新目标：继续改写 `Int_BQ76952` INT 通信层，参考旧项目薄封装风格，但以教学可读、精简、易审查为目标。
- 当前模块/门禁：`Int_BQ76952` / Implementation Gate；本轮只推进这一组 `.h/.c`。
- Replacement Gate 已重新读取：`README.md`、`docs/rules/parallel_work_mechanism.md`、`docs/rules/hardware_rules.md`、`docs/state/main_thread_checkpoint.md`、`docs/pre/int_bq76952_comm_pre.md`。
- 本轮补充读取：`Int/Int_BQ76952.h`、`Int/Int_BQ76952.c`、`Int/Int_BQ76952_BSP.h`、旧项目 `Int_BQ769.h/.c`、`git status --short`。
- 当前工作区事实：`docs/pre/int_bq76952_comm_pre.md`、`docs/pre/int_sc8815_comm_pre.md`、`docs/state/main_thread_checkpoint.md` 已修改，`docs/logic/` 未跟踪；这些改动不回滚，后续只在授权范围内继续写。
- 当前冻结边界：正式 public API 只保留 pre 中 10 个接口；`ReadDeviceNumber` 只能在 `INT_BQ76952_ENABLE_BRINGUP_API` 打开时出现；`ReadBatteryStatus`、`ReadAlarmStatus`、`ClearAlarmStatus`、`ReadFetStatus`、`ReadCellVoltage` 不得作为正式 public API。
- 当前写锁处理：`BQ76952_INT_REWRITE_002` 仍是唯一待派发写锁；派发前不得给其他 worker 分配 `Int/Int_BQ76952.h/.c`。
- 唯一下一步：派发 `BQ76952_INT_REWRITE_002` 代码 worker，写入范围仅限 `Int/Int_BQ76952.h`、`Int/Int_BQ76952.c`。
- 禁止下一步：禁止改 `Int/Int_BQ76952_BSP.h`、SC、COM/APP、`docs/pre/*`、`docs/rules/*`；禁止新增板级 GPIO API 到本通信层任务。

## 0.18 G7 BQ76952_INT_REWRITE_002 写锁派发

- 任务卡编号：`BQ76952_INT_REWRITE_002`。
- 持有者：Fermat / `019ef333-634c-7a52-a515-d929115eb9fc`。
- 模块：`Int_BQ76952`。
- 门禁：Implementation Gate。
- 锁粒度：文件级独占锁。
- 允许写入文件：`Int/Int_BQ76952.h`、`Int/Int_BQ76952.c`。
- 允许修改函数/代码块：整个 `Int_BQ76952` 通信层 public API 声明与对应实现；仅限 pre 允许的 10 个正式 API 和受 `INT_BQ76952_ENABLE_BRINGUP_API` 保护的 `ReadDeviceNumber`。
- 禁止写入文件：`Int/Int_BQ76952_BSP.h`、`Int/Int_SC8815*`、`docs/pre/*`、`docs/rules/*`、`docs/state/*`、旧项目目录、COM/APP。
- 基线：派发前已读取当前 `Int/Int_BQ76952.h/.c`，其中存在 16 个 public API；pre 要求收敛到 10 个正式 API + 默认关闭的 bring-up API。
- 租约：当前回合结束前返回；如失败或 429，必须由主线程显式标记 revoked。
- 状态：`released`。
- 返回结果：Fermat 报告仅修改 `Int/Int_BQ76952.h`、`Int/Int_BQ76952.c`；默认 public API 已收敛到 10 个正式接口，`ReadDeviceNumber` 已加 `INT_BQ76952_ENABLE_BRINGUP_API` 条件编译，CFGUPDATE 轮询改用 private helper；本机未找到可用 C 编译器。
- 释放说明：主线程越权检查、pre 一致性检查和 Hooke 只读复核均未发现 blocking；写锁释放。

## 0.19 G7 REVIEW_BQ76952_INT_REWRITE_002 只读复核派发

- 任务卡编号：`REVIEW_BQ76952_INT_REWRITE_002`。
- 持有者：Hooke / `019ef338-9652-7fc0-8838-cde5ef970c62`。
- 角色：Review/Safety agent。
- 模块：`Int_BQ76952`。
- 门禁：Review Gate。
- 写权限：无。
- 允许读取：`README.md`、`docs/rules/parallel_work_mechanism.md`、`docs/rules/hardware_rules.md`、`docs/state/main_thread_checkpoint.md`、`docs/pre/int_bq76952_comm_pre.md`、`Int/Int_BQ76952.h`、`Int/Int_BQ76952.c`、`Int/Int_BQ76952_BSP.h`。
- 审查重点：授权范围、pre 一致性、非正式 public API 残留、CFGUPDATE 私有 helper、跨层业务/FreeRTOS/printf/动态内存/板级 GPIO/BQ76930 事实污染、教学可读性。
- 状态：`returned/released`。
- 复核结论：Hooke 未发现 blocking；确认 BQ `.h/.c` 默认只暴露 10 个正式 API，`ReadDeviceNumber` 受条件编译保护，非正式状态/告警/FET/cell API 无外部符号残留，未发现 FreeRTOS/printf/动态内存/板级 GPIO/旧 BQ76930 事实污染。
- 释放说明：只读复核完成，无写权限需要释放。

## 0.20 G7 BQ76952 INT review 文档同步

- 更新文件：`docs/review/int_bq76952_comm_review.md`。
- 更新原因：旧 review 文档仍记录“16 个 public API”，与最新 `pre` 和 `Int_BQ76952.h/.c` 不一致。
- 更新内容：改为记录 10 个正式 public API + 条件编译 `ReadDeviceNumber`；记录被移出 INT public API 的 `ReadBatteryStatus`、`ReadAlarmStatus`、`ClearAlarmStatus`、`ReadFetStatus`、`ReadCellVoltage`；记录 CFGUPDATE 已改用 private helper。
- 当前门禁：Review Gate 已完成代码级复核；Freeze Gate 尚未完成真实编译和硬件验证。
- 当前唯一下一步：运行最终静态检查并汇报结果；若用户继续，可进入下一个明确 INT/board GPIO pre，不得直接进入 COM/APP。

## 0.21 G7 BQ76952_INT_REWRITE_002 主工作区合并修正

- 触发原因：最终本地 `rg` 发现主工作区 `Int/Int_BQ76952.c` 仍残留旧的 `ReadBatteryStatus`、`ReadAlarmStatus`、`ClearAlarmStatus`、`ReadFetStatus`、`ReadCellVoltage` 实现，与 Fermat/Hooke 报告不一致。
- 处理原则：不把 agent 输出直接视为已合并事实；主线程按同一任务卡授权范围执行合并，只修改 `Int/Int_BQ76952.c`。
- 合并内容：补入私有 `Int_BQ76952_ReadCfgUpdateBit()`；`ReadDeviceNumber()` 加 `INT_BQ76952_ENABLE_BRINGUP_API` 条件编译；删除非正式状态/告警/FET/cell 外部函数；`EnterConfigUpdate()` / `ExitConfigUpdate()` 改用私有 CFGUPDATE helper。
- 越权检查：未修改 BSP、SC、COM/APP、`docs/pre/*`、`docs/rules/*` 或旧项目目录。
- 当前状态：等待最终本地静态检查；检查通过后 `BQ76952_INT_REWRITE_002` 维持 released，若失败则回到 needs_fix。

## 0.22 G7 REVIEW_BQ76952_INT_REWRITE_003 最终只读复核派发

- 任务卡编号：`REVIEW_BQ76952_INT_REWRITE_003`。
- 持有者：Pauli / `019ef340-7761-7672-9b80-302bd3ac687c`。
- 角色：Review/Safety agent。
- 写权限：无。
- 复核目标：只看当前主工作区真实文件，复核 `Int_BQ76952.h/.c` 和 `docs/review/int_bq76952_comm_review.md` 是否与 pre 一致，是否仍残留非正式 public API 或跨层/旧项目污染。
- 状态：`returned/released`。
- 复核结论：当前主工作区真实文件通过最终只读复核；`Int_BQ76952.h` 默认 10 个正式 API，`ReadDeviceNumber` 仅在 `INT_BQ76952_ENABLE_BRINGUP_API` 下；`Int_BQ76952.c/.h` 无非正式 status/alarm/FET/cell API 残留；未发现 FreeRTOS、printf、动态内存、HAL GPIO、旧 BQ76930、`BQ76952_CELL_MASK_6S_HW_DRAFT` 写入。
- 当前结论：`Int_BQ76952` 代码级 Review Gate 通过；Freeze Gate 尚未完成真实编译、CubeMX 句柄适配和硬件 I2C/CRC/transfer buffer 验证。
- 唯一下一步：向用户汇报本轮结果；若继续推进，需先重新读取 rules/state/hardware/pre 后进入下一个明确任务。

## 0. G3 Replacement Freeze 交接包

- 触发原因：Codex 已提示长线程和多次压缩会降低准确性，符合 `docs/rules/parallel_work_mechanism.md` 中正式主线程替代/交接硬触发条件。
- 冻结时间：2026-06-22。
- 冻结原则：停止签发新任务卡；不做代码编辑；只更新本状态交接包并向用户汇总。
- 最近用户目标：必须多 agent 协作；每次执行前读取 rules/state/pre；当前只读审查 `Int_SC8815.c` 与 `Int_BQ76952.c` 的私有函数数量、必要性、风格、教学可读性和行数取舍。
- 当前模块/门禁：`Int_SC8815` 与 `Int_BQ76952` 源文件只读 Review Gate。
- 已完成只读 agent：
  - `REVIEW_SC8815_C_001` / Euclid / `019eeed7-ea04-7841-a72a-bbf368e2e8e2`：已返回并关闭，未发现 SC8815 blocking；`Int_SC8815.c` 私有函数偏多但多数必要。
  - `REVIEW_BQ76952_C_001` / Carson / `019eeed7-ea7b-78a0-b333-42e1aa738ffe`：已返回并关闭，发现 BQ76952 blocking：CRC public API 与 pre 签名不一致；`SendSubcommand()` 有未使用局部变量 `ret`。
  - `REVIEW_INT_STYLE_001` / Avicenna / `019eeeda-405c-74e0-9829-ea1c5a26d69a`：已返回并关闭，确认 SC8815 helper 数量高但结构性合理，BQ76952 private helper 数量克制。
- 当前写权限锁：无活动代码写锁；本轮 review agent 均无写权限。
- 未合并 worker 输出：无。本轮没有 worker 写代码输出。
- 已确认 blocking：
  - `Int_BQ76952.c/.h` 的 CRC 开关 API 必须与 `docs/pre/int_bq76952_comm_pre.md` 对齐，或先改 pre 再改代码；默认下一步应改代码匹配 pre。
  - `Int_BQ76952_SendSubcommand()` 中未使用局部变量 `ret` 需要移除。
- 取舍结论：
  - SC8815 保留 6 个软件 IIC 底层原语、4 个 IIC 协议 helper、`ReadRegRaw/WriteRegRaw`、`GuardWrite`、ADC/电流换算 helper。
  - SC8815 不建议为了降低数量机械删除 helper；应优先处理 `GuardWrite()` 分段可读性、`ReadAdcCurrentMa()` 重复逻辑、`WriteReg()` 与 `GuardWrite()` 的轻微重复。
  - BQ76952 不存在私有函数过多问题；6 个 private helper 均有协议/校验边界价值，不建议内联。
- 新主线程唯一下一步：执行 Replacement Gate，重新读取 `README.md`、`docs/rules/parallel_work_mechanism.md`、`docs/rules/hardware_rules.md`、本文件、`docs/pre/int_sc8815_comm_pre.md`、`docs/pre/int_bq76952_comm_pre.md`，然后只签发一个代码 worker 任务卡修复 BQ76952 两个 blocking，或等待用户明确要求先处理 SC8815 可读性。
- 禁止下一步：禁止直接进入 COM/APP；禁止修改 `Int/Int_SC8815_BSP.h` 或 `Int/Int_BQ76952_BSP.h`；禁止无任务卡直接改 `Int/Int_SC8815.c` 或 `Int/Int_BQ76952.c`；禁止把 agent 结论当作未经主线程核验的最终事实。

## 0.1 G4 接班确认与当前窄修复

- 主线程世代：`G4`。
- 接班时间：2026-06-22。
- 接班原因：用户要求“继续”，且 G3 因 Codex 长线程提示已执行 Replacement Freeze。
- Replacement Gate 已读取：`README.md`、`docs/rules/parallel_work_mechanism.md`、`docs/rules/hardware_rules.md`、`docs/state/main_thread_checkpoint.md`、`docs/pre/int_sc8815_comm_pre.md`、`docs/pre/int_bq76952_comm_pre.md`。
- 当前唯一动作：收口 `BQ76952_CRC_API_FIX_002` 和 `REVIEW_BQ76952_FIX_003`，记录 BQ76952 两个 blocking 已解除。
- 当前不处理：SC8815 可读性优化、BSP、COM、APP、pre/rules 修改。
- 工作区策略：已有未提交/未跟踪文件视为当前工作区事实，不回滚；本轮只允许代码 worker 改 `Int/Int_BQ76952.h` 和 `Int/Int_BQ76952.c` 中授权函数。
- 复核结果：只读 review agent `019eeee5-5b77-72c3-89a3-c64051bdfd83`（Cicero）已返回并关闭，未发现 blocking；CRC API 已与 `docs/pre/int_bq76952_comm_pre.md` 对齐，`SendSubcommand()` 未使用变量已移除，未发现 `printf`、FreeRTOS 或动态内存。

## 0.2 G4 BQ76952 review 文档同步

- 当前动作：主线程已同步 `docs/review/int_bq76952_comm_review.md`，记录 CRC API 签名修复和 `SendSubcommand()` 未使用变量修复；未改 BQ 代码。
- 本动作原因：review 文档中旧结论“所有 public API 返回状态”与已冻结的 CRC API `void/bool` 签名冲突，需要与 `docs/pre/int_bq76952_comm_pre.md` 和当前代码对齐。
- 多 agent 复核任务：`REVIEW_BQ76952_REVIEW_DOC_001` 已由只读 agent `019eeeea-fcbf-7493-82df-c8fb1844d5f8`（Jason）返回并关闭；无写权限，结论为 review 文档和 BQ 代码在 CRC API、`SendSubcommand()`、禁用项记录上无 blocking。
- 后续主线程修正：根据复核 agent 指出的文字张力，`docs/pre/int_bq76952_comm_pre.md` 验收标准已从“所有 public API 返回状态”改为“除 CRC 配置接口外，通信/读取类 public API 返回状态”。
- 当前冻结判断：`Int_BQ76952` 的 pre、代码、review 在 CRC API 与状态返回描述上已对齐；未执行编译器语法检查，原因仍是本机未发现 `gcc`、`clang`、`cl`。

## 0.3 G4 SC8815 局部可读性优化

- 当前动作：代码 worker `SC8815_READABILITY_001` 已返回并通过只读复核。
- 当前模块/门禁：`Int_SC8815`，Implementation 局部补丁。
- 授权范围：只允许修改 `Int/Int_SC8815.c` 中 `Int_SC8815_WriteReg()` 和 `Int_SC8815_ReadAdcCurrentMa()` 两个函数。
- 结果：`WriteReg()` 保持 `ReadRegRaw -> GuardWrite -> WriteRegRaw`；`ReadAdcCurrentMa()` 改为先选择 `high_reg/low_reg`，再统一读 high/low。
- 禁止：不得新增 public API；不得新增新的软件 IIC 底层原语；不得修改 `Int/Int_SC8815_BSP.h`、`Int/Int_BQ76952*`、`docs/rules/*`、COM/APP；不得改变 guard 安全策略。
- 主线程初查：public API 仍为 12 个；软件 IIC 底层 GPIO 原语仍为 6 个；未发现 `HAL_I2C`、FreeRTOS、`printf`、动态内存；写寄存器路径仍经过 `Int_SC8815_GuardWrite()`。
- 只读复核：`REVIEW_SC8815_READABILITY_001` 已由只读 agent `019eeef2-4046-73c3-b604-c28cf94803d5`（Chandrasekhar）返回并关闭；未发现 blocking，建议释放写锁。
- 剩余说明：工作区存在多项未提交/未跟踪文件，无法仅凭当前 git 状态把所有文件改动归因到某个任务；本轮按状态文件和只读复核记录处理，不回滚既有工作区事实。

## 0.4 G4 硬件设计逻辑与接口预留分析

- 当前用户目标：读取硬件相关信息，分析当前电路设计的关键芯片应该预留哪些软件/硬件接口；BQ 的均衡逻辑后续由软件手写，SC 的充电管理由硬件自动完成。
- 当前门禁：Hardware Fact / Logic Analysis 已整合；不改 `Int/*.c/.h`，不改 BSP，不进入 COM/APP。
- 本轮已重新读取：`README.md`、`docs/rules/parallel_work_mechanism.md`、`docs/rules/hardware_rules.md`、本文件、`docs/pre/int_bq76952_comm_pre.md`、`docs/pre/int_sc8815_comm_pre.md`、`docs/pre/int_bq76952_bsp_pre.md`、`docs/pre/int_sc8815_bsp_pre.md`。
- 当前资料入口：根目录两份原理图 PDF、`full_netlist (4).csv`、`full_netlist (5).csv`、`Int/Int_BQ76952_BSP.h`、`Int/Int_SC8815_BSP.h`、对应 pre/review 与 `docs/agent_reports/*_bsp_register_facts_2026-06-22.md`。
- 只读任务卡：
  - `HW_BQ_INTERFACE_FACTS_001`：已返回并核验整合；BQ76952 侧硬件接口与后续软件均衡预留点，只读，无写权限。
  - `HW_SC_INTERFACE_FACTS_001`：已返回并核验整合；SC8815 侧硬件自动充电边界与 MCU 预留接口，只读，无写权限。
  - `HW_SYSTEM_INTERFACE_FACTS_001`：已返回并核验整合；系统级接口预留，包括 OLED、EEPROM、CAN、调试 GPIO、电源路径，只读，无写权限。
- 当前写权限锁：无新增代码写锁；本轮只允许主线程按需要更新状态/硬件逻辑文档。
- 禁止下一步：禁止把 SC8815 设计成软件充电状态机；禁止把 BQ 均衡策略写进 INT 通信层；禁止根据旧项目硬件连线推导新项目事实；禁止无新 pre 直接新增 public API。
- 本轮输出：已新增 `docs/logic/hardware_interface_reservation.md`，记录 BQ 软件手写均衡的低层能力边界、SC 硬件自动充电边界、系统级接口预留和 bring-up 待确认项。
- 本轮冻结结论：
  - BQ 必须预留 I2C1、ALERT/EXTI、WAKE、ONLINE、SHUT、cell/current/temperature/FET/alarm/balancing status 等低层能力；均衡策略放 COM/APP，不放 INT。
  - SC 必须预留 PA6/PA7 软件 IIC、PA5 INT、PB1 #CE、PB0 PSTOP、状态/ADC/限流能力；充电管理、涓流、终止、保护动作让硬件自动完成，软件不写微观充电状态机。
  - OLED/EEPROM/CAN/GPIO/电源路径只预留底层接口，页面、参数结构、CAN 协议、告警策略放后续 logic/COM/APP。
- 本轮唯一下一步：等待用户审核 `docs/logic/hardware_interface_reservation.md`；若继续推进，建议先写 `docs/pre/int_gpio_board_pre.md`，覆盖 LED、蜂鸣器、按键、BMS wake/shut/alert/online 的 INT 边界。

## 0.5 G4 INT 最小硬件接口边界审查

- 当前用户目标：在“业务逻辑在 APP，INT 只留对应操作硬件的接口”的新要求下，评估当前 BQ76952/SC8815 INT public API 是否应精简。
- 当前门禁：API Boundary Review 已完成；不改 `Int/*.c/.h`，不改 BSP，不进入 COM/APP。
- 本轮已重新读取：`README.md`、`docs/rules/parallel_work_mechanism.md`、`docs/state/main_thread_checkpoint.md`、`docs/rules/hardware_rules.md`、`docs/pre/int_bq76952_comm_pre.md`、`docs/pre/int_sc8815_comm_pre.md`、`docs/logic/hardware_interface_reservation.md`、两份 INT 头文件和 review。
- 只读任务卡：
  - `REVIEW_BQ_INT_MINIMAL_API_001`：已返回并核验整合；审查 BQ76952 INT 最小硬件操作接口，只读，无写权限。
  - `REVIEW_SC_INT_MINIMAL_API_001`：已返回并核验整合；审查 SC8815 INT 最小硬件操作接口，只读，无写权限。
- 当前写权限锁：无新增代码写锁。
- 禁止下一步：禁止直接删 API；禁止未改 pre 就改代码；禁止把 COM/APP 业务移回 INT；禁止扩展 SC OTG/反向输出能力。
- 本轮结论：
  - 新需求下 INT 可以且应该精简；当前 BQ 16 个 public API 和 SC 12 个 public API 都偏宽。
  - BQ 最小正式 API 应收敛为 CRC 开关和 direct/subcommand/Data Memory 访问机制；语义化读取如 battery/alarm/FET/cell voltage 可上移到 COM/APP 或作为 bring-up helper。
  - SC 最小正式 API 应保留安全态、#CE、PSTOP、状态读取、ADC 监测和限流；`ReadReg/WriteReg/UpdateReg` 不建议作为正式 public API，可降为 bring-up/debug 条件接口或改私有。
  - 两个模块都应先修改对应 `pre` 文档，冻结“正式最小 API + bring-up/debug 可选 API”边界，再开代码任务卡改 `.h/.c`。
- 本轮唯一下一步：等待用户确认是否按最小 API 方案先改 `docs/pre/int_bq76952_comm_pre.md` 与 `docs/pre/int_sc8815_comm_pre.md`；确认后再签发代码精简任务卡。

## 0.6 G4 BQ/SC INT 最小 API pre 更新

- 当前用户目标：先输出最新 pre 文档供审核，说明 INT 层在新需求下应该保留什么、删除/上移什么，以及原因和逻辑。
- 当前门禁：API Gate 文档更新已完成；不改 `Int/*.c/.h`，不改 BSP，不进入 COM/APP。
- 本轮执行前已重新读取：`README.md`、`docs/rules/parallel_work_mechanism.md`、`docs/state/main_thread_checkpoint.md`、`docs/rules/hardware_rules.md`、`docs/pre/int_bq76952_comm_pre.md`、`docs/pre/int_sc8815_comm_pre.md`。
- 只读任务卡：
  - `PRE_BQ76952_MIN_API_REVIEW_001` / Descartes / `019ef203-bf70-7db0-9f58-5524862189ba`：已返回，建议 BQ 正式最小 API 保留 CRC、direct/subcommand/Data Memory 和 CONFIG_UPDATE；语义化读取上移 COM/APP 或 debug。
  - `PRE_SC8815_MIN_API_REVIEW_001` / Ohm / `019ef203-bfff-7162-b9c5-e96472c7bd88`：已返回，建议 SC 正式最小 API 保留安全态、#CE、PSTOP、状态、ADC、限流；`ReadReg/WriteReg/UpdateReg` 只作 debug 条件接口。
- 文档改动：
  - 已更新 `docs/pre/int_bq76952_comm_pre.md`：正式 public API 从旧 16 个收敛为 10 个；`ReadDeviceNumber` 为 bring-up/debug；battery/alarm/FET/cell voltage/clear alarm 上移 COM/APP。
  - 已更新 `docs/pre/int_sc8815_comm_pre.md`：正式 public API 从旧 12 个收敛为 9 个；`ReadReg/WriteReg/UpdateReg` 改为 `INT_SC8815_ENABLE_BRINGUP_API` 条件接口且默认关闭。
- 主线程取舍：
  - BQ `EnterConfigUpdate/ExitConfigUpdate` 保留为正式最小 API，因为它们是 Data Memory 配置写入所需的芯片硬件模式控制，不是业务策略；正式 API 数量为 10，仍低于 API Gate 默认 12 个上限。
  - SC `ReadAdcVoltageMv/ReadAdcCurrentMa` 保留在 INT，因为换算依赖板级分压、电流采样和 BSP 常量，属于硬件适配，不是业务策略。
- 当前写权限锁：无代码写锁；本轮 `docs/pre/*` 由主线程按规则落地。
- 禁止下一步：禁止未获用户审核就改 `.h/.c`；禁止直接进入 COM/APP；禁止在 INT 恢复语义化状态读取或 SC 原始寄存器正式 public API。
- 本轮唯一下一步：等待用户审核两份最新 pre；若用户确认，下一步再签发代码 worker 任务卡分别精简 BQ/SC `.h/.c`，且每张任务卡登记写锁。

## 0.7 G4 旧项目 BQ INT 接口颗粒度核对

- 当前用户目标：核对旧项目 BQ 芯片在 INT 层留下了哪些接口，希望新 BQ76952 INT 尽可能精简到旧项目那种“只保留必要硬件接口”的颗粒度。
- 当前门禁：旧项目风格/接口只读核对；不改 `Int/*.c/.h`，不改 BSP，不进入 COM/APP。
- 本轮执行前已重新读取：`README.md`、`docs/rules/parallel_work_mechanism.md`、`docs/state/main_thread_checkpoint.md`、`docs/rules/hardware_rules.md`、`docs/pre/int_bq76952_comm_pre.md`。
- 只读任务卡：
  - `OLD_BQ_INT_INTERFACE_SCAN_001` / Dalton / `019ef20e-9df5-7960-b70b-7779831be391`：已返回，确认旧项目 BQ INT public API 以 `Int_BQ769.h` 为准，共 6 个声明，其中 `Int_BQ769_Read()` 只声明未实现。
- 旧项目 BQ INT 对外接口事实：
  - `Int_BQ769_WakeUp()`
  - `Int_BQ769_Ship()`
  - `Int_BQ769_Reset()`
  - `Int_BQ769_WriteReg(uint8_t reg, uint8_t data)`
  - `Int_BQ769_ReadReg(uint8_t reg, uint8_t *buff, uint16_t read_len)`
  - `Int_BQ769_Read(uint8_t reg, uint8_t *read_buff, uint8_t read_len)`：未找到 `.c` 实现，视为遗留/废弃声明。
- 旧项目颗粒度结论：
  - 旧 BQ INT 层只封装 wake/ship/reset、寄存器读写和 CRC helper；没有 `ReadCellVoltage`、`ReadAlarmStatus`、`SetBalance`、`SetFet` 等语义化 API。
  - 寄存器地址、位域、阈值、枚举、`RegisterGroup` 都主要放在 `Int_BQ769_BSP.h`；业务层直接用 BSP 宏和 INT 读写原语组合业务。
  - 旧项目 App 直接调用 `Int_BQ769_ReadReg/WriteReg` 和 `BQ769_RegisterGroup`，这只作为“接口颗粒度参考”，不作为新项目跨层设计事实。
- 对新 BQ76952 的影响：
  - 新项目不能复用旧 BQ76930 寄存器事实，但可以参考“INT `.c` 尽量薄，BSP 承载详细寄存器表，业务组合留给上层”的风格。
  - 若按旧项目颗粒度进一步精简，下一版 BQ pre 可把正式 API 从 10 个再压缩，重点讨论是否把 `EnterConfigUpdate/ExitConfigUpdate` 合并到 Data Memory 写流程或降为内部 helper。
- 当前写权限锁：无代码写锁；本轮只读核对，无文件代码改动。
- 禁止下一步：禁止因为旧项目只有 BQ76930 裸寄存器读写，就把 BQ76952 的 subcommand/Data Memory 协议细节错误简化掉；禁止把旧寄存器地址或旧 MCU 外设搬入新项目。
- 本轮唯一下一步：等待用户确认是否按“旧项目式极简”继续修改 `docs/pre/int_bq76952_comm_pre.md`，再开代码精简任务。

## 0.8 G4 BQ INT 边界纠偏：按硬件和需求推导，不按数量压缩

- 当前用户纠偏：用户不认可“压数量”的判断；要求根据当前硬件和需求概述得出 BQ INT 边界结论。
- 当前门禁：硬件需求驱动的 API 边界只读审查；不改 `Int/*.c/.h`，不改 BSP，不进入 COM/APP；暂不改 pre，先修正判断依据。
- 本轮执行前已重新读取：`README.md`、`docs/rules/parallel_work_mechanism.md`、`docs/state/main_thread_checkpoint.md`、`docs/rules/hardware_rules.md`、`docs/logic/hardware_interface_reservation.md`、`docs/pre/int_bq76952_comm_pre.md`、`Int/Int_BQ76952_BSP.h`、BQ BSP 报告与 review。
- 只读任务卡：
  - `BQ_HW_REQUIREMENT_API_DERIVE_001` / Erdos / `019ef217-f145-79e3-b785-dab9135ed6c7`：已返回，明确正式 INT 接口应按“硬件访问机制”保留，而不是按 API 数量保留。
- 修正后的判断依据：
  - BQ76952 当前硬件需要 I2C1 通信、ALERT/EXTI、WAKE、CHIP_SHUT、ONLINE、6S VC 映射、低边电流采样等能力。
  - BQ76952 访问模型天然包含 direct command、subcommand、Data Memory、transfer buffer checksum、可选 I2C CRC；不能因为旧项目 BQ76930 只有裸寄存器读写而省掉这些访问机制。
  - BQ 均衡逻辑由软件手写，但不能写在 INT 通信层；INT 只保留可让上层实现均衡的底层读写能力。
- 当前建议保留为 BQ 通信 INT 正式硬件接口的类别：
  - 通信格式控制：`SetCrcEnabled()`、`IsCrcEnabled()`。
  - direct command 原语：`ReadDirect()`、`WriteDirect()`。
  - subcommand 原语：`SendSubcommand()`、`ReadSubcommand()`。
  - Data Memory 原语：`ReadDataMemory()`、`WriteDataMemory()`。
  - 配置写入所需芯片模式控制：`EnterConfigUpdate()`、`ExitConfigUpdate()`。
- 应上移/不放 INT 的原因：
  - `ReadBatteryStatus()`、`ReadAlarmStatus()`、`ReadFetStatus()`、`ReadCellVoltage()` 上移 COM，不是为了减少数量，而是它们把寄存器读包装成业务可理解语义，尤其 `ReadCellVoltage(cell_index)` 绑定 cell index/VC 映射，后续均衡会依赖。
  - `ClearAlarmStatus()` 上移 COM/APP，因为“何时清除告警”是业务时机。
  - 自动均衡、SOC、保护阈值选择、FET 业务开关、自动清告警、写入 6S draft mask 都不进 INT。
- 板级 GPIO/中断能力：
  - `BMS_WAKE`、`BMS_CHIP_SHUT`、`BMS_CHIP_ONLINE`、`BMS_INT/ALERT` 必须预留在 INT 层，但建议另放 `GPIO/board` INT pre 或单独 BQ board-control pre；当前通信 pre 不混入这些接口，原因是 CubeMX GPIO 宏名和有效电平尚未冻结。
- 旧项目可参考项：INT `.c` 薄、BSP 承载详细寄存器/位域/注释、上层用 BSP 常量加 INT 原语组合业务；不可参考项：接口数量、BQ76930 寄存器事实、旧 MCU 外设和旧硬件连线。
- 当前写权限锁：无代码写锁；本轮只读纠偏，无代码改动。
- 禁止下一步：禁止再把“API 数量少”当主因；禁止为模仿旧项目而破坏 BQ76952 官方访问模型；禁止在未确认 GPIO 宏名和电平前把 wake/shut/online 混入通信 pre。
- 本轮唯一下一步：若用户认可该推导，主线程应重写 `docs/pre/int_bq76952_comm_pre.md` 的论证段，使其明确“接口来自硬件访问模型和分层边界”，而不是“压到旧项目数量”。

## 1. 用户最新目标

根据 `README.md` 启动新 BMS 项目开发。用户最新要求为：必须多 agent 协作，主线程不能包办审查；每次执行前必须读取 `rules/state`；当前任务是只读审查两个 INT 源文件 `Int_SC8815.c` 和 `Int_BQ76952.c` 的私有函数数量、必要性、风格、教学可读性和行数取舍，不改代码。

用户已明确指出：

- 最基础的子任务应有写代码权力。
- 不能让主线程过早承载过多上下文。
- 必须设计主线程发生上下文压缩后仍不跑偏的机制。
- 必须遵守已落地的主线程替代机制和写权限锁机制；正式主线程替换以 Codex 上下文提示、压缩恢复提示、handoff summary 或用户明确要求为硬触发。
- 每次签发任务卡、改文件、跑检查、合并代码或继续下一步前，必须先读取 `docs/rules/parallel_work_mechanism.md`、`docs/state/main_thread_checkpoint.md`、`docs/rules/hardware_rules.md` 和当前模块 `pre` 文档。
- 当前为 review-only 审查轮，不进入 COM/APP，不签发代码写锁；两个 INT 源文件分别由不同只读 agent 审查，避免单个 agent 同时负责两个芯片。

## 2. 当前阶段

- 当前模块：`Int_SC8815` 与 `Int_BQ76952` 源文件只读审查。
- 当前门禁：Review Gate（只读，多 agent）。
- 主线程世代：`G3`。
- 替代/修正原因：用户纠正主线程替代机制应以 Codex 上下文提示为准；已修正 `docs/rules/parallel_work_mechanism.md`，后续不再仅凭主线程主观判断自动换代。
- 当前状态：`G3` 已重新读取 `docs/rules/parallel_work_mechanism.md`、`docs/rules/hardware_rules.md`、本文件和 `docs/pre/int_sc8815_comm_pre.md`。按用户最新要求，本轮将由多 agent 只读审查 `Int_SC8815.c`、`Int_BQ76952.c` 和整体风格/教学可读性；主线程只汇总，不直接包办审查结论。

## 3. 已批准事实

- 项目根目录：`C:\Users\lst\Desktop\bms`。
- 新项目资料包括根目录网表、原理图 PDF、`official_chip_docs_files` 和旧项目源码。
- 软件分层方向为 `Core/HAL -> INT -> COM -> APP`。
- 当前新项目尚无新的 `Int/`、`Com/`、`App/` 代码目录。
- 旧工程可以参考分层、命名和注释风格，但不能继承旧 BQ76930 寄存器、STM32F1 外设假设或旧硬件连接。
- `docs/rules/hardware_rules.md` 是当前硬件硬约束入口。
- `docs/rules/parallel_work_mechanism.md` 是当前并行协作硬约束入口。
- 主线程可以换代；正式换代以 Codex 上下文过长/压缩/接续提示为准，或用户明确要求为硬触发；新主线程接班前必须通过 Replacement Gate。
- 写权限锁必须记录到状态文件；未释放锁不得重复签发。
- 文件归属矩阵已规定 `docs/rules/*`、`docs/state/*` 默认只由主线程写；代码 worker 默认不得修改。
- 写锁有租约和状态流转；worker 返回后先进入 `returned`，经主线程检查后才能 `released/merged`。
- 主线程一次只处理一个 returned 锁，同一文件未审查完不得再次签发。
- agent 失败、超时、429、关闭或输出不完整时，写锁不得自动释放，必须由主线程显式标记。
- BQ76952 位于 BMS 板，当前硬件规则要求 BQ INT 层按 BQ76952 官方 TRM/Software Guide 重新设计，不得复用旧 BQ76930 寄存器表。
- SC8815 只允许用于 6S 三元锂充电方向，禁止 OTG、反向输出、反向供电。
- SC8815 固定 7-bit I2C 地址 `0x74`，但硬件 IIC3 接反，后续通信必须用 PA6/PA7 GPIO 软件 IIC。

## 4. 已拒绝假设

- 拒绝让主线程成为唯一代码编写者。
- 拒绝让子 agent 无任务卡写代码。
- 拒绝把旧项目代码当作新硬件事实来源。
- 拒绝在没有 `pre` 文档和 API 审核的情况下直接写模块代码。
- 拒绝上下文压缩后不重新读取状态文件就继续执行。
- 拒绝在替代交接未完成时跨模块推进。
- 拒绝在未释放旧写锁时给其他 worker 分配同一文件或同一函数。
- 拒绝把失败或迟到 agent 输出自动合并。

## 5. 当前活动任务卡

只读 review 任务卡，均无写权限：

| 任务卡编号 | 角色 | 范围 | 状态 | 写权限 |
| --- | --- | --- | --- | --- |
| REVIEW_SC8815_C_001 | Review/Safety agent | `Int/Int_SC8815.c` 相对 `docs/pre/int_sc8815_comm_pre.md`、`docs/rules/hardware_rules.md` 审查 | planned | 无 |
| REVIEW_BQ76952_C_001 | Review/Safety agent | `Int/Int_BQ76952.c` 相对 `docs/pre/int_bq76952_comm_pre.md`、`docs/rules/hardware_rules.md` 审查 | planned | 无 |
| REVIEW_INT_STYLE_001 | 风格/教学可读性 agent | 两个 INT 源文件的行数、私有函数数量、命名、教学可读性、精简取舍 | planned | 无 |
| BQ76952_CRC_API_FIX_001 | Code worker agent | `Int/Int_BQ76952.h/.c` 的 CRC API 签名和 `SendSubcommand()` 未使用变量 | revoked | `Int/Int_BQ76952.h`, `Int/Int_BQ76952.c` |
| BQ76952_CRC_API_FIX_002 | Code worker agent | `Int/Int_BQ76952.h/.c` 的 CRC API 签名和 `SendSubcommand()` 未使用变量 | released | `Int/Int_BQ76952.h`, `Int/Int_BQ76952.c` |
| REVIEW_BQ76952_FIX_002 | Review/Safety agent | 只读复核 `BQ76952_CRC_API_FIX_002` 的授权范围、pre 一致性和 blocking 是否消除 | revoked | 无 |
| REVIEW_BQ76952_FIX_003 | Review/Safety agent | 只读复核 `BQ76952_CRC_API_FIX_002` 的授权范围、pre 一致性和 blocking 是否消除 | returned/released | 无 |
| REVIEW_BQ76952_REVIEW_DOC_001 | Review/Safety agent | 只读复核 `docs/review/int_bq76952_comm_review.md` 是否与 BQ pre/code 一致 | returned/released | 无 |
| SC8815_READABILITY_001 | Code worker agent | `Int/Int_SC8815.c` 中 `WriteReg()` 和 `ReadAdcCurrentMa()` 局部可读性优化 | released | `Int/Int_SC8815.c` |
| REVIEW_SC8815_READABILITY_001 | Review/Safety agent | 只读复核 `SC8815_READABILITY_001` 的授权范围、guard 安全和禁用项 | returned/released | 无 |

实际派发状态：

- `REVIEW_SC8815_C_001` 已由只读 agent `019eeed7-ea04-7841-a72a-bbf368e2e8e2`（Euclid）返回，状态 `returned`，无写权限；结论为未发现 blocking，私有函数数量偏多但分层基本合理，主要可读性压力在 `GuardWrite()` 过长和 `ReadAdcCurrentMa()` 重复。
- `REVIEW_BQ76952_C_001` 已由只读 agent `019eeed7-ea7b-78a0-b333-42e1aa738ffe`（Carson）返回，状态 `returned`，无写权限；结论为存在 blocking：CRC 两个 public API 与 `docs/pre/int_bq76952_comm_pre.md` 签名不一致，且 `SendSubcommand()` 有未使用局部变量 `ret`。
- `REVIEW_INT_STYLE_001` 已由只读 agent `019eeeda-405c-74e0-9829-ea1c5a26d69a`（Avicenna）返回并关闭，状态 `returned/released`，无写权限；结论为 SC8815 20 个 static 中多数是软件 IIC、guard 和换算所需，BQ76952 6 个 static 不多，真正需要处理的是 BQ blocking 与 SC8815 局部可读性。
- `BQ76952_CRC_API_FIX_001` 已派发给代码 worker `019eeee0-06e5-7fd2-960c-db282c2f41b7`（Wegener），但因 429 失败，未返回可合并输出；状态显式标记为 `revoked`，迟到输出只能作为参考材料，不能直接合并。
- `BQ76952_CRC_API_FIX_002` 已由代码 worker `019eeee1-4579-71a2-af60-3f16b5d1f137`（Poincare）返回并关闭，状态 `released`；主线程初查和只读复核均显示 CRC API 已对齐 pre，`SendSubcommand()` 未使用变量已移除。
- `REVIEW_BQ76952_FIX_002` 已派发给只读 agent `019eeee4-43fd-75c3-a60d-84235e2b3f4b`（Arendt），但因 429 失败，未返回可用输出；状态显式标记为 `revoked`，不得作为复核依据。
- `REVIEW_BQ76952_FIX_003` 已由只读 agent `019eeee5-5b77-72c3-89a3-c64051bdfd83`（Cicero）返回并关闭，状态 `returned/released`；无写权限，结论为未发现 blocking，窄修复已解决 CRC API 与未使用变量问题。
- `REVIEW_BQ76952_REVIEW_DOC_001` 已由只读 agent `019eeeea-fcbf-7493-82df-c8fb1844d5f8`（Jason）返回并关闭，状态 `returned/released`；无写权限，结论为 review 文档和 BQ 代码在 CRC API、`SendSubcommand()`、禁用项记录上无 blocking。其指出 pre 验收标准中“所有 public API 返回状态”泛化表述与 CRC 特例有文字张力，主线程已修正文档。
- `SC8815_READABILITY_001` 已由代码 worker `019eeeef-86d7-7251-8e3e-fcebe5600e45`（Lorentz）返回并关闭，状态 `released`；写权限只覆盖 `Int/Int_SC8815.c` 中 `Int_SC8815_WriteReg()` 和 `Int_SC8815_ReadAdcCurrentMa()`。主线程初查和只读复核均未发现越权或 guard 放宽。
- `REVIEW_SC8815_READABILITY_001` 已由只读 agent `019eeef2-4046-73c3-b604-c28cf94803d5`（Chandrasekhar）返回并关闭，状态 `returned/released`；无写权限，结论为未发现 blocking。

## 6. 当前写权限锁

无 worker 持有写权限。本轮只读 review agent 不持有写锁。

当前写锁登记：

| 锁编号 | 持有者 | 模块 | 允许写入 | 状态 | 释放/撤销说明 |
| --- | --- | --- | --- | --- | --- |
| SC8815_COMM_001 | 主线程 G2 | Int_SC8815 | `Int/Int_SC8815.h`, `Int/Int_SC8815.c`, `docs/review/int_sc8815_comm_review.md`, `docs/state/main_thread_checkpoint.md` | released | 已生成代码和 review，静态检查完成。 |
| SC8815_READABILITY_001 | `019eeeef-86d7-7251-8e3e-fcebe5600e45` Lorentz | Int_SC8815 | `Int/Int_SC8815.c` | released | 已通过主线程初查和只读复核；同一文件可重新分配新任务卡。 |
| BQ76952_CRC_API_FIX_001 | `019eeee0-06e5-7fd2-960c-db282c2f41b7` Wegener | Int_BQ76952 | `Int/Int_BQ76952.h`, `Int/Int_BQ76952.c` | revoked | 429 失败，无可合并输出；迟到输出不得直接合并。 |
| BQ76952_CRC_API_FIX_002 | `019eeee1-4579-71a2-af60-3f16b5d1f137` Poincare | Int_BQ76952 | `Int/Int_BQ76952.h`, `Int/Int_BQ76952.c` | released | 已返回并通过只读复核；CRC API 与 pre 对齐，`SendSubcommand()` 未使用变量已移除。 |

`G3` 接班时确认：没有活动写锁；未释放锁不得重复签发；后续如继续同一文件，必须先按本文件登记新的任务卡和写权限锁。

最近一次失败 agent：

- `019eeeb2-9bd9-7d72-8769-b47a0626f480` 因 429 失败，未获得写权限；其输出不得作为事实或合并依据。

## 6.1 G3 接班确认与机制修正

- 新主线程世代：`G3`。
- 接班时间：2026-06-22。
- 接班原因：用户曾明确允许接班；随后用户纠正机制，要求后续正式替换以 Codex 上下文提示为准。
- 当前机制修正：没有 Codex 上下文过长/压缩/接续提示时，不因主线程主观判断自动换代；只刷新交接包和状态文件。
- Replacement Gate 读取文件：`README.md`、`docs/rules/parallel_work_mechanism.md`、`docs/rules/hardware_rules.md`、`docs/state/main_thread_checkpoint.md`、`docs/pre/int_sc8815_comm_pre.md`、`docs/review/int_sc8815_comm_review.md`。
- 工作区状态：存在未提交/未跟踪改动，视为当前工作区事实，不回滚；后续只在明确任务卡和写锁范围内修改。
- 活动任务卡：无。
- 活动写锁：无。
- 未合并 worker 输出：无。
- 失败/身份不明 agent 输出处理：仅可作为参考材料，不能作为事实或合并依据。
- 接班后唯一动作：若用户要求继续代码，先重新读取协作文档与当前模块 `pre`，然后只复核或修正 `Int_SC8815.c/.h` 一个范围；不得顺手进入 COM/APP。

## 6.2 G7 SC8815 充电电流目标更新

- 主线程世代：`G7`。
- 更新原因：当前对话出现 handoff summary/上下文接续提示，并且用户明确新增硬件确认事实。
- 本轮执行前已重新读取：`README.md`、`docs/rules/parallel_work_mechanism.md`、`docs/state/main_thread_checkpoint.md`、`docs/rules/hardware_rules.md`、`docs/pre/int_sc8815_comm_pre.md`。
- 用户最新确认：电池侧充电电流 `5A` 可以，已和硬件确认；为了保险，软件初期运行先按 `3A`。
- 直接硬件/寄存器事实：当前 SC8815 采样为 `R5=10mΩ`、`R14=10mΩ`，`IBUS_RATIO=3`、`IBAT_RATIO=6`；寄存器量程约 `IBUS=3A`、`IBAT=6A`。
- 合并判断：`IBAT=5A` 可作为电池侧软件硬上限，初期默认/运行目标先取 `3000mA`；`IBUS` 仍保持 `3000mA` 上限，不能因为电池侧 5A 目标而改成输入侧 5A。
- 已更新文件：`docs/rules/hardware_rules.md`、`docs/pre/int_sc8815_comm_pre.md`、`docs/state/main_thread_checkpoint.md`。
- 当前只读 agent：`SC8815_CURRENT_TARGET_REVIEW_001` / Franklin / `019ef38b-9dd9-7fa2-87e1-929a6caec3e5`，状态 `in_progress`，无写权限；输出只用于复核，不自动成为事实。
- 当前写权限锁：无代码写锁；本轮只更新主线程归属文档，不改 `Int/Int_SC8815_BSP.h` 或 `Int/Int_SC8815.c/.h`。
- 唯一下一步：等待只读复核返回；若无 blocking，再开窄代码 worker 任务卡更新 SC8815 BSP 宏和 guard。
- 禁止下一步：禁止把输入侧 `IBUS` 提到 `5A`；禁止在未登记代码任务卡和写锁前修改 `Int/Int_SC8815_BSP.h`、`Int/Int_SC8815.c` 或 `Int/Int_SC8815.h`。

## 7. 未解决问题

- 尚未创建 `docs/rules/coding_rules.md`。
- 新项目代码目录结构暂定新建根目录 `Int/Int_BQ76952_BSP.h`，已在 `pre` 中说明。
- 旧项目 `Int_BQ769_BSP.h` 只能作为风格参考，不能复用旧寄存器地址。
- 本机未发现 `gcc`、`clang`、`cl`，所以 `Int_BQ76952_BSP.h` 尚未做编译器语法检查。
- `BQ76952_CELL_MASK_6S_HW_DRAFT` 必须由用户/硬件资料复核后才能用于实际配置。
- 当前没有新 CubeMX `.ioc`，`Int_BQ76952.c` 首版默认使用 `hi2c1`，后续如句柄名不同需要适配。
- `Int_BQ76952.c/.h` 未做编译器语法检查，原因同上。
- SC8815 `FB/ADIN` 在本项目上的具体外部连接仍需后续结合原理图复核。
- R17/R18 实物是否已改为 `100kΩ/5.1kΩ`，需要 bring-up 前确认。
- `Int_SC8815_BSP.h` 已做文本级静态检查和手册页复核；仍未做 C 编译器语法检查，因为本机未发现 `gcc`、`clang`、`cl`。
- 当前没有新 CubeMX `.ioc`，`Int_SC8815.c` 首版必须用可配置 GPIO 宏适配 PA6/PA7/PB0/PB1，后续按实际 CubeMX 宏名修正。
- `Int_SC8815.c/.h` 未做编译器语法检查，因为本机未发现 `gcc`、`clang`、`cl`。

## 7.1 G8 App_BQ76952 首版任务登记

- 主线程世代：`G8`。
- 更新日期：2026-06-24。
- 触发原因：用户明确要求“根据旧项目，说说新项目 BQ 需要做什么，然后把 APP 层写出来”，并强调旧项目风格、教学目的、INT 不要继续变厚。
- 本轮执行前已重新读取：`README.md`、`docs/rules/parallel_work_mechanism.md`、`docs/state/main_thread_checkpoint.md`、`docs/rules/hardware_rules.md`、`docs/pre/int_bq76952_comm_pre.md`、`docs/logic/hardware_interface_reservation.md`。
- 当前模块：`App_BQ76952`。
- 当前门禁：Pre Gate -> Implementation Gate。
- 本轮只推进一个模块：`App_BQ76952`；不改 INT，不改 SC，不创建 COM。
- 已创建 pre：`docs/pre/app_bq76952_pre.md`。
- 合并判断：
  - 旧项目可借鉴 BQ APP 的初始化、采样、SOC、均衡、FET/告警策略的流程组织。
  - 新项目首版 APP 不直接调用 `Int_BQ76952_*`，因为 COM 尚未落地；首版只消费未来 COM 提供的测量快照并做本地策略缓存。
  - `UpdateBalance()` 只计算逻辑 6S mask，不写 BQ76952 host balancing mask，不使用 `BQ76952_CELL_MASK_6S_HW_DRAFT`。
  - `SetChargeEnable()` / `SetDischargeEnable()` 只记录请求，不直接操作 FET。
- 只读 agent：
  - `APP_BQ_OLD_FLOW_001` / Leibniz / `019ef820-fe64-77a2-9d52-03c9d221203a`：已返回，只读，无写权限；结论为旧项目流程可借鉴，寄存器/阈值/GPIO 禁止照搬。
  - `APP_BQ_BOUNDARY_001` / Kuhn / `019ef821-4802-7a23-a0c9-427038d5fc7c`：已返回，只读，无写权限；结论为无 COM 时 APP 不应直接调用 INT 正式业务路径。
  - `APP_BQ_STYLE_001` / Mencius / `019ef821-8f81-71d1-8537-c495656c1bab`：已返回，只读，无写权限；结论为首版应少量 public API、一个静态状态结构体、步骤式中文注释、不引入复杂抽象。
- 代码 worker 任务卡：
  - 任务卡编号：`APP_BQ76952_DRAFT_001`
  - 模块：`App_BQ76952`
  - 当前门禁：Implementation Gate
  - 输入文件：`docs/pre/app_bq76952_pre.md`、`docs/rules/hardware_rules.md`、旧项目 `App_BatMan.c/.h`
  - 允许写入：`App/App_BQ76952.h`、`App/App_BQ76952.c`
  - 禁止修改：`Int/*`、`Com/*`、`docs/rules/*`、`docs/state/*`、旧项目目录
  - 验收标准：不包含 `Int_BQ76952.h`；不调用 `Int_BQ76952_*`；不包含 FreeRTOS、printf、动态内存；public API 与 pre 一致。
- 当前写权限锁：
  - `APP_BQ76952_DRAFT_001` / worker 待派发 / `App_BQ76952` / 文件级锁 / 允许写入 `App/App_BQ76952.h`, `App/App_BQ76952.c` / 状态 `requested` / 释放条件：worker 返回后经主线程越权检查和只读复核。
- 禁止下一步：禁止并行给其他 worker 分配 `App/App_BQ76952.h/.c`；禁止顺手修改 INT、SC、COM；禁止将 APP 直接改成调用 INT 的业务路径。

## 8. 下一步唯一动作

当前唯一动作：按用户最新要求，只读分析外部参考库 `skriachko/bq76952` 的 INT/底层读写封装方法，判断其优点、完整方法列表，以及本项目 `Int_BQ76952` 应借鉴/保留/不上收的内容；本轮不改代码。

## 8.1 G9 BQ76952 外部读写封装参考分析登记

- 主线程世代：`G9`。
- 更新日期：2026-06-24。
- 触发原因：用户要求参考 `https://github.com/skriachko/bq76952`，分析其读写封装优点、全部方法，以及本项目需要什么。
- 本轮执行前已重新读取：`README.md`、`docs/rules/parallel_work_mechanism.md`、`docs/state/main_thread_checkpoint.md`、`docs/rules/hardware_rules.md`、`docs/pre/int_bq76952_comm_pre.md`。
- 当前模块：`Int_BQ76952`。
- 当前门禁：API/Implementation Boundary Review，只读，不改代码。
- 只读任务卡：
  - `BQ_EXT_REF_METHODS_001`：枚举 `skriachko/bq76952` 暴露的封装方法、底层读写结构和依赖边界；无写权限。
  - `BQ_EXT_REF_PROJECT_FIT_001`：对照本项目硬件规则和当前 BQ INT pre，判断哪些做法值得借鉴、哪些不适合进入 INT；无写权限。
- 当前写权限锁：无代码写锁；本轮不写 `Int/*.c/.h`，不改 BSP。
- agent 返回：
  - `BQ_EXT_REF_METHODS_001` / Fermat / `019ef831-aec6-7b90-8266-5555321ac466`：已返回，只读，无写权限；结论为外部库 public API 包含生命周期、读电压/电流/温度/状态、FET、保护阈值、OTP/FullAccess、Data Memory 配置写封装等大包方法；底层读写为 `directCommand`、`subCommand`、`subCommandResponseInt`、`readDataMemory`、`writeDataMemory`。
  - `BQ_EXT_REF_PROJECT_FIT_001` / Bernoulli / `019ef832-097e-7c13-b9a5-12593721b1e2`：已返回，只读，无写权限；结论为可借鉴 direct/subcommand/Data Memory 的访问机制分层，但不应把状态解释、保护阈值、FET、OTP、FullAccess、硬编码初始化搬入本项目 INT。
- 本轮合并判断：
  - 外部库优点：把 BQ 访问拆成 direct command、subcommand、Data Memory 三类；Data Memory 写入流程直观；BSP/常量与读写原语分层思路值得参考。
  - 外部库不适合直接借鉴：C++ 单例、Arduino Wire 风格 I2C 包装、头文件暴露 HAL、无 CRC/PEC、读 transfer buffer 缺少 checksum/length 校验、无超时 busy wait、初始化中硬编码 `VCELL_MODE=0xAAAF`、保护/FET/OTP 写入。
  - 本项目需要：INT 只保留 BQ 访问机制原语；语义化状态、电芯电压映射、告警处理、FET 策略、保护阈值、OTP/FullAccess 和均衡策略留给 COM/APP 或专门 bring-up/制造流程。
- 禁止下一步：禁止直接把外部库方法照搬进本项目；禁止越过 `pre` 新增 public API；禁止把状态解释、保护策略、均衡策略、FET 业务控制下沉到 INT。

下一步按顺序执行：

1. 若继续改 SC8815，必须另开新的窄任务卡和写锁。
2. 若进入 BQ 后续配置或其他 INT 模块，必须先完成对应 pre/review 状态更新。
3. 若进入 COM/APP，必须由用户明确要求并先完成 INT 冻结门禁。

禁止下一步：

- 禁止改 `Int/Int_SC8815_BSP.h`。
- 禁止改 `Int/Int_BQ76952_BSP.h`。
- 禁止使用 HAL I2C、FreeRTOS、printf、动态内存。
- 禁止提供 OTG/反向输出 API。
- 禁止在未完成新门禁和写锁登记前进入 COM/APP。
- 禁止本轮直接改 `Int/Int_SC8815.c` 或 `Int/Int_BQ76952.c`；若审查后需要精简，必须另开代码 worker 任务卡和写锁。

## 9. 压缩恢复 Rebase Gate

主线程恢复后必须先执行：

1. 读取 `README.md`。
2. 读取 `docs/rules/parallel_work_mechanism.md`。
3. 读取 `docs/rules/hardware_rules.md`。
4. 读取本文件。
5. 检查用户最新消息是否覆盖本文件。
6. 明确当前只做一个动作。
7. 更新本文件后再继续。
