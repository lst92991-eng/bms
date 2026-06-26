# Int_BQ76952.c/.h 通信层预设计

记录日期：2026-06-22

更新日期：2026-06-23

本版按最新边界收敛：业务逻辑在 APP，组合逻辑在 COM，INT 只保留“能操作 BQ76952 硬件访问机制”的接口。BQ 的均衡策略后续由上层手写，INT 不保存均衡决策、不遍历电芯做策略判断、不封装保护阈值业务。

## 1. 本轮目标

生成 BQ76952 INT 通信层首版：

- `Int/Int_BQ76952.h`
- `Int/Int_BQ76952.c`

本轮只实现底层通信访问机制，不实现 SOC、均衡策略、保护策略、任务调度、显示、CAN，也不把 battery/alarm/FET/cell voltage 这类语义化读取作为正式 INT public API。

## 2. 输入文件

- `Int/Int_BQ76952_BSP.h`
- `docs/review/int_bq76952_bsp_review.md`
- `docs/agent_reports/bq76952_bsp_register_facts_2026-06-22.md`
- `docs/rules/hardware_rules.md`
- `official_chip_docs_files/TI_BQ76952_Technical_Reference_Manual_sluuby2b.pdf`
- `official_chip_docs_files/TI_BQ769x2_Software_Development_Guide_sluaa11.pdf`

## 3. 边界

正式允许实现：

- I2C direct command read/write。
- command-only subcommand。
- subcommand read。
- Data Memory read/write。
- I2C CRC8 可选校验。
- transfer buffer checksum 校验。
- 进入/退出 CONFIG_UPDATE。

bring-up/debug 可选实现，但不进入正式最小 public API：

- 读取 device number。

这些接口只用于上电验证、手册流程核对或工厂调试。若需要保留到代码中，必须用明确的 bring-up/debug 条件编译隔离，例如 `INT_BQ76952_ENABLE_BRINGUP_API`，并且默认关闭。

上移到 COM/APP：

- 读取 battery status。
- 读取 alarm status。
- 清除 alarm status。
- 读取 FET status。
- 读取单体电压。

上移原因：这些接口已经把 BQ 寄存器/子命令包装成业务可理解语义。它们本身不一定有策略错误，但会诱导 APP 绕过 COM 的组合边界，导致 INT 越来越像业务服务层。COM/APP 可基于 `ReadDirect`、`ReadSubcommand`、`ReadDataMemory` 和 BSP 常量组合出这些语义。

禁止实现：

- SOC。
- 自动均衡策略。
- 保护阈值策略。
- FET 业务开关策略。
- FreeRTOS task/mutex。
- printf 调试输出。
- GPIO wake/shutdown，因当前没有新 CubeMX 宏名。

## 4. HAL 依赖约定

由于当前仓库还没有新 CubeMX 工程和 `.ioc`，首版采用最小 HAL 约定：

- `.c` 包含 `i2c.h`，默认使用 `extern I2C_HandleTypeDef hi2c1`。
- 如果后续 CubeMX 句柄名不同，可在 `.c` 顶部通过 `INT_BQ76952_I2C_HANDLE` 宏重定向。
- `.h` 不包含 HAL 头，避免上层业务被 HAL 类型污染。

## 5. Public API

### 5.1 正式最小 public API

```c
void Int_BQ76952_SetCrcEnabled(bool enabled);
bool Int_BQ76952_IsCrcEnabled(void);
Int_BQ76952_StatusTypeDef Int_BQ76952_ReadDirect(uint8_t command, uint8_t *data, uint8_t len);
Int_BQ76952_StatusTypeDef Int_BQ76952_WriteDirect(uint8_t command, const uint8_t *data, uint8_t len);
Int_BQ76952_StatusTypeDef Int_BQ76952_SendSubcommand(uint16_t subcommand);
Int_BQ76952_StatusTypeDef Int_BQ76952_ReadSubcommand(uint16_t subcommand, uint8_t *data, uint8_t len);
Int_BQ76952_StatusTypeDef Int_BQ76952_ReadDataMemory(uint16_t address, uint8_t *data, uint8_t len);
Int_BQ76952_StatusTypeDef Int_BQ76952_WriteDataMemory(uint16_t address, const uint8_t *data, uint8_t len);
Int_BQ76952_StatusTypeDef Int_BQ76952_EnterConfigUpdate(void);
Int_BQ76952_StatusTypeDef Int_BQ76952_ExitConfigUpdate(void);
```

说明：

- 这 10 个接口覆盖 BQ76952 的三类访问机制：direct command、subcommand、Data Memory；CRC 开关是通信格式能力，CONFIG_UPDATE 是 Data Memory 配置写入所需的芯片硬件模式控制，都不是业务策略。
- INT 的职责是把 I2C/CRC/checksum/transfer buffer 这些芯片访问细节稳定封装起来，让 COM/APP 不直接处理协议细节。
- 不再把“状态含义”封装为正式 public API。状态解释、均衡判断、故障处理、告警清除时机均属于上层逻辑。

### 5.2 bring-up/debug 可选 API

默认不启用，仅在 `INT_BQ76952_ENABLE_BRINGUP_API` 打开时允许出现：

```c
Int_BQ76952_StatusTypeDef Int_BQ76952_ReadDeviceNumber(uint16_t *device_number);
```

保留理由：

- `ReadDeviceNumber` 用于确认 I2C、地址、CRC 配置和芯片身份，适合 bring-up。

限制：

- 默认关闭，正式应用层不得依赖这些接口。
- 不允许在 INT 内自动执行配置流程或写保护阈值。
- 不允许把 device number 读取扩展成设备识别状态机。

### 5.3 不再保留为 INT public API

```c
Int_BQ76952_StatusTypeDef Int_BQ76952_ReadBatteryStatus(uint16_t *status);
Int_BQ76952_StatusTypeDef Int_BQ76952_ReadAlarmStatus(uint16_t *status);
Int_BQ76952_StatusTypeDef Int_BQ76952_ClearAlarmStatus(uint16_t mask);
Int_BQ76952_StatusTypeDef Int_BQ76952_ReadFetStatus(uint8_t *status);
Int_BQ76952_StatusTypeDef Int_BQ76952_ReadCellVoltage(uint8_t cell_index, int16_t *cell_mv);
```

删除/上移理由：

- `ReadBatteryStatus`、`ReadAlarmStatus`、`ReadFetStatus` 是语义化状态读取，应由 COM 组合 direct/subcommand 结果并交给 APP 决策。
- `ClearAlarmStatus` 涉及“何时清除告警”的业务时机，不能放在 INT。
- `ReadCellVoltage` 虽然是硬件读数，但它把 cell index、VC 映射和电芯语义绑定到 public API；上层均衡逻辑会频繁使用，放在 COM 更清晰。
- BQ 均衡策略需要读取单体电压、温度、告警、FET 状态后综合判断，这些组合不属于 INT。

## 6. 错误码

建议错误码：

- `INT_BQ76952_OK`
- `INT_BQ76952_ERROR`
- `INT_BQ76952_ERROR_PARAM`
- `INT_BQ76952_ERROR_HAL`
- `INT_BQ76952_ERROR_CRC`
- `INT_BQ76952_ERROR_CHECKSUM`
- `INT_BQ76952_ERROR_TIMEOUT`
- `INT_BQ76952_ERROR_LENGTH`

## 7. 关键实现规则

- I2C CRC 关闭时，direct read/write 按普通 HAL memory read/write。
- I2C CRC 开启时：
  - write 第一个数据字节 CRC 覆盖 `write_addr + command + data0`。
  - write 后续数据字节 CRC 只覆盖该数据字节。
  - read 第一个数据字节 CRC 覆盖 `write_addr + command + read_addr + data0`。
  - read 后续数据字节 CRC 只覆盖该数据字节。
- Data Memory 写入必须先写 `0x3E/0x3F + data`，再写 `0x60/0x61`，其中 length 为 data length + 4。
- subcommand/data memory read 读取时必须验证 transfer buffer checksum。
- 只有需要读回数据的 subcommand/data memory read 采用轮询 `0x3E/0x3F` echo；command-only subcommand 只写 `0x3E/0x3F`，不强制等待 echo。
- 读取 transfer buffer 时先读 `0x61` length，再读 `0x40-0x5F` buffer，最后读 `0x60` checksum；避免在读 buffer 前把 `0x60/0x61` 一起读出。
- 所有正式 public API 检查空指针和长度。
- 不默认启用 CRC，bring-up 初期由上层调用 `SetCrcEnabled` 决定。
- 不允许 INT 根据 battery/alarm/FET/cell voltage 结果做策略判断。
- 不允许 INT 自动清除告警。
- 不允许 INT 写入 `BQ76952_CELL_MASK_6S_HW_DRAFT` 或任何均衡配置策略。

## 8. 任务卡

任务卡编号：`BQ76952_COMM_MIN_API_002`

允许读取：

- 本 `pre` 文档。
- `Int/Int_BQ76952_BSP.h`
- `docs/agent_reports/bq76952_bsp_register_facts_2026-06-22.md`
- `docs/review/int_bq76952_bsp_review.md`

允许写入：

- `Int/Int_BQ76952.h`
- `Int/Int_BQ76952.c`

禁止修改：

- `Int/Int_BQ76952_BSP.h`
- `README.md`
- `docs/rules/*`
- `docs/state/*`
- 旧项目目录
- APP/COM 文件

验收标准：

- `.h` 不包含 HAL 头。
- `.c` 不包含 FreeRTOS，不调用 printf。
- 正式 public API 只保留 10 个最小接口。
- 除 CRC 配置接口 `SetCrcEnabled()`/`IsCrcEnabled()` 外，通信类 public API 返回状态。
- bring-up/debug API 若保留，必须受 `INT_BQ76952_ENABLE_BRINGUP_API` 保护且默认关闭。
- `ReadBatteryStatus`、`ReadAlarmStatus`、`ClearAlarmStatus`、`ReadFetStatus`、`ReadCellVoltage` 不得作为正式 public API 暴露。
- 无业务策略。
- 不写 `BQ76952_CELL_MASK_6S_HW_DRAFT` 到芯片。
