# 主线程状态检查点

更新时间：2026-06-22

本文档是主线程发生上下文压缩、恢复、中断或被用户指出跑偏后的第一读取入口。主线程不能依赖压缩前记忆继续执行，必须先读取本文件和 `docs/rules/parallel_work_mechanism.md`。

## 1. 用户最新目标

根据 `README.md` 启动新 BMS 项目开发。当前用户要求先由主线程按官方手册再次确认 `Int_BQ76952_BSP.h`，确认后直接生成 BQ 芯片源文件和头文件。

用户已明确指出：

- 最基础的子任务应有写代码权力。
- 不能让主线程过早承载过多上下文。
- 必须设计主线程发生上下文压缩后仍不跑偏的机制。
- 当前先做 `BQ76952` 的 BSP 头文件；若用户改为先做 `SC8815`，必须更新本文件。

## 2. 当前阶段

- 当前模块：`Int_BQ76952`。
- 当前门禁：Implementation Gate。
- 当前状态：`Int_BQ76952_BSP.h` 已通过 65 项关键宏二次核对；已生成 `docs/pre/int_bq76952_comm_pre.md`；准备签发 `BQ76952_COMM_001`。

## 3. 已批准事实

- 项目根目录：`C:\Users\lst\Desktop\bms`。
- 新项目资料包括根目录网表、原理图 PDF、`official_chip_docs_files` 和旧项目源码。
- 软件分层方向为 `Core/HAL -> INT -> COM -> APP`。
- 当前新项目尚无新的 `Int/`、`Com/`、`App/` 代码目录。
- 旧工程可以参考分层、命名和注释风格，但不能继承旧 BQ76930 寄存器、STM32F1 外设假设或旧硬件连接。
- `docs/rules/hardware_rules.md` 是当前硬件硬约束入口。
- `docs/rules/parallel_work_mechanism.md` 是当前并行协作硬约束入口。
- BQ76952 位于 BMS 板，当前硬件规则要求 BQ INT 层按 BQ76952 官方 TRM/Software Guide 重新设计，不得复用旧 BQ76930 寄存器表。

## 4. 已拒绝假设

- 拒绝让主线程成为唯一代码编写者。
- 拒绝让子 agent 无任务卡写代码。
- 拒绝把旧项目代码当作新硬件事实来源。
- 拒绝在没有 `pre` 文档和 API 审核的情况下直接写模块代码。
- 拒绝上下文压缩后不重新读取状态文件就继续执行。

## 5. 当前活动任务卡

`BQ76952_COMM_001`

- 模块：`Int_BQ76952`
- 允许写入：`Int/Int_BQ76952.h`、`Int/Int_BQ76952.c`
- 禁止修改：`Int/Int_BQ76952_BSP.h`、`README.md`、`docs/rules/*`、`docs/state/*`、旧项目目录、APP/COM 文件

## 6. 当前写权限锁

`BQ76952_COMM_001` 持有 `Int/Int_BQ76952.h` 和 `Int/Int_BQ76952.c` 写权限。

## 7. 未解决问题

- 尚未创建 `docs/rules/coding_rules.md`。
- 新项目代码目录结构暂定新建根目录 `Int/Int_BQ76952_BSP.h`，已在 `pre` 中说明。
- 旧项目 `Int_BQ769_BSP.h` 只能作为风格参考，不能复用旧寄存器地址。
- 本机未发现 `gcc`、`clang`、`cl`，所以 `Int_BQ76952_BSP.h` 尚未做编译器语法检查。
- `BQ76952_CELL_MASK_6S_HW_DRAFT` 必须由用户/硬件资料复核后才能用于实际配置。
- 当前没有新 CubeMX `.ioc`，`Int_BQ76952.c` 首版默认使用 `hi2c1`，后续如句柄名不同需要适配。

## 8. 下一步唯一动作

等待代码 worker 完成 `BQ76952_COMM_001`。

下一步按顺序执行：

1. 主线程审核 `Int/Int_BQ76952.h/.c` 是否越权。
2. 检查是否写入业务策略或使用 `BQ76952_CELL_MASK_6S_HW_DRAFT` 配置芯片。
3. 写 `docs/review/int_bq76952_comm_review.md`。
4. 更新本检查点并释放写锁。

## 9. 压缩恢复 Rebase Gate

主线程恢复后必须先执行：

1. 读取 `README.md`。
2. 读取 `docs/rules/parallel_work_mechanism.md`。
3. 读取 `docs/rules/hardware_rules.md`。
4. 读取本文件。
5. 检查用户最新消息是否覆盖本文件。
6. 明确当前只做一个动作。
7. 更新本文件后再继续。
