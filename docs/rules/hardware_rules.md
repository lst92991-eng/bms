# 新 BMS 项目硬件事实与硬约束

记录日期：2026-06-22

更新日期：2026-07-12

本文档记录会长期影响软件实现的硬件事实。后续任何 `pre`、代码实现、review 都必须以这里为强约束；若实物、BOM、`.ioc` 或原理图更新导致冲突，先更新本文档，再改代码。

## 0. 本轮复核来源

当前 BQ/SC 关键硬件事实综合以下资料与人工确认：

- 新项目 BMS 板：`full_netlist (4).csv`、`SCH_机器人BMS板_2026-06-17.pdf`。
- 新项目控制板：`full_netlist (5).csv`、`SCH_Schematic1_2026-06-17.pdf`。
- 旧项目资料只用于历史对比和文档风格，不得覆盖当前实物确认。
- 人工确认记录：`docs/wordflow/manual_confirmations.md`。

## 1. 项目基本硬件

- 电池：6 串三元锂 21700。
- 最高满充电压：`25.2V`。
- 用户确认运行电流：充电 `5A`，放电 `10A`，放电方向 `VBAT -> BMS_OUT-`。
- 完整放电链路已通过当前阶段测试；后续只做必要回归。
- SC8815 VBATS 外部分压名义目标：`25.2V`；实际截止电压受 SC8815 基准和电阻精度影响，必须通过充电曲线测试确认。
- 控制板 MCU：`STM32G0B1CBT6`。
- 充电控制器：`SC8815QDER`。
- BMS AFE：`BQ76952PFBR`。
- OLED：`HS96L03W2C03 / SSD1315 / 128x64 / I2C`。
- EEPROM：`M24C64-RMN6TP / 64Kbit / I2C`。
- CAN-FD 收发器：`TJA1051T/3`。

## 2. SC8815 硬约束

- SC8815 只允许用于 6S 三元锂充电方向。
- 禁止 OTG、反向输出、反向供电。
- SC8815 7-bit I2C 地址固定为 `0x74`。
- 网表命名为 I2C3，但用户已确认 IIC3 线接反/不可用，必须使用 `PA6/PA7` GPIO 软件模拟 IIC。
- `PA7 = SCL`，`PA6 = SDA`，`PA5 = INT`，`PB1 = #CE`，`PB0 = PSTOP`。
- `PA5 / SC8815_INT` 必须在 CubeMX 中配置为 EXTI；ISR 只置 pending flag，禁止在 ISR 中访问软件 IIC。
- `#CE` 低有效；高电平 disable。
- `PSTOP` 高电平 standby；低电平才允许功率级工作。
- Init 必须先建立安全态：`PSTOP=high`、`#CE=high`；不得在 Init 中启动充电。
- SCL/SDA 外部上拉：`R53/R52=2k` 到 `SYS_3V3`。
- INT 外部上拉：`R20=10k`。
- PSTOP/#CE 外部上拉：`R46/R19=10k`。
- 输入采样电阻：`R5=10mΩ`。
- 电池采样电阻：`R14=10mΩ`。
- `IBUS_RATIO=3`，`IBAT_RATIO=6`。
- VBATS 外部分压实物值：`R17=200kΩ`（`BMS+ -> VBATS`），`R18=10kΩ`（`VBATS -> GND`）；用户已于 2026-07-10 确认改焊完成。
- 按数据手册公式 `VBAT = 1.2V * (1 + R17 / R18)`，上述阻值的名义目标为 `25.2V`。旧网表中的 `R17/R18=0Ω` 仅作为改板前历史证据。
- SC8815 输入侧限流 `IBUS` 与电池侧充电限流 `IBAT` 必须分开处理，禁止把电池侧 `5A` 目标直接套到输入侧。
- 软件默认输入限流：`1500mA`。
- Release 默认电池充电限流：`1000mA`。当前 VBATS 目标为 `25.2V`
  （`4.20V/Cell`），因此按 EVE 规格书的标准充电电流执行。
- Bring-up 输入限流：`500mA`。
- Bring-up 电池充电限流：`300mA` 或 `500mA`，确认链路稳定后再升到 `3000mA`。
- 软件硬上限：输入 `3000mA`，电池充电 `5000mA`。
- 电池侧 `2.5A@4.15V/Cell` 与 `5A@4.10V/Cell` 只能在电压目标联锁、
  标定和 HIL/实板充电曲线全部通过后启用；不得在当前 `4.20V/Cell` Release
  曲线中直接套用。原 `3A/5A` 目标保留为工程验证项，不是发布默认值。

SC8815 软件必须拦截：

- `EN_OTG = 1`。
- OTG / 反向输出相关寄存器配置。
- `FB_SEL` 进入反向输出相关模式。
- 关闭涓流、终止、OVP、短路折返等关键保护。
- 电流限值为 `0A`、低于 `300mA`、输入侧高于 `3000mA` 或电池侧高于 `5000mA`。
- 与 `VBAT_SEL=1` 外部分压模式冲突的配置。
- 运行充电时修改关键配置位。

## 3. BQ76952 硬约束

- BQ76952 位于 BMS 板，控制板通过跨板接口连接。
- BQ76952 7-bit I2C 地址固定为 `0x08`，8-bit write `0x10`，8-bit read `0x11`。
- 当前项目与实板通信默认使用 `I2C CRC disabled`；INT 层必须继续保留 CRC 支持与切换能力，但不得再把“默认开启 CRC”作为当前硬件事实。
- `PB6 = I2C1_SCL`，`PB7 = I2C1_SDA`。
- `PB4 = BMS_INT/ALERT`，外部上拉 `R56=10k`，配置为 EXTI；ISR 只置标志。
- 新版硬件已删除原 `PB3 / BMS_WAKE / TS2` 外部唤醒通道。该信号不是普通人机按键；当前软件不得依赖或驱动 PB3 完成 BQ 唤醒。
- `BMS_CHIP_SHUT / RST_SHUT` 通过跨板接口和相关电路连接，短复位/长关断时序后续放 BSP/GPIO 层细化。
- `REG18 -> BMS_CHIP_ONLINE`。
- 电芯接口 `CN2` 为 1x7P：`CELL_6+`、`CELL_5+`、`CELL_4+`、`CELL_3+`、`CELL_2+`、`CELL_1+`、`CELL_1-`。
- BQ VC 映射：`VC16 -> CELL_6+`，`VC15~VC12 -> CELL_5+`，`VC11~VC9 -> CELL_4+`，`VC8~VC6 -> CELL_3+`，`VC5~VC2 -> CELL_2+`，`VC1 -> CELL_1+`，`VC0 -> CELL_1-`。
- 低边电流采样：`R18=5mΩ/6W`，位于 `GND` 与 `BMS_OUT-` 之间。若旧网表或迁移说明仍写 `0.5mΩ`，以后续实物/用户确认值为准。
- `TS1/TS3` 为靠近低边采样电阻的热敏电阻；NTC 型号、阻值、B 值和分压参数仍需确认。
- 设计放电电流目标：`10A`；完整放电测试已通过当前阶段验证。
- BQ76952 过流、短路、欠压、过温阈值仍需结合充电测试与标定记录完成文档闭环，不得仅凭代码常量宣告最终定版。
- BQ INT 层必须按 BQ76952 官方 TRM/Software Guide 设计，不得复用旧 BQ76930 寄存器表。

## 4. OLED 与 EEPROM

- OLED 与 EEPROM 共用 I2C2：`PA11=SCL`、`PA12=SDA`。
- 总线外部上拉：`R47/R51=2k`。
- OLED 地址未从网表确认，需通过模块资料或 I2C 扫描确认。
- EEPROM `A0/A1/A2` 接 GND，M24C64 常规 7-bit 地址通常为 `0x50`，最终以 datasheet/扫描结果确认。
- OLED/EEPROM INT 层不得负责显示业务页面或参数业务结构。
- CAN、EEPROM 等项目要求的外设初始化失败时不得进入正常 RUN；允许进入明确的安全故障态并输出诊断。

## 5. FDCAN

- `PB9 = FDCAN1_TX`。
- `PB8 = FDCAN1_RX`。
- 收发器 `TJA1051T/3`：`VCC=SYS_5V`，`VIO=SYS_3V3`。
- 总线终端 `R30=120Ω`。
- CAN 协议 ID、帧格式、充放电命令属于 `docs/logic`，不写死在 INT 层。

## 6. 调试与 GPIO

- USART1 调试：`PA9=TX`，`PA10=RX`。
- SWD：`PA13=SWDIO`，`PA14=SWCLK`，另有 `NRST/GND`。
- 红灯：`PB13`。
- 绿灯：`PB14`。
- 蜂鸣器：`PB5 / TIM3_CH2`。
- 按键/复用按钮：`PD3 / MUC_GPIO_PD3_BMS_MUX_BTN`；它与已删除的 BQ WAKE/TS2 通道不是同一接口。
- 网络名前缀有 `MUC_` 拼写，软件命名时统一按功能命名，不依赖网名前缀自动生成。

## 7. 电源路径

- 关键电源网：`24V_IN`、`VBUS`、`BMS+`、`BMS_OUT+`、`BMS_OUT-`、`5V_FROM_SC8815`、`SYS_5V`、`SYS_3V3`。
- `U2/U3` 是理想二极管/电源路径控制相关器件。
- `F2` 位于 VBUS 路径，标注 `33V`、`Ihold=500mA`、`Itrip=1A`。
- 控制板包含 5V buck 与 3.3V LDO。
- 若电源路径器件没有 MCU 可控/可测接口，则不单独做 INT，只作为硬件规则和 bring-up 检查项。

## 8. 必须继续确认

- SC8815 `FB/ADIN` 外部连接与目标电压配置方式。
- SC8815 VBATS 在接近满充时的实测电压、实际截止电压及 R17/R18 精度。
- 充电曲线中的 PACK、各单体、电流、温度、SC 状态、BQ 告警和 FET 状态。
- BQ76952 带数据 subcommand 写入和 readback 时序。
- OLED I2C 地址。
- EEPROM 页大小、写周期、地址宽度。
- 新 CubeMX `.ioc` 中 PA5 SC8815_INT 的 EXTI、GPIO 宏命名和中断优先级。
- 6S 21700 实际容量。
- `TS1/TS3` 热敏电阻的 NTC 型号、阻值、B 值和分压参数。
