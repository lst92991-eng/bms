# App_BQ76952 首版预设计

记录日期：2026-06-24

本轮只写 BQ76952 的 APP 层教学版骨架。旧项目只参考流程和代码颗粒度，不迁移 BQ76930 寄存器、阈值、GPIO、换算公式。

## 1. 本轮目标

生成：

- `App/App_BQ76952.h`
- `App/App_BQ76952.c`

首版 APP 层只做业务编排和策略缓存：

- 保存 BQ 测量快照。
- 根据快照判断本地故障标志。
- 根据快照计算逻辑均衡 mask。
- 预留 SOC 更新入口。
- 预留充电/放电允许请求。

## 2. 分层边界

APP 层允许：

- 消费未来 `COM_BQ76952` 提供的语义化测量快照。
- 维护本地 `Snapshot`。
- 计算 6 节逻辑电芯的均衡候选。
- 记录充电/放电允许请求，但不直接操作 FET。

APP 层禁止：

- 直接调用 `Int_BQ76952_ReadDirect()` / `WriteDirect()` / `ReadDataMemory()` / `WriteDataMemory()` 作为正式业务路径。
- 写 `BQ76952_CELL_MASK_6S_HW_DRAFT` 或任何 Data Memory 配置。
- 自动清除告警。
- 发送 `RESET`、`SHUTDOWN`、`ALL_FETS_ON/OFF` 等危险 subcommand。
- 读取或控制 `BMS_WAKE`、`BMS_CHIP_SHUT`、`BMS_CHIP_ONLINE`。
- 引入 FreeRTOS task、mutex、printf、动态内存。
- 与 SC8815、OLED、CAN、EEPROM 混写。

## 3. 首版 public API

```c
void App_BQ76952_Init(void);
App_BQ76952_StatusTypeDef App_BQ76952_UpdateMeasurements(const App_BQ76952_MeasurementTypeDef *measurement);
App_BQ76952_StatusTypeDef App_BQ76952_UpdateProtectionStatus(uint16_t alarm_status, uint8_t fet_status);
App_BQ76952_StatusTypeDef App_BQ76952_UpdateSoc(uint32_t delta_ms);
App_BQ76952_StatusTypeDef App_BQ76952_UpdateBalance(void);
void App_BQ76952_SetChargeEnable(bool enabled);
void App_BQ76952_SetDischargeEnable(bool enabled);
bool App_BQ76952_IsReady(void);
bool App_BQ76952_HasFault(void);
void App_BQ76952_GetSnapshot(App_BQ76952_SnapshotTypeDef *snapshot);
```

说明：

- `UpdateMeasurements()` 的输入未来由 COM 层填充，本轮不创建 COM。
- `UpdateBalance()` 只输出逻辑 cell mask：bit0 到 bit5 对应物理第 1 到第 6 串；后续由 COM 映射到 BQ76952 host balancing mask。
- `SetChargeEnable()` / `SetDischargeEnable()` 只记录 APP 请求，不直接操作 BQ FET。
- `UpdateSoc()` 在容量宏未配置时返回 `APP_BQ76952_ERROR_NOT_READY`。

## 4. 首版策略参数

以下为教学版默认值，后续必须由用户、电芯规格和硬件实测确认：

- 均衡最低单体电压：`3900mV`。
- 均衡启动压差：`30mV`。
- 均衡温度窗口：`0C` 到 `45C`。
- 单体电压有效范围：`2500mV` 到 `4300mV`。

这些参数只影响 APP 本地策略缓存，不写入 BQ76952。

## 5. 任务卡

任务卡编号：`APP_BQ76952_DRAFT_001`

允许读取：

- 本 pre 文档。
- `docs/rules/hardware_rules.md`
- `docs/logic/hardware_interface_reservation.md`
- `Int/Int_BQ76952.h`
- 旧项目 `App_BatMan.c/.h`

允许写入：

- `App/App_BQ76952.h`
- `App/App_BQ76952.c`

禁止修改：

- `Int/*`
- `Com/*`
- `docs/rules/*`
- `docs/state/*`
- 旧项目目录

验收标准：

- 不包含 `Int_BQ76952.h`。
- 不调用 `Int_BQ76952_*`。
- 不包含 FreeRTOS、printf、动态内存。
- public API 与本 pre 一致。
- 代码风格直观，函数短，中文步骤注释清楚。
