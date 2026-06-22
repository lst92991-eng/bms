# Int_BQ76952_BSP.h 预设计

记录日期：2026-06-22

## 1. 本轮目标

创建新项目 BQ76952 的 BSP 头文件首版：

- 目标文件：`Int/Int_BQ76952_BSP.h`
- 本轮只写头文件，不写 `.c`。
- 本轮只定义寄存器、子命令、Data Memory 地址、bit mask、单位和默认值注释。
- 本轮不实现 I2C 读写、CRC 函数、唤醒函数、保护配置函数。

## 2. 输入文件

- `README.md`
- `docs/rules/hardware_rules.md`
- `docs/rules/parallel_work_mechanism.md`
- `docs/state/main_thread_checkpoint.md`
- `docs/agent_reports/bq76952_bsp_register_facts_2026-06-22.md`
- `official_chip_docs_files/TI_BQ76952_Technical_Reference_Manual_sluuby2b.pdf`
- `official_chip_docs_files/TI_BQ769x2_Software_Development_Guide_sluaa11.pdf`
- 旧项目参考：`旧项目信息/旧项目代码/ups/ups001/MDK-ARM/Int/Int_BQ769_BSP.h`

## 3. Source Gate 结论

- 新项目芯片是 `BQ76952PFBR`，不是旧项目的 BQ76930。
- 旧 `Int_BQ769_BSP.h` 只参考组织方式：分区宏、中文注释、bit mask、枚举。
- 旧 BQ76930 direct register 地址不能复用。
- 本项目当前还没有新 `Int/` 目录，因此本轮允许创建 `Int/` 并只新增 `Int_BQ76952_BSP.h`。

## 4. Hardware Fact Gate 结论

来自 `docs/rules/hardware_rules.md`：

- BQ76952 位于 BMS 板。
- MCU 通过 `PB6/PB7` 的 I2C1 访问 BQ。
- `PB4` 是 BMS INT/ALERT。
- `PB3` 是 BMS_WAKE。
- 本项目电池是 6S 三元锂 21700。
- 电芯接口是 `CN2 1x7P`。
- 低边采样电阻为 `0.5mΩ/6W`。
- BQ VC 映射需要后续按 TI 6S 推荐接法复核，BSP.h 首版不得冻结最终 `Vcell Mode` 运行值。

## 5. Register/Datasheet Gate 结论

官方事实入口：

- `docs/agent_reports/bq76952_bsp_register_facts_2026-06-22.md`

首版 BSP.h 必须包含：

- I2C 地址常量：8-bit write/read 和 7-bit 地址都定义，并说明 STM32 HAL 常用左移地址。
- CRC 常量：多项式 `0x07`、初始值 `0x00`。
- 子命令窗口常量：`0x3E`、`0x3F`、`0x40-0x5F`、`0x60`、`0x61`。
- Direct Commands：状态、cell voltage、stack/pack/LD voltage、CC2 current、alarm、temperature、FET status。
- Command-only Subcommands：reset、config update、FET 控制、sleep、shutdown、通信切换。
- Subcommands with data：device number、FW/HW version、manufacturing status、DASTATUS、CB/FET control。
- Data Memory 地址：通信、保护、alarm、FET、cell balancing 的首版必要项。
- Direct Command bit masks：Safety A/B/C、Alarm、Battery Status、FET Status。
- Data Memory bit masks：Vcell Mode、Enabled Protections A/B/C、FET Options、Balancing Configuration。

## 6. 头文件结构

建议结构：

1. include guard 和 `stdint.h`。
2. 芯片基本参数。
3. I2C/CRC/transfer buffer 常量。
4. Direct Command 地址。
5. Direct Command bit mask。
6. Command-only Subcommand。
7. Data Subcommand。
8. Data Memory 地址。
9. Data Memory bit mask。
10. 6S 辅助 mask。
11. 简单 enum/typedef，仅限无硬件副作用的常量类型。

## 7. 命名规则

- 文件名：`Int_BQ76952_BSP.h`
- 宏前缀：`BQ76952_`
- Direct Command 地址：`BQ76952_CMD_<NAME>`
- 子命令：`BQ76952_SUBCMD_<NAME>`
- Data Memory 地址：`BQ76952_DM_<CLASS>_<NAME>`
- bit mask：`BQ76952_<REGISTER>_<BITNAME>_MASK`
- 6S 辅助 mask：`BQ76952_CELL_MASK_*`

## 8. 注释要求

- 每个寄存器或子命令旁边必须有中文注释。
- 每个 bit mask 必须说明 bit 含义。
- 每个单位相关条目必须说明单位，例如 `mV`、`0.1 K`、`userA`、`Hex`。
- 对容易误用的地址必须写清楚，例如 `0x10/0x11` 是 8-bit I2C 地址，7-bit 地址是 `0x08`。
- 对 `Vcell Mode` 必须注明当前 6S 最终 mask 未冻结，不能擅自作为配置值写入芯片。

## 9. 禁止事项

- 禁止复制旧 BQ76930 地址。
- 禁止在 BSP.h 中写 HAL、FreeRTOS 或 I2C 访问函数。
- 禁止定义 App 层 SOC、均衡策略、故障策略。
- 禁止把 `Vcell Mode` 最终 6S 运行值写死为已确认值。
- 禁止把未抽到官方来源的寄存器写入首版。
- 禁止默认启用或禁用 CRC 策略；BSP 只提供常量。

## 10. 验收标准

- 新增 `Int/Int_BQ76952_BSP.h`。
- 头文件可被 C 编译器包含，不依赖 HAL。
- 只包含 `stdint.h`，不包含 `gpio.h`、`i2c.h`、`FreeRTOS.h`。
- 宏和 bit mask 具备中文注释。
- 文件中不得出现旧项目 `BQ_SYS_STAT`、`BQ_VC1_HI` 这类 BQ76930 命名。
- 文件中必须注明来源是 BQ76952 TRM / BQ769x2 Software Guide。

## 11. 任务卡

任务卡编号：`BQ76952_BSP_HEADER_001`

允许读取：

- 本 `pre` 文档。
- `docs/agent_reports/bq76952_bsp_register_facts_2026-06-22.md`
- `docs/rules/hardware_rules.md`
- 旧 `Int_BQ769_BSP.h`，仅参考组织风格。

允许写入：

- `Int/Int_BQ76952_BSP.h`

禁止修改：

- `README.md`
- `docs/rules/*`
- `docs/state/*`
- 旧项目目录 `旧项目信息/*`
- 任何 `.c` 文件

验收标准：

- 按第 10 节执行。
- 输出必须列出写入文件和是否偏离本 `pre` 文档。
