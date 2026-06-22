# Int_BQ76952.c/.h 通信层预设计

记录日期：2026-06-22

## 1. 本轮目标

生成 BQ76952 INT 通信层首版：

- `Int/Int_BQ76952.h`
- `Int/Int_BQ76952.c`

本轮只实现底层通信和少量无业务策略的便捷读取，不实现 SOC、均衡策略、保护策略、任务调度、显示、CAN。

## 2. 输入文件

- `Int/Int_BQ76952_BSP.h`
- `docs/review/int_bq76952_bsp_review.md`
- `docs/agent_reports/bq76952_bsp_register_facts_2026-06-22.md`
- `docs/rules/hardware_rules.md`
- `official_chip_docs_files/TI_BQ76952_Technical_Reference_Manual_sluuby2b.pdf`
- `official_chip_docs_files/TI_BQ769x2_Software_Development_Guide_sluaa11.pdf`

## 3. 边界

允许实现：

- I2C direct command read/write。
- command-only subcommand。
- subcommand read。
- Data Memory read/write。
- I2C CRC8 可选校验。
- transfer buffer checksum 校验。
- 读取 device number、battery status、alarm status、FET status、单体电压。
- 清除 alarm status。
- 进入/退出 CONFIG_UPDATE。

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

默认 public API：

```c
void Int_BQ76952_SetCrcEnabled(bool enabled);
bool Int_BQ76952_IsCrcEnabled(void);
Int_BQ76952_StatusTypeDef Int_BQ76952_ReadDirect(uint8_t command, uint8_t *data, uint8_t len);
Int_BQ76952_StatusTypeDef Int_BQ76952_WriteDirect(uint8_t command, const uint8_t *data, uint8_t len);
Int_BQ76952_StatusTypeDef Int_BQ76952_SendSubcommand(uint16_t subcommand);
Int_BQ76952_StatusTypeDef Int_BQ76952_ReadSubcommand(uint16_t subcommand, uint8_t *data, uint8_t len);
Int_BQ76952_StatusTypeDef Int_BQ76952_ReadDataMemory(uint16_t address, uint8_t *data, uint8_t len);
Int_BQ76952_StatusTypeDef Int_BQ76952_WriteDataMemory(uint16_t address, const uint8_t *data, uint8_t len);
Int_BQ76952_StatusTypeDef Int_BQ76952_ReadDeviceNumber(uint16_t *device_number);
Int_BQ76952_StatusTypeDef Int_BQ76952_ReadBatteryStatus(uint16_t *status);
Int_BQ76952_StatusTypeDef Int_BQ76952_ReadAlarmStatus(uint16_t *status);
Int_BQ76952_StatusTypeDef Int_BQ76952_ClearAlarmStatus(uint16_t mask);
Int_BQ76952_StatusTypeDef Int_BQ76952_ReadFetStatus(uint8_t *status);
Int_BQ76952_StatusTypeDef Int_BQ76952_ReadCellVoltage(uint8_t cell_index, int16_t *cell_mv);
Int_BQ76952_StatusTypeDef Int_BQ76952_EnterConfigUpdate(void);
Int_BQ76952_StatusTypeDef Int_BQ76952_ExitConfigUpdate(void);
```

说明：

- API 数量超过 12 个，因为 BQ76952 通信层需要同时覆盖 direct/subcommand/data memory 三类访问，并提供 bring-up 常用读取函数；这些便捷函数不写业务策略。
- 后续如用户要求压缩 API，可只保留底层 8 个通信 API，把便捷函数放入 COM 或 APP。

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
- 子命令完成等待采用轮询 `0x3E/0x3F` echo，超时返回 `INT_BQ76952_ERROR_TIMEOUT`。
- 所有 public API 检查空指针和长度。
- 不默认启用 CRC，bring-up 初期由上层调用 `SetCrcEnabled` 决定。

## 8. 任务卡

任务卡编号：`BQ76952_COMM_001`

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
- 所有 public API 返回状态。
- 无业务策略。
- 不写 `BQ76952_CELL_MASK_6S_HW_DRAFT` 到芯片。
