# Int_BQ76952.c/.h Review

记录日期：2026-06-22

## 1. Review 范围

本次 review 覆盖：

- `Int/Int_BQ76952.h`
- `Int/Int_BQ76952.c`
- `Int/Int_BQ76952_BSP.h`
- `docs/pre/int_bq76952_comm_pre.md`

## 2. Blocking 问题

无当前 blocking 问题。

## 3. Review 中发现并已修正的问题

### 3.1 Command-only subcommand 不应强制等待 echo

初版 `Int_BQ76952_SendSubcommand()` 在写入 `0x3E/0x3F` 后调用 `Int_BQ76952_WaitEcho()`。

已修正：

- `Int_BQ76952_SendSubcommand()` 现在只写 `0x3E/0x3F`。
- 只有 `ReadSubcommand()` 和 `ReadDataMemory()` 这类需要读回数据的路径等待 echo。

原因：

- BQ76952 手册说明 echo 轮询用于读回型 subcommand 完成判断；command-only subcommand 不要求读 transfer buffer。

### 3.2 Transfer buffer 读取顺序错误

初版 `ReadTransfer()` 在读取 buffer 前一起读取 `0x60/0x61`。

已修正：

- 先读 `0x61` length。
- 再读 `0x40` 起始的 buffer。
- 最后读 `0x60` checksum 并校验。

原因：

- BQ76952 TRM 提醒 `0x60/0x61` 不应在读 buffer 前一起读取，否则可能触发 auto-increment 副作用。

## 4. 已检查项

- `.h` 只包含 `stdbool.h` 和 `stdint.h`。
- `.c` 未包含 FreeRTOS。
- 未调用 `printf`。
- 未使用动态内存。
- 未调用 GPIO wake/shutdown。
- 未出现 `App_`、`Com_`、SOC、业务均衡策略。
- 未写入 `BQ76952_CELL_MASK_6S_HW_DRAFT` 到芯片。
- 未发现旧 BQ76930 宏名。
- 默认 public API 只保留 `pre` 冻结的 10 个正式接口。
- `ReadDeviceNumber` 仅在 `INT_BQ76952_ENABLE_BRINGUP_API` 打开时声明和实现，默认不暴露。
- `ReadBatteryStatus`、`ReadAlarmStatus`、`ClearAlarmStatus`、`ReadFetStatus`、`ReadCellVoltage` 不再作为 INT public API 暴露。
- 除 2 个 CRC 配置接口外，其余 8 个正式通信 public API 返回 `Int_BQ76952_StatusTypeDef`。
- 2 个 CRC 配置 public API 与 `pre` 一致：`SetCrcEnabled()` 返回 `void`，`IsCrcEnabled()` 返回 `bool`。

## 5. 后续复核中发现并已修正的问题

### 5.1 CRC 配置 API 与 pre 签名不一致

中间版本中 `Int_BQ76952_SetCrcEnabled()` 和 `Int_BQ76952_IsCrcEnabled()` 的签名曾偏离 `docs/pre/int_bq76952_comm_pre.md`。

已修正：

- `Int_BQ76952_SetCrcEnabled(bool enabled)` 返回 `void`。
- `Int_BQ76952_IsCrcEnabled(void)` 返回 `bool`。

原因：

- CRC 开关只改变驱动内部通信格式假设，不访问芯片，不需要返回通信状态。

### 5.2 `SendSubcommand()` 中存在未使用局部变量

中间版本中 `Int_BQ76952_SendSubcommand()` 曾保留未使用的 `ret`。

已修正：

- 移除未使用变量，函数直接返回 `Int_BQ76952_WriteDirect()` 的结果。

### 5.3 public API 过宽，越过最新 INT 边界

中间版本曾把 `ReadBatteryStatus()`、`ReadAlarmStatus()`、`ClearAlarmStatus()`、`ReadFetStatus()`、`ReadCellVoltage()` 作为正式 INT public API 暴露。

已修正：

- `.h` 默认只暴露 10 个正式通信接口。
- `ReadDeviceNumber()` 加入 `INT_BQ76952_ENABLE_BRINGUP_API` 条件编译，默认关闭。
- `EnterConfigUpdate()` / `ExitConfigUpdate()` 改用私有 `ReadCfgUpdateBit()`，不再依赖 public `ReadBatteryStatus()`。

原因：

- 状态解释、告警清除时机、单体电压到电芯语义的映射应由 COM/APP 基于通信原语组合，INT 只稳定封装 BQ76952 的 direct/subcommand/Data Memory/CRC/CONFIG_UPDATE 访问机制。

## 6. 剩余风险

- 当前仓库没有新 CubeMX `.ioc` 和 HAL 工程，`Int_BQ76952.c` 默认使用 `hi2c1`，若后续句柄名不同，需要通过 `INT_BQ76952_I2C_HANDLE` 或源码适配。
- 本机未发现 `gcc`、`clang`、`cl`，未做编译器语法检查。
- I2C CRC 是否实际启用取决于 BQ76952 `Comm Type`，当前驱动只提供开关，不主动写 `Comm Type`。
- GPIO wake/shutdown 未实现，因为当前没有确认的新 CubeMX 宏名。
- Data Memory 写入不自动进入/退出 CONFIG_UPDATE，调用者必须先调用 `EnterConfigUpdate()` 并确认返回成功。
- 未做真实硬件 I2C、CRC、transfer buffer checksum 时序验证。

## 7. 结论

`Int_BQ76952.c/.h` 可以作为 BQ76952 INT 通信层首版。

下一步建议：

1. 在新 CubeMX 工程生成后接入 `hi2c1`、GPIO wake/alert 宏名。
2. 做上电 bring-up：在 `INT_BQ76952_ENABLE_BRINGUP_API` 打开时读取 `DEVICE_NUMBER`，并用 `ReadDirect()` 读取必要 direct command。
3. 再进入保护配置和 6S `Vcell Mode` 复核，不要直接使用 draft mask 写芯片。
