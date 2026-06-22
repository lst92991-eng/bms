# Int_BQ76952_BSP.h Review

记录日期：2026-06-22

## 1. Review 范围

本次 review 只覆盖：

- `Int/Int_BQ76952_BSP.h`
- `docs/pre/int_bq76952_bsp_pre.md`
- `docs/agent_reports/bq76952_bsp_register_facts_2026-06-22.md`
- `docs/rules/hardware_rules.md`

不覆盖：

- BQ76952 `.c/.h` 通信实现。
- CRC 函数。
- 唤醒/休眠流程。
- 保护阈值策略。
- SOC、均衡业务策略。

## 2. Blocking 问题

无 blocking 问题。

## 3. 已检查项

- 文件路径符合任务卡：`Int/Int_BQ76952_BSP.h`。
- 未修改旧项目目录。
- 未修改 `docs/rules/*` 和 `docs/state/*`。
- 未新增 `.c` 文件。
- 头文件只包含 `<stdint.h>`。
- 未包含 `gpio.h`、`i2c.h`、`FreeRTOS.h`。
- 未发现旧 BQ76930 宏名，例如 `BQ_SYS_STAT`、`BQ_CELLBAL`、`BQ_PROTECT`、`BQ_OV_TRIP`、`BQ_UV_TRIP`。
- Direct Command、Subcommand、Data Memory、bit mask 均来自 `docs/agent_reports/bq76952_bsp_register_facts_2026-06-22.md`。
- `Vcell Mode` 的 6S mask 已标记为 `DRAFT`，并注明禁止当作已确认配置值写芯片。

## 4. 剩余风险

- 本机未发现 `gcc`、`clang`、`cl`，所以未做编译器语法检查。
- `BQ76952_CELL_MASK_6S_HW_DRAFT` 是根据当前硬件规则推导，后续必须结合原理图和 TI 6S 推荐连接方式复核。
- BSP.h 当前只定义常量，不证明通信流程正确；后续 `Int_BQ76952.c/.h` 仍需单独 review。
- 是否启用 I2C CRC 尚未冻结，BSP 只提供 CRC 常量。

## 5. 结论

`Int/Int_BQ76952_BSP.h` 可以作为 BQ76952 INT 层后续通信实现的首版寄存器/子命令常量入口。

## 6. 二次手册复核

复核日期：2026-06-22

用户未手工逐项核对，因此主线程再次按官方手册事实做脚本核对：

- 核对来源：`docs/agent_reports/bq76952_bsp_register_facts_2026-06-22.md`，该报告来自 BQ76952 TRM 和 BQ769x2 Software Guide 的本地 PDF 抽取。
- 核对范围：I2C 地址、CRC 常量、transfer buffer 地址、Direct Command、Subcommand、Data Memory 地址、关键 bit mask。
- 脚本核对关键宏数量：65。
- 结果：0 个地址/bit mask 错误。
- 旧 BQ76930 宏名检查：未发现 `BQ_SYS_STAT`、`BQ_CELLBAL`、`BQ_PROTECT`、`BQ_OV_TRIP`、`BQ_UV_TRIP`、`BQ_VC1_HI`。
- include 检查：除 `<stdint.h>` 外无其他 include。

二次结论：BSP.h 可以进入 `Int_BQ76952.c/.h` 通信层实现。

下一步建议：

1. 用户审核 BSP.h 中的命名、注释详细程度和 6S draft mask 表达方式。
2. 若通过，进入 `Int_BQ76952` 通信 pre：I2C direct read/write、subcommand read/write、Data Memory read/write、CRC8。
