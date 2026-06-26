# BQ76952 旧项目风格平移计划书

记录日期：2026-06-25

## 1. 目标

本计划用于把旧 UPS 项目 `App_BatMan` 中围绕 BQ 芯片的业务逻辑，平移到新项目 BQ76952 硬件环境。

本文档定位为 Phase 1 bring-up / 教学型平移计划，不是长期量产架构终态。待 BQ76952 参数、保护阈值、电流校准、温度采样和 FET 状态读回稳定后，再评估是否把部分芯片语义重新收敛到 COM/BQIF 层。

本次目标不是继续加厚三层封装，而是回到旧项目适合教学的写法：

- INT 层保留当前 BQ76952 已有基本通信方法，不强行改薄。
- COM 层像旧项目一样薄，只放查表、简单换算或纯数据辅助。
- APP 层承接主要业务流程，可以直接调用 `Int_BQ76952_*` 基本方法。
- APP 层保留旧项目那种顺序清楚、变量外露、`printf` 调试、中文步骤注释的风格。
- 每个 `.c` 文件的 `static` 函数最好不超过 4 个；超过必须说明原因。

## 2. 已读取资料

旧项目：

- `旧项目信息/旧项目代码/ups/ups001/MDK-ARM/App/App_BatMan.c`
- `旧项目信息/旧项目代码/ups/ups001/MDK-ARM/App/App_BatMan.h`
- `旧项目信息/旧项目代码/ups/ups001/MDK-ARM/App/App_Soc.c`
- `旧项目信息/旧项目代码/ups/ups001/MDK-ARM/App/App_Soc.h`
- `旧项目信息/旧项目代码/ups/ups001/MDK-ARM/Com/Com_Bq769.c`
- `旧项目信息/旧项目代码/ups/ups001/MDK-ARM/Com/Com_Bq769.h`
- `旧项目信息/旧项目代码/ups/ups001/MDK-ARM/Int/Int_BQ769.c`
- `旧项目信息/旧项目代码/ups/ups001/MDK-ARM/Int/Int_BQ769.h`
- `旧项目信息/旧项目代码/ups/ups001/MDK-ARM/Int/Int_BQ769_BSP.h`

新项目：

- `docs/rules/hardware_rules.md`
- `docs/logic/hardware_interface_reservation.md`
- `Int/Int_BQ76952.h`
- `Int/Int_BQ76952.c`
- `Int/Int_BQ76952_BSP.h`
- `Com/Com_BQ76952.h`
- `Com/Com_BQ76952.c`
- `App/App_BatMan.h`
- `App/App_BatMan.c`
- `App/App_BQ76952.h`
- `App/App_BQ76952.c`

本轮硬件资料已重新核对网表与原理图 PDF：

- 新 BMS 板：`full_netlist (4).csv`、`SCH_机器人BMS板_2026-06-17.pdf`
- 新控制板：`full_netlist (5).csv`、`SCH_Schematic1_2026-06-17.pdf`
- 旧控制板：`旧项目信息/old/full_netlist (4).csv`、`旧项目信息/old/SCH_UPS控制板_2026-06-18.pdf`
- 旧电池板：`旧项目信息/old/full_netlist (5).csv`、`旧项目信息/old/SCH_6节14500板_2026-06-18.pdf`

### 2.1 硬件复核结论

- 旧控制板 BQ 页为 `BQ7693003DBTR`，围绕 `DSG/CHG/SDA/SCL/TS1/VC0..VC10/SRP/SRN/ALERT` 工作，旧软件寄存器表只适用于 BQ76930。
- 旧电池板为 6 节 14500/AA 电池结构，电芯网络为 `CELL0..CELL6`，功率接口是 XT30。
- 新 BMS 板为 `BQ76952PFBR`，标题为 `6S 三元锂 高输出能力BMS`，功率接口是 XT90，低边采样为 `R18=0.5mΩ/6W`。
- 新 BMS 板均衡电阻为 `R3/R4/R5/R6/R7/R19=62Ω/1W`，物理 6 串映射到 BQ `Cell1/2/6/9/12/16`。
- 新 BMS 板 `TS1/TS3` 为靠近采样电阻的热敏电阻，用于温度补偿/热保护；NTC 参数仍待确认。
- 新控制板为 SC8815 充电路径，`R5/R14=10mΩ/3W`，电池侧充电目标 `5A`，放电电流由 BMS 路径承担，目标 `10A`。
- 新控制板 `VBATS` 分压网表仍显示 `R17/R18=0Ω`，bring-up 前必须确认实物改焊或 DNP 状态。

## 3. 旧项目 BQ 逻辑抽取

### 3.1 旧 INT 层

旧 `Int_BQ769` 只有少量直接硬件方法：

| 方法                     | 旧项目职责                                       |
| ------------------------ | ------------------------------------------------ |
| `Int_BQ769_WakeUp()`   | 拉 GPIO 唤醒 BQ，延时等待芯片启动                |
| `Int_BQ769_Ship()`     | 写`SYS_CTRL1.SHUT_A/SHUT_B` 进入 ship/休眠序列 |
| `Int_BQ769_Reset()`    | 先 ship，再 wake                                 |
| `Int_BQ769_WriteReg()` | 单字节寄存器写，附 I2C CRC                       |
| `Int_BQ769_ReadReg()`  | 连续寄存器读，每字节校验 CRC                     |
| `Int_BQ769_Read()`     | 头文件声明存在，但当前`.c` 未实现              |

旧 INT 的风格特点：

- 很薄，直接围绕 GPIO/I2C/CRC。
- 不做业务状态机。
- 不返回复杂业务语义。
- 使用 `printf` 打印 I2C/CRC 错误。
- 旧 BQ76930 的寄存器事实放在 `Int_BQ769_BSP.h`。

### 3.2 旧 COM 层

旧 `Com_Bq769` 基本不访问 BQ 芯片，只做查表：

| 方法/数据                           | 旧项目职责         |
| ----------------------------------- | ------------------ |
| `resist_temper_table[181]`        | NTC 电阻到温度表   |
| `voltage_soc_ocv_table[101]`      | 单体 OCV 到 SOC 表 |
| `getTableIndexByValue()`          | 通用查表索引函数   |
| `Com_BQ769_getTemperByResist()`   | 电阻转温度         |
| `Com_BQ769_getPercentByVoltage()` | 电压转 SOC 百分比  |

旧 COM 的风格特点：

- COM 很薄。
- COM 不包 BQ 寄存器读写。
- COM 不做保护策略。
- COM 不做均衡策略。
- COM 只服务 APP/SOC 的查表需求。

### 3.3 旧 APP 层

旧 `App_BatMan` 是 BQ 业务主文件。它直接调用 INT 和 COM：

| 旧 APP 方法                        | 旧项目逻辑                                                                  |
| ---------------------------------- | --------------------------------------------------------------------------- |
| `App_BatMan_Init()`              | reset/wake、读 gain/offset、配置 ADC/CC/FET、配置保护、读回调试、初始化 SOC |
| `App_BatMan_loadCellsVoltage()`  | 按`cell_reg_indexes[]` 读取物理 6 串电压，换算成 `cell_mv[]`            |
| `App_BatMan_loadBatVoltage()`    | 读取总压并换算`bat_mv`                                                    |
| `App_BatMan_loadCurrent()`       | 读取 CC，换算`current_a`，电流方向在新项目里需要重新定义                  |
| `App_BatMan_loadTemperature()`   | 读取 TS1，换算电阻，再调用 COM 查表                                         |
| `App_BatMan_CalcSoc()`           | 调用`App_Soc_Update()`，再做显示滤波                                      |
| `App_BatMan_CellBalance()`       | 找最低电压、按温度/压差/最低电压筛候选、相邻互斥、写均衡寄存器、读回打印    |
| `App_BatMan_SetChargeState()`    | 读改写`SYS_CTRL2.CHG_ON`                                                  |
| `App_BatMan_SetDischargeState()` | 读改写`SYS_CTRL2.DSG_ON`                                                  |

旧 APP 的风格特点：

- 大部分业务都能在一个文件里看到。
- 初始化和周期任务是“步骤式”的，不像框架。
- 使用全局变量：`cell_mv[]`、`bat_mv`、`current_a`、`tempr`、`soc_percent`。
- 注释写业务步骤，适合教学。
- `static` 函数只有少量几个：`configParameters()`、`loadGainAndOffset()`；逻辑没有拆得很碎。

## 4. 新项目硬件事实

新项目不能继承旧 BQ76930 的寄存器和公式，只继承业务流程风格。

新 BQ76952 相关硬件事实：

- 电芯：`EVE-INR21700/50E`，6 串三元锂；第一版软件参数先按 6S1P 推导，若实物为 6SxP，则容量和电流按并数 P 倍放大。
- 用户确认运行目标：充电 `5A`、放电 `10A`。
- 放电方向：`VBAT -> BMS_OUT-`。
- 满充电压：`25.2V`。
- SC8815 原计划目标充电电压：约 `24.73V`，即 `4.1217V/cell`；该值高于 EVE 50E 在 `5A` 最大持续充电条件下的 `4.10V/cell`，需要按第 19.3 节重新评估。
- BQ：`BQ76952PFBR`。
- BQ 通信：`I2C1`。
- BQ 地址：7-bit `0x08`，8-bit write `0x10`，8-bit read `0x11`。
- BQ 默认 I2C CRC 开启。
- `PB6 = I2C1_SCL`，`PB7 = I2C1_SDA`。
- `PB4 = BMS_INT/ALERT`。
- `PB3 = BMS_WAKE`，走 TS2 唤醒路径。
- `BMS_CHIP_SHUT / RST_SHUT` 通过板级电路连接，短复位/长关断时序后续细化。
- 低边电流采样：`R18=0.5mΩ/6W`。
- `TS1/TS3` 为靠近低边采样电阻的热敏电阻，型号按 `MF52AT` 系列 `10K/100K` 温度传感器处理，端子规格 `XH2.54/PH2.0/1.25/1.5`；B 值和分压仍待确认。
- 均衡支路：每串 `62Ω/1W`。

### 4.1 电芯事实

| 项目               | EVE-INR21700/50E 规格                                                                        |
| ------------------ | -------------------------------------------------------------------------------------------- |
| 典型容量           | `5000mAh`                                                                                  |
| 最小容量           | `4900mAh`                                                                                  |
| 标称电压           | `3.65V`                                                                                    |
| 标准充电           | CCCV，`1000mA`，`4.20V`，`100mA cut-off`                                               |
| 倍率充电           | CCCV，`2500mA`，`4.15V`，`100mA cut-off`                                               |
| 最大持续充电       | CCCV，`5000mA`，`4.10V`，`100mA cut-off`                                               |
| 标准放电           | `1000mA`，`2.50V cut-off`                                                                |
| 最大持续放电       | `15000mA`，`2.50V cut-off`                                                               |
| 标准充放电电压范围 | `4.20V ~ 2.50V`                                                                            |
| 初始内阻           | `<=20mΩ`                                                                                  |
| 10A 放电能力       | `2C`，相对容量 `>=93%`                                                                   |
| 15A 放电能力       | `3C`，相对容量 `>=90%`                                                                   |
| 循环寿命测试条件   | `1000mA` 充到 `4.20V`，`1000mA` 放到 `3.00V cut-off`，`1000` 次后容量 `>=70% Ci` |
| 出厂电压           | `3.45V ~ 3.60V`，约 `20%~30% SOC`                                                        |

### 4.2 6S1P Pack 推导参数

当前计划书第一版按 6S1P 写 APP 参数和 bring-up 目标；如果实物不是 6S1P，而是 6SxP，容量和允许电流必须按并数 P 倍放大，并重新确认热设计。

| 项目                | 6S1P 推导值                       |
| ------------------- | --------------------------------- |
| 典型容量            | `5000mAh`                       |
| 最小容量            | `4900mAh`                       |
| 标称电压            | `21.9V`                         |
| 标准满充电压        | `25.2V`                         |
| 5A 最大持续充电目标 | `4.10V/cell`，即 `24.6V pack` |
| 2.5A 倍率充电目标   | `4.15V/cell`，即 `24.9V pack` |
| 标准 1A 充电目标    | `4.20V/cell`，即 `25.2V pack` |
| 放电截止            | `2.50V/cell`，即 `15.0V pack` |
| 寿命友好低电参考    | `3.00V/cell`，即 `18.0V pack` |

新 BQ76952 6S 物理映射：

| 物理电芯 | BQ 通道 | Direct Command | 均衡 bit |
| -------- | ------- | -------------- | -------- |
| Cell1    | Cell1   | `0x14`       | bit0     |
| Cell2    | Cell2   | `0x16`       | bit1     |
| Cell3    | Cell6   | `0x1E`       | bit5     |
| Cell4    | Cell9   | `0x24`       | bit8     |
| Cell5    | Cell12  | `0x2A`       | bit11    |
| Cell6    | Cell16  | `0x32`       | bit15    |

因此新项目建议使用以下业务宏。这里是计划书建议，不要求本轮立刻写入代码：

```c
#define APP_BATMAN_CELL_COUNT                  (6u)
#define APP_BATMAN_CELL_CAP_TYP_MAH            (5000u)
#define APP_BATMAN_CELL_CAP_MIN_MAH            (4900u)
#define APP_BATMAN_CELL_NOMINAL_MV             (3650u)
#define APP_BATMAN_PACK_NOMINAL_MV             (21900u)
#define APP_BATMAN_CELL_FULL_STD_MV            (4200u)
#define APP_BATMAN_PACK_FULL_STD_MV            (25200u)
#define APP_BATMAN_CELL_CHG_RATE_MV            (4150u)
#define APP_BATMAN_PACK_CHG_RATE_MV            (24900u)
#define APP_BATMAN_CELL_CHG_1C_MV              (4100u)
#define APP_BATMAN_PACK_CHG_1C_MV              (24600u)
#define APP_BATMAN_CELL_DSG_CUTOFF_MV          (2500u)
#define APP_BATMAN_PACK_DSG_CUTOFF_MV          (15000u)
#define APP_BATMAN_CELL_DSG_LIFE_MV            (3000u)
#define APP_BATMAN_PACK_DSG_LIFE_MV            (18000u)
#define APP_BATMAN_CHG_STD_CURRENT_MA          (1000)
#define APP_BATMAN_CHG_RATE_CURRENT_MA         (2500)
#define APP_BATMAN_CHG_MAX_CURRENT_MA          (5000)
#define APP_BATMAN_DSG_STD_CURRENT_MA          (1000)
#define APP_BATMAN_DSG_TARGET_CURRENT_MA       (10000)
#define APP_BATMAN_DSG_MAX_CONT_CURRENT_MA     (15000)
#define BQ76952_VCELL_MODE_6S                  (0x8923u)
```

禁止继承：

- 旧 `cell_reg_indexes[6] = {0, 1, 4, 5, 6, 9}`。
- 旧 `BQ_CELLBAL1/BQ_CELLBAL2` 写法。
- 旧 `BQ_SYS_CTRL2.CHG_ON / DSG_ON` FET 控制方式。
- 旧 ADC gain/offset 电压公式。
- 旧 TS1 热敏公式；新硬件虽然有 `TS1/TS3`，但必须按新 NTC 参数重做换算。
- 旧 BQ76930 保护阈值寄存器公式。

## 5. 新项目分层方向

本章仍按 Phase 1 bring-up / 教学型平移来约束：APP 可以直接调用 `Int_BQ76952_*`，COM 不重新变成厚 wrapper，也不新增复杂 snapshot/status 框架。长期量产终态等 BQ76952 Data Memory、保护阈值、电流方向、温度采样和 FET 读回全部稳定后再讨论。

### 5.1 INT 层保留现状

当前 `Int_BQ76952.h` 暴露的是 BQ76952 基本访问模型：

- CRC 开关。
- Direct Command read/write。
- command-only subcommand。
- read subcommand。
- write subcommand with data。
- Data Memory read/write。
- Enter/Exit ConfigUpdate。
- 条件编译 bring-up `ReadDeviceNumber()`。

这些方法可以不改。原因是 BQ76952 不像旧 BQ76930 只有裸寄存器读写，必须保留 direct/subcommand/Data Memory/transfer buffer checksum 这些协议原语。

INT 层后续只允许新增板级方法的前提：

- 已确认 CubeMX GPIO 宏名。
- `BMS_WAKE`、`BMS_CHIP_SHUT`、`BMS_CHIP_ONLINE` 的有效电平在原理图 PDF 红字里有说明，实施时按图摘录，不在此处写死。
- 方法命名必须避免裸 `Reset()` / `Shutdown()` 歧义，例如 `Int_BQ76952_BoardWake()`。

本计划第一阶段不改 INT public API。

### 5.2 COM 层改薄

按旧项目风格，`Com_BQ76952` 不再做厚语义封装，不再作为 APP 和 INT 中间的大层。

建议 COM 只保留这类内容：

| 内容                                  | 是否保留                                              | 原因                                                            |
| ------------------------------------- | ----------------------------------------------------- | --------------------------------------------------------------- |
| OCV-SOC 表                            | 保留                                                  | 旧 COM 就是表驱动换算                                           |
| 温度电阻表                            | 暂缓                                                  | 新项目`TS1/TS3` 通道已确认，但 NTC 型号、分压和换算参数未确认 |
| `Com_BQ76952_GetPercentByVoltage()` | 保留                                                  | 给 SOC 使用                                                     |
| `Com_BQ76952_GetTemperByResist()`   | 等 NTC 参数确认后保留                                 | 当前不能套旧 TS1 公式                                           |
| BQ direct read wrapper                | 不作为主线                                            | APP 可直接调用 INT                                              |
| BQ FET wrapper                        | 不作为主线                                            | APP 用 INT + BSP subcommand 直接表达                            |
| BQ default config 大表                | 移到 APP 的`configParameters/configProtectSet` 风格 | 让教学流程可见                                                  |
| physical mask -> BQ mask              | 建议放 APP 或 BSP 宏                                  | 这是 BQ76952 6S 硬件映射，不必厚封装                            |

目标 `Com_BQ76952.c` 的 `static` 函数数量：0 到 2 个。

如果保留查表通用函数 `getTableIndexByValue()`，它可以是唯一 `static` 函数。

### 5.3 APP 层承接主逻辑

`App_BatMan.c` 作为新 BQ76952 的主业务文件，直接调用：

- `Int_BQ76952_ReadDirect()`
- `Int_BQ76952_WriteDirect()`
- `Int_BQ76952_SendSubcommand()`
- `Int_BQ76952_ReadSubcommand()`
- `Int_BQ76952_WriteSubcommandData()`
- `Int_BQ76952_ReadDataMemory()`
- `Int_BQ76952_WriteDataMemory()`
- `Int_BQ76952_EnterConfigUpdate()`
- `Int_BQ76952_ExitConfigUpdate()`

APP 层保留这些可教学的全局变量：

```c
uint16_t cell_mv[APP_BATMAN_CELL_COUNT];
uint32_t stack_mv;
uint32_t pack_mv;
int32_t current_ma;
float current_a;
uint16_t cell_min_mv;
uint16_t cell_max_mv;
uint16_t cell_avg_mv;
uint16_t cell_delta_mv;
int16_t temp_ic_c;
uint16_t alarm_status;
uint16_t battery_status;
uint16_t balance_mask;
bool fault_active;
bool charge_allowed;
bool discharge_allowed;
float soc_percent;
float display_soc_percent;
```

APP 层使用旧项目类似数组表达新硬件映射：

```c
static uint8_t cell_voltage_commands[6] = {
    BQ76952_CMD_CELL1_VOLTAGE,
    BQ76952_CMD_CELL2_VOLTAGE,
    BQ76952_CMD_CELL6_VOLTAGE,
    BQ76952_CMD_CELL9_VOLTAGE,
    BQ76952_CMD_CELL12_VOLTAGE,
    BQ76952_CMD_CELL16_VOLTAGE,
};

static uint16_t cell_balance_bits[6] = {
    BQ76952_VCELL_MODE_CELL1_MASK,
    BQ76952_VCELL_MODE_CELL2_MASK,
    BQ76952_VCELL_MODE_CELL6_MASK,
    BQ76952_VCELL_MODE_CELL9_MASK,
    BQ76952_VCELL_MODE_CELL12_MASK,
    BQ76952_VCELL_MODE_CELL16_MASK,
};
```

这比厚 COM wrapper 更接近旧项目 `cell_reg_indexes[]` 的教学风格，同时不会继承旧映射。

## 6. 旧逻辑到新逻辑的平移表

| 旧项目逻辑                         | 新项目平移方案                                                                                                             |
| ---------------------------------- | -------------------------------------------------------------------------------------------------------------------------- |
| `Int_BQ769_Reset()`              | 第一阶段用 BQ76952 subcommand`BQ76952_SUBCMD_RESET` 或暂不做板级 reset；`BMS_WAKE/RST_SHUT` 等 GPIO 确认后再补板级函数 |
| `loadGainAndOffset()`            | 不平移；BQ76952 direct command 已给 mV/userA 语义，校准另做 bring-up                                                       |
| `configParameters()`             | 平移为 APP 内初始化步骤：CRC 默认、进入 ConfigUpdate、写`Vcell Mode=0x8923`、DA config、FET options、balancing config    |
| `configProtectSet()`             | 平移为 APP 内保护配置步骤，但只写已确认 Data Memory；不要套旧 OV/UV trip 公式                                              |
| `App_BatMan_loadCellsVoltage()`  | 直接调用`Int_BQ76952_ReadDirect()` 读取 6 个 direct command，按物理顺序填 `cell_mv[]`                                  |
| `App_BatMan_loadBatVoltage()`    | 读取`STACK_VOLTAGE` 和 `PACK_PIN_VOLTAGE`                                                                              |
| `App_BatMan_loadCurrent()`       | 读取 BQ 电流读数，按 BQ76952 校准后的 userA/user mA 解释；放电方向按`VBAT -> BMS_OUT-` 建立符号约定                      |
| `App_BatMan_loadTemperature()`   | 第一版读取`INT_TEMPERATURE`；NTC 参数确认后读取 `TS1/TS3`，用于采样电阻温度补偿/热保护                                 |
| `App_BatMan_CalcSoc()`           | 保留调用`App_Soc_Update()`，容量参数必须改成真实 21700 pack 容量                                                         |
| `App_BatMan_CellBalance()`       | 继承旧算法结构，最后写`BQ76952_SUBCMD_CB_ACTIVE_CELLS` 的 16-bit mask                                                    |
| `App_BatMan_SetChargeState()`    | 不再写旧`SYS_CTRL2.CHG_ON`；改写 `FET_CONTROL` 或发送 BQ76952 FET subcommand                                           |
| `App_BatMan_SetDischargeState()` | 不再写旧`SYS_CTRL2.DSG_ON`；改写 `FET_CONTROL` 或发送 BQ76952 FET subcommand                                           |

## 7. 建议文件调整

### 7.1 保留

- `Int/Int_BQ76952.h`
- `Int/Int_BQ76952.c`
- `Int/Int_BQ76952_BSP.h`
- `App/App_BatMan.h`
- `App/App_BatMan.c`
- `Com/Com_BQ76952.h`
- `Com/Com_BQ76952.c`

### 7.2 降级为废弃草稿

- `App/App_BQ76952.h`
- `App/App_BQ76952.c`

原因：

- 它使用 snapshot/status enum 风格，不像旧项目。
- `App/App_BQ76952.c` 当前有 6 个 `static` helper，已经超过本计划目标。
- 它把业务拆散成状态更新 API，不如 `App_BatMan.c` 适合教学。

处理方式：

- 第一阶段不删除，避免误伤引用。
- 在计划实施时确认无人引用后，改名归档或从工程中移除。

## 8. `static` 函数数量预算

| 文件                  | 当前 static 函数 |       目标 | 是否超过 4 | 说明                                                                                                     |
| --------------------- | ---------------: | ---------: | ---------- | -------------------------------------------------------------------------------------------------------- |
| `Int/Int_BQ76952.c` |                7 |   保持现状 | 是         | INT 是协议层，CRC、checksum、echo、transfer、CFGUPDATE 轮询都有协议边界；用户已允许当前 INT 基本方法不改 |
| `Com/Com_BQ76952.c` |                4 |        0-2 | 否         | 当前刚好 4，但计划会继续变薄，尽量只保留查表 helper                                                      |
| `App/App_BatMan.c`  |                0 |        0-3 | 否         | 业务函数保持外露，方便教学和单步调试                                                                     |
| `App/App_BQ76952.c` |                6 | 不作为主线 | 是         | 该文件建议废弃或归档，不继续扩展                                                                         |

如果后续 `App_BatMan.c` 需要 static 函数，建议只允许这几个：

- `App_BatMan_ReadU16Le()`：解析 BQ little-endian 两字节。
- `App_BatMan_U16ToLe()`：写 subcommand/Data Memory 前打包两字节。
- `configParameters()`：旧项目同款初始化参数配置。
- `configProtectSet()`：旧项目同款保护参数配置。

超过 4 个时，优先把逻辑摊回公开业务函数或局部代码块，不继续拆 helper。

## 9. APP 层建议函数列表

`App_BatMan.h` 保留旧项目式 API：

```c
void App_BatMan_Init(void);
void App_BatMan_Task(uint16_t interval_ms);

void App_BatMan_LoadCellsVoltage(void);
void App_BatMan_LoadBatVoltage(void);
void App_BatMan_LoadCurrent(void);
void App_BatMan_LoadTemperature(void);
void App_BatMan_LoadBqStatus(void);
void App_BatMan_CalcSoc(uint16_t interval_ms);
void App_BatMan_CellBalance(void);

void App_BatMan_SetChargeState(uint8_t charge_state);
void App_BatMan_SetDischargeState(uint8_t discharge_state);
void App_BatMan_PrintDebug(void);
```

可以保留 `App_BatMan_PrintDebug()`，但不必把所有 debug 都抽象成日志框架。

## 10. APP 初始化流程

建议 `App_BatMan_Init()` 写成和旧项目一样的步骤：

```c
void App_BatMan_Init(void)
{
    // 1. 设置 BQ76952 通信 CRC 模式
    // 2. 唤醒/复位：第一阶段先读 DeviceNumber；GPIO 时序确认后再补 Wake/RST_SHUT
    // 3. 进入 ConfigUpdate
    // 4. 设置参数：Vcell Mode、DA config、FET options、balancing config
    // 5. 设置保护：只写已确认保护项，未确认阈值先不写
    // 6. 退出 ConfigUpdate
    // 7. 清启动告警，关闭均衡
    // 8. 允许 BQ 管理 FET，但 APP 只表达 charge/discharge allowed
    // 9. 读取一次电压/电流/温度初值
    // 10. 按 EVE 50E 容量初始化 SOC
}
```

注意：这里允许 APP 调 INT，所以步骤 3 到 7 可以直接调用 `Int_BQ76952_*`，不用 COM 包一层。

## 11. 采样流程

建议 `App_BatMan_Task()` 保持顺序清晰：

```c
void App_BatMan_Task(uint16_t interval_ms)
{
    App_BatMan_LoadCellsVoltage();
    App_BatMan_LoadBatVoltage();
    App_BatMan_LoadCurrent();
    App_BatMan_LoadTemperature();
    App_BatMan_LoadBqStatus();
    App_BatMan_CalcSoc(interval_ms);
    App_BatMan_CellBalance();
    App_BatMan_SetChargeState(charge_allowed);
    App_BatMan_SetDischargeState(discharge_allowed);
    App_BatMan_PrintDebug();
}
```

不建议再引入 snapshot 结构或多层状态对象。

### 11.1 SOC 与容量参数

- 6S1P 第一版默认容量按 `5000mAh`，保守容量按 `4900mAh`。
- `App_Soc_Update()` 的容量参数必须从旧 UPS 项目值改为 EVE 50E pack 容量；若后续确认是 6SxP，则容量按 P 倍放大。
- BQ 电流读数正负号未上板确认前，SOC 库仑积分只能作为待验证逻辑，不能作为最终剩余电量判断。
- `3.45V ~ 3.60V/cell` 可作为出厂/运输状态 `20%~30% SOC` 的粗略参考。
- OCV-SOC 表如果没有 EVE 50E 厂家完整曲线，第一版可以沿用旧表结构，但必须标记为“待用 EVE 50E 曲线替换/校准”。

## 12. 均衡逻辑平移

继承旧项目均衡算法：

1. 找最低单体电压。
2. 遍历 6 节电芯，筛候选：
   - 温度在允许范围。
   - 当前电芯电压大于均衡最低电压。
   - 当前电芯和最低电芯压差超过阈值。
3. 相邻物理电芯不能同时均衡，保留更高电压者。
4. 生成 BQ76952 16-bit host balancing mask。
5. 用 `Int_BQ76952_WriteSubcommandData(BQ76952_SUBCMD_CB_ACTIVE_CELLS, data, 2)` 写入。
6. 用 `Int_BQ76952_ReadSubcommand(BQ76952_SUBCMD_CBSTATUS1, data, len)` 读回打印。

第一版均衡策略补充：

- 均衡启动电压建议放在高 SOC 区间，例如 `cell_mv > 4000mV`，或者更保守阈值；最终根据实测温升和策略确认。
- 均衡压差阈值可以先保守使用 `20mV~30mV`，标记为待实测确认。
- 均衡 mask 必须是 BQ76952 16-bit mask，不是 6-bit physical mask。
- 物理 `Cell1/2/3/4/5/6` 对应 BQ `Cell1/2/6/9/12/16`，对应 mask `bit0/bit1/bit5/bit8/bit11/bit15`。

第一版建议仍只允许同时均衡 1 节，原因：

- 新硬件每路均衡电阻为 `62Ω/1W`。
- BQ 默认 `CB_MAX_CELLS=1` 更保守。
- 上板时更容易确认哪一路发热。

## 13. 充放电控制平移

旧项目：

- APP 直接写 `SYS_CTRL2.CHG_ON`。
- APP 直接写 `SYS_CTRL2.DSG_ON`。

新项目：

- APP 仍保留 `App_BatMan_SetChargeState()` / `App_BatMan_SetDischargeState()`。
- 函数内部不再通过 COM wrapper。
- 函数内部直接用 INT 写 BQ76952 FET subcommand 或 `FET_CONTROL` data subcommand。
- 真实 MOS 是否导通仍由 BQ76952 保护状态决定。

建议第一版逻辑：

- `charge_state = 0`：写 `FET_CONTROL.CHG_OFF=1`。
- `charge_state = 1`：清 `FET_CONTROL.CHG_OFF`。
- `discharge_state = 0`：写 `FET_CONTROL.DSG_OFF=1`。
- `discharge_state = 1`：清 `FET_CONTROL.DSG_OFF`。

APP 可保留一个全局 `fet_off_mask`，像旧项目 `BQ769_RegisterGroup` 一样让读改写过程可见。

业务含义补充：

- `5A` 充电在电芯规格书中是允许的，但对应 `4.10V/cell`，不是满充 `4.20V/cell`。
- `10A` 放电在规格书中属于 `2C`，且相对容量 `>=93%`，因此可作为系统目标值。
- `15A` 是电芯最大持续放电，不应直接作为系统长期目标；保护阈值要结合 MOS、采样电阻、PCB 温升和负载峰值实测。
- `charge_state` / `discharge_state` 只是 APP 请求，不保证 BQ FET 实际导通，必须读回 FET status / safety status。

## 14. 保护配置平移

旧项目保护配置包括：

- SCD delay / threshold。
- OCD delay / threshold。
- UV/OV delay。
- OV/UV trip。

新项目原则：

- 不继承旧 BQ76930 寄存器和公式。
- 只把“保护配置在 APP 初始化里集中写”这个流程平移过来。
- BQ76952 保护阈值必须基于 TRM 和实测电流方向/采样校准确认后再写。

第一版可写：

- `Vcell Mode = 0x8923`。
- `DA Configuration`。
- `Default Alarm Mask`。
- `FET Options`。
- `Balancing Configuration`。
- `CB_MAX_CELLS = 1`。
- 均衡温度/电压阈值。

第一版不建议写死：

- COV/CUV 具体阈值和恢复条件。
- OCC/OCD/SCD 具体阈值。
- `TS1/TS3` 热敏保护阈值和温度补偿曲线。
- OTP/PF 相关项。
- 10A 放电工作点对应的最终过流分档和延时值。

第一版的原则边界：

- 电芯规格书已经给出业务目标阈值，但 BQ76952 Data Memory 地址、单位、编码、延时公式不得臆造。
- COV/CUV/OCC/OCD/SCD 只能先写“目标值”和“待转 BQ76952 编码”。
- BQ 的 COV 保护阈值不要简单等同于 `4.10V/cell` 充电 CV；正常充电 CV 应由 SC8815/充电策略控制，BQ76952 COV 应作为安全保护。
- CUV 可以以 `2.50V/cell` 作为绝对规格截止参考，`3.00V/cell` 作为寿命友好低电策略参考。

温度策略在本阶段建议只写业务目标，不硬编码 BQ Data Memory 细节：

- 低于 `0℃` 禁止充电。
- `0~15℃` 充电限流到 `<=1A`。
- `15~45℃` 允许最高 `5A` 充电。
- `45~55℃` 充电限流到 `<=1A`。
- 高于 `55℃` 禁止充电。
- 低于 `-20℃` 禁止放电。
- `-20~60℃` 允许放电。
- 高于 `60℃` 禁止放电或进入降级保护。
- `70℃` 作为 cell surface cut-off temperature 的硬边界。
- 以上都是电芯温度策略目标，不等于 TS1/TS3 的最终 BQ76952 Data Memory 编码；TS1/TS3 NTC 型号、B 值、分压公式仍待确认。

## 15. 当前代码与计划的差异

当前新项目已经有一版能跑的三层，但和本计划的旧项目风格还有差异：

| 当前代码                                    | 本计划建议                      |
| ------------------------------------------- | ------------------------------- |
| APP 主要调用 COM                            | APP 可直接调用 INT              |
| COM 提供大量 BQ wrapper                     | COM 改薄，只保留查表/纯辅助     |
| `App_BQ76952.c` 使用 snapshot/status 风格 | 不作为主线                      |
| `App_BatMan.c` 已接近旧流程               | 继续把 BQ 访问从 COM 展开到 APP |
| FET/Config/Readback 在 COM 中               | 平移到 APP 的初始化和控制步骤中 |

## 16. 实施顺序

### Step 1：冻结主入口

- 确认新项目 BQ 主入口只用 `App/App_BatMan.c/.h`。
- `App/App_BQ76952.c/.h` 标记为废弃草稿或从工程移除。

### Step 2：COM 瘦身

- 保留 OCV 表和查表函数。
- 暂停或删除 BQ 访问 wrapper。
- 不在 COM 写 Data Memory。
- 不在 COM 写 FET 或均衡。

### Step 3：APP 展开初始化

- 在 `App_BatMan_Init()` 里直接调用 INT。
- 写出 ConfigUpdate 进入、Data Memory 写、读回、退出的完整步骤。
- 保留 `printf`，每一步失败直接打印。

### Step 4：APP 展开采样

- `App_BatMan_LoadCellsVoltage()` 内部按 `cell_voltage_commands[]` 逐个 `ReadDirect`。
- `App_BatMan_LoadBatVoltage()` 直接读 stack/pack。
- `App_BatMan_LoadCurrent()` 直接读 BQ 电流读数。
- `App_BatMan_LoadTemperature()` 先读 internal temp，NTC 参数确认后补读 `TS1/TS3`。

### Step 5：APP 展开均衡

- 保留旧项目 1/2/3/4 步注释。
- 最后写 BQ76952 16-bit mask。
- 读回 CB status 打印。

### Step 6：APP 展开充放电

- `App_BatMan_SetChargeState()` 直接写 FET control。
- `App_BatMan_SetDischargeState()` 直接写 FET control。
- 保留全局 `charge_allowed/discharge_allowed/fault_active`。

### Step 7：上板前复核

- 扫描旧 BQ76930 符号，确保没有 `BQ_CELLBAL`、`BQ_SYS_CTRL`、`BQ_PROTECT`、`BQ769_RegisterGroup`。
- 扫描 APP 是否仍依赖厚 COM wrapper。
- 检查每个 `.c` 的 `static` 函数数量。

## 17. 验收标准

代码风格验收：

- `App_BatMan.c` 读起来像旧项目：步骤明确、直接读写、中文注释、`printf` 调试。
- APP 业务逻辑可从一个文件顺下来，不需要理解 snapshot/status 框架。
- COM 很薄，不再承接 BQ 大量业务 wrapper。
- INT 保留 BQ76952 必要协议复杂度，不为了像旧项目而破坏芯片访问模型。
- 每个源文件 `static` 函数不超过 4 个；`Int_BQ76952.c` 作为协议层例外，保留现状并注明原因。

硬件正确性验收：

- 6S 映射必须是 `Cell1/2/6/9/12/16`。
- `Vcell Mode` 必须是 `0x8923`。
- I2C 地址必须是 `0x08 / 0x10 / 0x11`。
- 默认 CRC 开启。
- 均衡 mask 必须是 BQ76952 16-bit mask，不是 6-bit physical mask。
- 不继承旧 BQ76930 寄存器、旧 ADC 公式、旧温度公式。

Bring-up 验收：

- DeviceNumber 可读。
- `Vcell Mode=0x8923` 写入后可读回。
- 6 个 direct command 对应的物理电芯电压正确。
- `CB_ACTIVE_CELLS` 写入后，实际发热通道和物理电芯对应。
- FET control 写入后，BQ FET status 与预期一致。
- `TS1/TS3` 读数与采样电阻附近实测温度趋势一致。
- CRC 通信和 transfer buffer checksum 读回稳定。

## 18. 真实待确认项

以下内容不应在计划阶段拍脑袋写死：

- BQ 电流读数符号与 `VBAT -> BMS_OUT-` 放电方向之间的对应关系。
- BQ 电流读数零点、增益和校准参数。
- `TS1/TS3` 热敏电阻的 B 值和分压。
- FET status / safety status 读回解析。
- 均衡通道实际发热验证。

已解决：

- pack 已按 6S1P 处理。
- 电芯型号。
- 典型容量 / 最小容量。
- 标称电压。
- 标准 / 倍率 / 最大持续充电电流与电压。
- 最大持续放电电流。
- 充放电温度范围。
- 出厂电压范围。

部分待确认：

- `TS1/TS3` NTC 型号和端子规格已明确，但 B 值、阻值和分压仍待确认。
- `BMS_WAKE`、`BMS_CHIP_SHUT`、`BMS_CHIP_ONLINE` 的有效电平已在原理图 PDF 红字标注，但 CubeMX 宏名和代码落点仍待确认。
- SC8815 `R17/R18` 实物状态已知为已焊接，但本文不展开 SC 逻辑，仍只保留为板级背景信息。

## 19. EVE INR21700/50E 电芯参数落地

### 19.1 电芯规格

- 型号：`EVE-INR21700/50E`。
- 典型容量：`5000mAh`。
- 最小容量：`4900mAh`。
- 标称电压：`3.65V`。
- 标准充电：`1000mA`，`4.20V`，`100mA cut-off`。
- 倍率充电：`2500mA`，`4.15V`，`100mA cut-off`。
- 最大持续充电：`5000mA`，`4.10V`，`100mA cut-off`。
- 标准放电：`1000mA`，`2.50V cut-off`。
- 最大持续放电：`15000mA`，`2.50V cut-off`。
- 初始内阻：`<=20mΩ`。
- 10A 放电能力：`2C`，相对容量 `>=93%`。
- 15A 放电能力：`3C`，相对容量 `>=90%`。
- 循环寿命测试：`1000mA` 充到 `4.20V`，`1000mA` 放到 `3.00V cut-off`，`1000` 次后容量 `>=70% Ci`。
- 出厂电压：`3.45V ~ 3.60V`，约 `20%~30% SOC`。

### 19.2 6S1P Pack 推导参数

第一版计划按 6S1P 推导：

- 典型容量：`5000mAh`。
- 最小容量：`4900mAh`。
- 标称电压：`21.9V`。
- 标准满充电压：`25.2V`。
- `5A` 最大持续充电对应目标：`4.10V/cell`，即 `24.6V pack`。
- `2.5A` 倍率充电对应目标：`4.15V/cell`，即 `24.9V pack`。
- `1A` 标准充电对应目标：`4.20V/cell`，即 `25.2V pack`。
- 放电截止：`2.50V/cell`，即 `15.0V pack`。
- 寿命友好低电参考：`3.00V/cell`，即 `18.0V pack`。

如果后续确认不是 6S1P，而是 6SxP，则容量和电流按并数 P 倍放大。

### 19.3 对 SC8815 充电目标的修正

- `24.73V / 6 = 4.1217V/cell`。
- 该值高于规格书中 `5A` 最大持续充电对应的 `4.10V/cell`。
- 若坚持 `5A` 充电，建议把充电目标重新评估到约 `24.6V`。
- 若坚持 `24.73V`，则需要降低充电电流目标，或单独评估热、寿命和规格符合性。
- 若追求 `25.2V` 满充，应按 `1A` 标准充电更符合规格书。

### 19.4 对 SOC 的影响

- `6S1P` 默认容量按 `5000mAh`，保守容量按 `4900mAh`。
- `App_Soc_Update()` 的容量参数必须使用 EVE 50E pack 容量。
- 电流方向未上板确认前，SOC 库仑积分只能作为待验证逻辑。
- `3.45V~3.60V/cell` 可作为出厂 / 运输 SOC `20%~30%` 的粗略参考。
- OCV-SOC 表若没有厂家完整曲线，第一版可沿用旧表结构，但必须标记为“待用 EVE 50E 曲线替换/校准”。

### 19.5 对保护阈值的影响

- 保护阈值只写目标值和待转 BQ76952 编码，不臆造 Data Memory 地址、单位和延时公式。
- `COV/CUV/OCC/OCD/SCD` 先保留业务语义，不直接写死寄存器值。
- `COV` 作为安全保护，不等同于 `4.10V/cell` 的正常充电 CV。
- `CUV` 可取 `2.50V/cell` 作为绝对截止参考，`3.00V/cell` 作为寿命友好低电参考。
- `10A` 放电目标合理，但 `15A` 不应直接成为长期系统目标。

### 19.6 对温度策略的影响

- `0℃` 以下禁止充电。
- `0~15℃` 充电限流到 `<=1A`。
- `15~45℃` 允许最高 `5A` 充电。
- `45~55℃` 充电限流到 `<=1A`。
- `55℃` 以上禁止充电。
- `-20℃` 以下禁止放电。
- `-20~60℃` 允许放电。
- `60℃` 以上禁止放电或进入降级保护。
- `70℃` 作为 cell surface cut-off temperature 硬边界。
- 这些都是电芯温度策略目标，不等于 TS1/TS3 的最终 BQ76952 Data Memory 编码；TS1/TS3 NTC 型号、B 值、分压公式仍待确认。

### 19.7 仍未解决项

- BQ 电流读数符号与放电方向之间的对应关系。
- BQ 电流读数零点、增益和校准参数。
- `TS1/TS3` 热敏电阻的 B 值和分压。
- FET status / safety status 读回解析。
- 均衡通道实际发热验证。
