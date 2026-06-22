# 新 BMS 项目硬件事实与硬约束

记录日期：2026-06-22

本文档记录会长期影响软件实现的硬件事实。后续任何 `pre`、代码实现、review 都必须以这里为强约束；若实物、BOM、`.ioc` 或原理图更新导致冲突，先更新本文档，再改代码。

## 1. 项目基本硬件

- 电池：6 串三元锂 21700。
- 最高满充电压：`25.2V`。
- SC8815 目标充电电压：约 `24.73V`。
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
- `#CE` 低有效；高电平 disable。
- `PSTOP` 高电平 standby；低电平才允许功率级工作。
- Init 必须先建立安全态：`PSTOP=high`、`#CE=high`；不得在 Init 中启动充电。
- ISR 不得访问 IIC，只能置标志，后续由任务读取状态。
- SCL/SDA 外部上拉：`R53/R52=2k` 到 `SYS_3V3`。
- INT 外部上拉：`R20=10k`。
- PSTOP/#CE 外部上拉：`R46/R19=10k`。
- 输入采样电阻：`R5=10mΩ`。
- 电池采样电阻：`R14=10mΩ`。
- `IBUS_RATIO=3`，`IBAT_RATIO=6`。
- VBATS 外部分压最终要求：`R17=100kΩ`，`R18=5.1kΩ`。
- 当前网表中 `R17/R18=0Ω`，若两颗 0Ω 同贴会通过 `VBATS` 把 `BMS+` 短到 `GND`；bring-up 前必须实物确认改焊/DNP 状态。
- 软件默认输入限流：`1500mA`。
- 软件默认电池充电限流：`1000mA`。
- Bring-up 输入限流：`500mA`。
- Bring-up 电池充电限流：`300mA 或 500mA`。
- 软件硬上限：输入 `3000mA`，电池充电 `2000mA`。

SC8815 软件必须拦截：

- `EN_OTG = 1`。
- OTG / 反向输出相关寄存器配置。
- `FB_SEL` 进入反向输出相关模式。
- 关闭涓流、终止、OVP、短路折返等关键保护。
- 电流限值为 `0A`。
- 与 `VBAT_SEL=1` 外部分压模式冲突的配置。
- 运行充电时修改关键配置位。

## 3. BQ76952 硬约束

- BQ76952 位于 BMS 板，控制板通过 7P 跨板接口连接。
- `PB6 = I2C1_SCL`，`PB7 = I2C1_SDA`。
- `PB4 = BMS_INT/ALERT`，外部上拉 `R56=10k`。
- `PB3 = BMS_WAKE`。
- `BMS_CHIP_SHUT` 通过跨板接口和开关相关电路连接。
- `REG18 -> BMS_CHIP_ONLINE`。
- 电芯接口 `CN2` 为 1x7P：`CELL_6+`、`CELL_5+`、`CELL_4+`、`CELL_3+`、`CELL_2+`、`CELL_1+`、`CELL_1-`。
- BQ VC 映射：`VC16 -> CELL_6+`，`VC15~VC12 -> CELL_5+`，`VC11~VC9 -> CELL_4+`，`VC8~VC6 -> CELL_3+`，`VC5~VC2 -> CELL_2+`，`VC1 -> CELL_1+`，`VC0 -> CELL_1-`。
- 低边电流采样：`R18=0.5mΩ/6W`，位于 `GND` 与 `BMS_OUT-` 之间。
- BQ INT 层必须按 BQ76952 官方 TRM/Software Guide 重新设计，不得复用旧 BQ76930 寄存器表。

## 4. OLED 与 EEPROM

- OLED 与 EEPROM 共用 I2C2：`PA11=SCL`、`PA12=SDA`。
- 总线外部上拉：`R47/R51=2k`。
- OLED 地址未从网表确认，写代码前需要模块资料或 I2C 扫描确认。
- EEPROM `A0/A1/A2` 接 GND，M24C64 常规 7-bit 地址通常为 `0x50`，最终以 datasheet/扫描结果确认。
- OLED/EEPROM INT 层不得负责显示业务页面或参数业务结构。

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
- 按键/复用按钮：`PD3 / MUC_GPIO_PD3_BMS_MUX_BTN`。
- 网络名前缀有 `MUC_` 拼写，软件命名时统一按功能命名，不依赖网名前缀自动生成。

## 7. 电源路径

- 关键电源网：`24V_IN`、`VBUS`、`BMS+`、`BMS_OUT+`、`BMS_OUT-`、`5V_FROM_SC8815`、`SYS_5V`、`SYS_3V3`。
- `U2/U3` 是理想二极管/电源路径控制相关器件。
- `F2` 位于 VBUS 路径，标注 `33V`、`Ihold=500mA`、`Itrip=1A`。
- 控制板包含 5V buck 与 3.3V LDO。
- 若电源路径器件没有 MCU 可控/可测接口，则不单独做 INT，只作为硬件规则和 bring-up 检查项。

## 8. 必须继续确认

- SC8815 `FB/ADIN` 外部连接与目标电压配置方式。
- SC8815 R17/R18 实物是否已改为 `100kΩ/5.1kΩ`。
- BQ76952 I2C 地址、CRC/PEC、子命令格式。
- OLED I2C 地址。
- EEPROM 页大小、写周期、地址宽度。
- 新 CubeMX `.ioc` 中的外设句柄和 GPIO 宏命名。
- 6S 21700 实际容量、最大充放电电流、温度传感器数量和位置。
