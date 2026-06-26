# 硬件接口预留与职责边界

记录日期：2026-06-23

## 1. 本轮结论

当前硬件逻辑按两个核心原则冻结：

- BQ76952 负责 BMS AFE、采样、告警、FET/均衡相关底层能力；均衡逻辑由软件后续手写，但不能写在 INT 通信层。
- SC8815 负责 6S 充电控制；充电管理、涓流、终止、保护动作优先交给硬件自动完成，软件只做安全态、使能、状态读取、ADC 监测和保守限流。
- 当前运行电流目标：充电 `5A`，放电 `10A`；放电电流方向为 `VBAT -> BMS_OUT-`。

本文件只整理硬件接口预留和分层边界，不定义最终 public API，不进入 COM/APP 实现。

## 2. 资料入口

- `README.md`
- `docs/rules/hardware_rules.md`
- `docs/rules/parallel_work_mechanism.md`
- `docs/state/main_thread_checkpoint.md`
- `full_netlist (4).csv`
- `full_netlist (5).csv`
- `Int/Int_BQ76952_BSP.h`
- `Int/Int_BQ76952.h`
- `Int/Int_SC8815_BSP.h`
- `Int/Int_SC8815.h`
- `docs/pre/int_bq76952_comm_pre.md`
- `docs/pre/int_sc8815_comm_pre.md`
- 本轮只读 agent 输出：`HW_BQ_INTERFACE_FACTS_001`、`HW_SC_INTERFACE_FACTS_001`、`HW_SYSTEM_INTERFACE_FACTS_001`。这些输出是待核验材料，已由主线程结合硬件规则和网表摘要整合。

## 3. BQ76952 接口预留

### 3.1 必须预留的硬件接口

| 接口 | 网络/引脚 | 用途 | 预留要求 |
| --- | --- | --- | --- |
| I2C1 SCL | `PB6 / MCU_I2C1_SCL_PB6_BMS` | BQ76952 通信 | CubeMX 预留 I2C1，句柄名后续与代码适配。 |
| I2C1 SDA | `PB7 / MCU_I2C1_SDA_PB7_BMS` | BQ76952 通信 | 与 SCL 同步确认上拉和速率。 |
| I2C 地址/CRC | `0x08 / 0x10 / 0x11`，默认 CRC 开启 | BQ76952 通信格式 | INT 层保留 CRC 开关，COM 初始化按项目默认开启 CRC。 |
| ALERT/INT | `PB4 / MUC_EXTI4_PB4_BMS_INT` | 告警/事件中断 | 配 EXTI；ISR 只置标志，不访问 I2C。 |
| WAKE | `PB3 / MUC_GPIO_PB3_BMS_WAKE` | BQ 唤醒 | 走 TS2 唤醒路径；GPIO 时序放 BSP 层细化。 |
| CHIP_SHUT/RST_SHUT | `BMS_CHIP_SHUT` | 复位/关断相关电路 | 短复位/长关断时序后续放 BSP/GPIO 层细化。 |
| ONLINE | `BMS_CHIP_ONLINE / REG18` | BQ 在线状态 | 作为 GPIO 输入或状态采样预留。 |
| 电芯接口 | `CN2 1x7P` | 6S 采样 | 使用已确认的物理 Cell1/2/3/4/5/6 -> BQ Cell1/2/6/9/12/16 映射。 |
| 低边采样 | `R18=0.5mΩ/6W` | 电流测量 | 电流方向、校准参数后续确认。 |
| 采样电阻温度 | `TS1/TS3` | 热敏温度 | 靠近低边采样电阻，用于温度补偿/热保护；NTC 参数后续确认。 |

跨板 7P 接口 `U26` 需要整体保留：`ONLINE`、`WAKE`、`ALERT`、`I2C1_SCL`、`I2C1_SDA`、`CHIP_SHUT`、电池正侧相关脚。

### 3.2 BQ 软件均衡的边界

因为均衡逻辑后续由软件手写，所以必须预留低层能力，但不能在 INT 层写策略。

INT 层应该保留或后续补齐的低层能力：

- direct command、subcommand、Data Memory 读写。
- 读取单体电压、堆叠电压、电流、温度、Alarm、FET status。
- 读取 `CBSTATUS1/2/3`，确认实际均衡通道。
- 写 host controlled balancing mask 的受控底层入口，或明确由通用 subcommand/Data Memory 接口承载。
- 读取/清除 `ALARM_CB` 相关状态。
- 进入/退出 `CONFIG_UPDATE`，但不决定配置值。
- 电流方向按硬件定义分别解释：充电看 `BMS_OUT- -> VBAT`，放电看 `VBAT -> BMS_OUT-`。

COM 层以后负责：

- 把 cell、temperature、current、alarm、FET、balancing status 整理成 AFE 状态快照。
- 统一处理通信状态、CRC 模式、告警快照、bring-up 读数验证。
- 提供中间语义，例如“请求设置均衡 mask”“查询均衡状态”，但不硬编码最终策略。

APP 层以后负责：

- 均衡开启/停止条件、轮询周期、最大同时均衡数量、温度限制、充放电状态限制。
- SOC/SOH、保护阈值业务、故障锁存、FET 业务策略。
- 显示、CAN 上报、用户交互。

INT 层禁止：

- 自动均衡策略。
- SOC。
- 保护阈值业务选择。
- FET 业务开关策略。
- 在 INT 层直接写 Vcell Mode 或均衡 mask；这些由 COM/APP 通过语义接口完成。

## 4. SC8815 接口预留

### 4.1 必须预留的硬件接口

| 接口 | 网络/引脚 | 用途 | 预留要求 |
| --- | --- | --- | --- |
| 软件 IIC SCL | `PA7 / MCU_I2C3_SCL_PA7_SC8815` | SC8815 SCL | 配 GPIO 开漏输出；不能用硬件 I2C3。 |
| 软件 IIC SDA | `PA6 / MCU_I2C3_SDA_PA6_SC8815` | SC8815 SDA | 配 GPIO 开漏/输入读取能力；不能用硬件 I2C3。 |
| INT | `PA5 / MCU_EXTI5_PA5_SC8815_INT` | 充电状态/故障中断 | 配 EXTI；ISR 只置标志。 |
| #CE | `PB1 / MCU_GPIO_PB1_SC8815_#CE` | 芯片使能 | 低有效；安全态高电平 disable。 |
| PSTOP | `PB0 / MCU_GPIO_PB0_SC8815_PSTOP` | 功率级停机 | 高电平 standby；低电平允许功率级工作。 |
| VBATS 分压 | `R17/R18` | 6S 目标电压 | 网表为 0Ω，实物必须改为 `100kΩ/5.1kΩ` 后才允许按目标启动。 |
| 采样电阻 | `R5/R14=10mΩ` | 输入/电池电流 | 对应 `IBUS_RATIO=3`、`IBAT_RATIO=6`。 |

### 4.2 硬件自动充电的边界

SC8815 的充电管理由硬件完成。软件只保留：

- `InitSafe`：建立 `PSTOP=high`、`#CE=high` 安全态，不启动充电。
- 芯片使能/禁用和 standby/work 控制。
- 读取 `AC_OK`、`INDET`、`VBUS_SHORT`、`OTP`、`EOC` 等状态。
- ADC 读取 `VBUS/VBAT/ADIN/IBUS/IBAT`，用于 bring-up 和运行监测。
- 设置输入限流、电池充电限流和必要安全配置。
- 所有写寄存器路径必须经过 guard。

软件禁止：

- 实现 CC/CV、涓流、终止、保护动作等微观充电状态机。
- 用 `#CE/PSTOP` 高频模拟充电阶段切换。
- 提供 OTG、反向输出、VBUSREF 输出电压、FB 反向输出相关 API。
- 关闭涓流、自动终止、OVP、短路折返等保护。
- 配置 0A 限流或低于项目最低限流。
- 在非 standby 状态修改关键配置位。

## 5. 系统级接口预留

| 模块 | 硬件接口 | INT 层只做 | 不放入 INT 的内容 |
| --- | --- | --- | --- |
| OLED | I2C2 `PA11/PA12`，SSD1315，128x64 | 初始化、命令/数据写入、基础传输 | 页面、文案、告警显示策略。 |
| EEPROM | I2C2 `PA11/PA12`，M24C64 | 字节/页读写、写完成轮询、边界保护 | 参数结构、校验、版本迁移。 |
| CAN-FD | `PB9 TX`、`PB8 RX`，TJA1051T/3 | FDCAN 基础收发 | 协议 ID、帧格式、命令语义。 |
| USART/SWD | `PA9/PA10`，`PA13/PA14` | 调试串口可预留；SWD 只作硬件事实 | 业务日志框架和调试策略。 |
| GPIO Board | `PB13/PB14/PB5/PD3`，以及 BMS wake/shut/alert/online | LED、蜂鸣器、按键、板级 GPIO 读写 | 告警策略、按键业务、显示联动。 |
| 电源路径 | `24V_IN/VBUS/BMS+/BMS_OUT+/BMS_OUT-/SYS_5V/SYS_3V3` | 有 MCU 可测/可控脚时才做 INT | 理想二极管、保险丝、TVS 等不可控器件只进 bring-up。 |

系统协作方向：

- BQ 均衡状态、单体异常、FET/告警状态可以显示、存储和上报，但策略不放在 OLED/EEPROM/CAN INT。
- SC 充电状态、输入异常、限流和 ADC 值可以显示、存储和上报，但 SC INT 仍只提供状态和安全控制。
- `BMS_INT/ALERT`、`SC8815_INT` 都必须走“ISR 置标志，任务读取状态”的模式。

## 6. Bring-up 前必须确认

- SC8815 `R17/R18` 实物是否已改为 `100kΩ/5.1kΩ`，否则禁止启动充电链路。
- SC8815 `PA6/PA7` 软件 IIC 波形、ACK、上拉和时序。
- SC8815 `#CE/PSTOP` 默认电平、INT 触发和状态寄存器读取。
- SC8815 `FB/ADIN` 外部连接。
- BQ76952 `Vcell Mode=0x8923` 写入/读回，以及均衡通道实际发热位置需要实物 bring-up 复核。
- BQ76952 `TS1/TS3` 热敏电阻的 NTC 型号、阻值、B 值和分压参数。
- BQ76952 CRC 通信、带数据 subcommand 写入和 readback 时序需要实物 bring-up 复核。
- `BMS_WAKE`、`BMS_CHIP_SHUT`、`BMS_CHIP_ONLINE` 的电平有效性和 MCU 控制/检测方向。
- OLED I2C 地址。
- EEPROM 页大小、写周期、地址宽度、WP 脚状态。
- CAN 连接器、终端是否固定启用、CANH/CANL 命名。
- 新 CubeMX `.ioc` 中的外设句柄、GPIO 宏名、EXTI 线和中断优先级。

## 7. 下一步建议

建议下一步先写 `docs/pre/int_gpio_board_pre.md`，覆盖 LED、蜂鸣器、按键、`BMS_WAKE`、`BMS_CHIP_SHUT`、`BMS_INT/ALERT`、`BMS_CHIP_ONLINE` 的 INT 边界。

不建议直接进入 COM/APP，也不建议直接写 BQ 均衡策略。若要补 BQ 均衡相关低层能力，应先单独建立 BQ balancing low-level pre，明确只做状态读取和 host mask 控制，不做策略。
