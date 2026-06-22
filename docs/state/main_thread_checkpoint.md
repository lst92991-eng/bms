# 主线程状态检查点

更新时间：2026-06-22（G4，SC8815 局部可读性补丁已复核）

本文档是主线程发生上下文压缩、恢复、中断或被用户指出跑偏后的第一读取入口。主线程不能依赖压缩前记忆继续执行，必须先读取本文件和 `docs/rules/parallel_work_mechanism.md`。

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

## 8. 下一步唯一动作

当前唯一动作：等待用户审核当前 INT 阶段结果，或在重新读取 rules/state/hardware/pre 后进入下一个明确的 INT 模块；不得直接进入 COM/APP。

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
