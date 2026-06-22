# SC8815 BSP 寄存器事实报告

记录日期：2026-06-22

本报告来自本地 PDF 抽取，用于 `Int_SC8815_BSP.h` 首版。这里只记录可追溯事实，不写通信代码。

## 1. 读取文件

- `official_chip_docs_files/Southchip_SC8815_Datasheet_User_Provided.pdf`
- `official_chip_docs_files/Southchip_SC8815_EVM_User_Guide_20241217.pdf`
- `docs/rules/hardware_rules.md`
- `README.md`

## 2. 直接事实

### 2.1 I2C

来源：SC8815 datasheet page 15-16、23。

- 7-bit I2C 地址：`0x74`。
- 8-bit 写地址：`0xE8`。
- 8-bit 读地址：`0xE9`。
- I2C 支持 standard mode 100 kbit/s 和 fast mode 400 kbit/s。
- 本项目硬件约束：SC8815 硬件 IIC3 线接反/不可用，后续通信必须用 `PA6/PA7` GPIO 软件 IIC。

### 2.2 本项目硬件约束

来源：`docs/rules/hardware_rules.md`。

- SC8815 只允许用于 6S 三元锂充电方向。
- 禁止 OTG、反向输出、反向供电。
- `#CE = PB1`，低有效。
- `PSTOP = PB0`，高电平 standby。
- `INT = PA5`，ISR 只能置标志。
- 输入采样电阻 `R5 = 10mΩ`。
- 电池采样电阻 `R14 = 10mΩ`。
- `IBUS_RATIO = 3`。
- `IBAT_RATIO = 6`。
- VBATS 外部分压最终要求：`R17 = 100kΩ`，`R18 = 5.1kΩ`。
- 目标充电电压约 `24.73V`，不超过 6S 三元锂满充 `25.2V`。
- Bring-up 输入限流 `500mA`，电池充电限流 `300mA 或 500mA`。
- 首版默认输入限流 `1500mA`，电池充电限流 `1000mA`。
- 软件硬上限：输入 `3000mA`，电池充电 `2000mA`。

### 2.3 充电/放电模式

来源：SC8815 datasheet page 12-13。

- `EN_OTG = 0`：charging mode，电流从 VBUS 流向 VBAT。
- `EN_OTG = 1`：discharging mode，电流从 VBAT 流向 VBUS。
- 本项目禁止 `EN_OTG = 1`。
- SC8815 内部电池电压设置 `CSEL` 只支持 1S-4S；本项目是 6S，必须使用 `VBAT_SEL = 1` 的外部 VBATS 分压。
- 外部分压目标：`VBAT = 1.2V * (1 + RUP / RDOWN)`。
- `RUP=100kΩ`、`RDOWN=5.1kΩ` 时，目标约 `24.73V`。

### 2.4 Register Map

来源：SC8815 datasheet page 23-35。

| 地址 | 寄存器 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `0x00` | `VBAT_SET` | `0x01` | 电池目标电压、外部分压选择、IR 补偿 |
| `0x01` | `VBUSREF_I_SET` | `0x31` | OTG 内部 VBUS 参考高 8 bit |
| `0x02` | `VBUSREF_I_SET2` | `11xx xxxx` | OTG 内部 VBUS 参考低 2 bit |
| `0x03` | `VBUSREF_E_SET` | `0x7C` | OTG 外部 VBUS 参考高 8 bit |
| `0x04` | `VBUSREF_E_SET2` | `11xx xxxx` | OTG 外部 VBUS 参考低 2 bit |
| `0x05` | `IBUS_LIM_SET` | `0xFF` | 输入侧电流限制 |
| `0x06` | `IBAT_LIM_SET` | `0xFF` | 电池侧电流限制 |
| `0x07` | `VINREG_SET` | `0x2C` | 充电 VINREG 参考 |
| `0x08` | `RATIO` | `0x38` | 电流/电压比例 |
| `0x09` | `CTRL0_SET` | `0x04` | OTG、VINREG、频率、死区 |
| `0x0A` | `CTRL1_SET` | `0x01` | 充电电流选择、涓流、终止、FB、OVP |
| `0x0B` | `CTRL2_SET` | `0x01` | FACTORY、dither、slew |
| `0x0C` | `CTRL3_SET` | `0x02` | PGATE、GPO、ADC、loop、EOC、PFM |
| `0x0D` | `VBUS_FB_VALUE` | `0x00` | VBUS ADC 高 8 bit |
| `0x0E` | `VBUS_FB_VALUE2` | `0x00` | VBUS ADC 低 2 bit |
| `0x0F` | `VBAT_FB_VALUE` | `0x00` | VBAT ADC 高 8 bit |
| `0x10` | `VBAT_FB_VALUE2` | `0x00` | VBAT ADC 低 2 bit |
| `0x11` | `IBUS_VALUE` | `0x00` | IBUS ADC 高 8 bit |
| `0x12` | `IBUS_VALUE2` | `0x00` | IBUS ADC 低 2 bit |
| `0x13` | `IBAT_VALUE` | `0x00` | IBAT ADC 高 8 bit |
| `0x14` | `IBAT_VALUE2` | `0x00` | IBAT ADC 低 2 bit |
| `0x15` | `ADIN_VALUE` | `0x00` | ADIN ADC 高 8 bit |
| `0x16` | `ADIN_VALUE2` | `0x00` | ADIN ADC 低 2 bit |
| `0x17` | `STATUS` | `0x00` | AC_OK、INDET、VBUS_SHORT、OTP、EOC |
| `0x18` | reserved | `0x00` | 保留 |
| `0x19` | `MASK` | `0x80` | 中断 mask |
| `0x1A` | reserved | `0x00` | 保留 |
| `0x1B` | reserved | `xxx0 0000` | 保留 |

## 3. 关键 bit 事实

### 3.1 `0x00 VBAT_SET`

- bit7-6 `IRCOMP`：0/20/40/80 mΩ，设置需 PSTOP 高。
- bit5 `VBAT_SEL`：0 内部设置，1 外部分压设置，设置需 PSTOP 高。
- bit4-3 `CSEL`：内部 VBAT 设置时的电芯数，只支持 1S-4S。
- bit2-0 `VCELL_SET`：内部每节目标电压 4.1V-4.5V。

本项目结论：6S 必须使用 `VBAT_SEL=1`，不使用 `CSEL/VCELL_SET` 决定目标电压。

### 3.2 `0x08 RATIO`

- bit7-6 reserved，内部使用，不要覆盖。
- bit5 reserved，内部使用，不要覆盖，默认 1。
- bit4 `IBAT_RATIO`：0 = 6x，1 = 12x。
- bit3-2 `IBUS_RATIO`：01 = 6x，10 = 3x，00/11 不允许。
- bit1 `VBAT_MON_RATIO`：0 = 12.5x，1 = 5x。
- bit0 `VBUS_RATIO`：0 = 12.5x，1 = 5x。

本项目结论：`IBAT_RATIO=6x`、`IBUS_RATIO=3x`、电压监测默认 12.5x，保留 bit5=1，所以推荐草案值为 `0x28`。

### 3.3 `0x09 CTRL0_SET`

- bit7 `EN_OTG`：0 充电，1 放电/OTG。
- bit6-5 reserved，内部使用，不要覆盖。
- bit4 `VINREG_RATIO`：0 = 100x，1 = 40x。
- bit3-2 `FREQ_SET`：150/300/300/450 kHz。
- bit1-0 `DT_SET`：20/40/60/80 ns。

本项目结论：`EN_OTG` 必须保持 0。

### 3.4 `0x0A CTRL1_SET`

- bit7 `ICHAR_SEL`：0 以 IBUS 作为充电电流基准，1 以 IBAT 作为充电电流基准。
- bit6 `DIS_TRICKLE`：1 禁止涓流。
- bit5 `DIS_TERM`：1 禁止自动终止。
- bit4 `FB_SEL`：仅放电模式使用，0 内部 VBUS，1 外部 FB。
- bit3 `TRICKLE_SET`：0 = 70%，1 = 60%。
- bit2 `DIS_OVP`：1 禁用放电模式 OVP。
- bit1 reserved，内部使用，不要覆盖。
- bit0 reserved，内部使用，应保持 1。

本项目结论：`DIS_TRICKLE`、`DIS_TERM`、`FB_SEL`、`DIS_OVP` 都属于 guard 重点。

### 3.5 `0x0C CTRL3_SET`

- bit7 `EN_PGATE`：PGATE 控制。
- bit6 `GPO_CTRL`：GPO 控制。
- bit5 `AD_START`：启动 ADC。
- bit4 `ILIM_BW_SEL`：电流限制环路带宽。
- bit3 `LOOP_SET`：环路响应。
- bit2 `DIS_ShortFoldBack`：禁用 VBUS 短路折返。
- bit1 `EOC_SET`：EOC 电流阈值 1/25 或 1/10。
- bit0 `EN_PFM`：放电模式 PFM。

本项目结论：`DIS_ShortFoldBack` 禁止置 1，`EN_PFM` 属于 OTG/放电相关功能。

### 3.6 ADC 公式

- VBUS/VBAT ADC：`value10 = 4 * HIGH + LOW + 1`。
- `VBUS = value10 * VBUS_RATIO * 2mV`。
- `VBAT = value10 * VBAT_MON_RATIO * 2mV`。
- `ADIN = value10 * 2mV`。
- `IBUS(A) = value10 * 2 / 1200 * IBUS_RATIO * 10mΩ / RS1`。
- `IBAT(A) = value10 * 2 / 1200 * IBAT_RATIO * 10mΩ / RS2`。

### 3.7 `0x0B CTRL2_SET`

- bit7-4 reserved，内部使用，不要覆盖。
- bit3 `FACTORY`：手册要求 MCU 上电后写 1。
- bit2 `EN_DITHER`：启用频率抖动；启用后 PGATE/DITHER 引脚不再作为 PGATE 驱动。
- bit1-0 `SLEW_SET`：VBUS 动态变化斜率，00/01/10/11 分别对应 1/2/4/8 mV/us。

### 3.8 ADC 低 2 bit 寄存器

- `VBUS_FB_VALUE2`、`VBAT_FB_VALUE2`、`IBUS_VALUE2`、`IBAT_VALUE2`、`ADIN_VALUE2` 的有效低 2 bit 位于 bit7-6。
- bit5-0 为 reserved。

### 3.9 `0x17 STATUS` 和 `0x19 MASK`

- `STATUS` bit6 `AC_OK`、bit5 `INDET`、bit3 `VBUS_SHORT`、bit2 `OTP`、bit1 `EOC`。
- `STATUS` bit7、bit4、bit0 reserved。
- `MASK` bit6 `AC_OK_Mask`、bit5 `INDET_Mask`、bit3 `VBUS_SHORT_Mask`、bit2 `OTP_Mask`、bit1 `EOC_Mask`，1 表示对应中断禁用。
- `MASK` bit7、bit4、bit0 为 reserved/内部位，读改写必须保留。

## 4. 不确定项

- SC8815 `FB/ADIN` 在本项目上的具体外部连接仍需结合原理图复核。
- R17/R18 实物是否已经改为 `100kΩ/5.1kΩ` 需要 bring-up 前确认。
- BSP 只定义寄存器和 bit；实际 guard 应在后续 `Int_SC8815.c` 写寄存器路径实现。

## 5. 对当前模块的影响

- 可以生成 `docs/pre/int_sc8815_bsp_pre.md`。
- 可以生成 `Int/Int_SC8815_BSP.h`。
- BSP.h 必须把 OTG/反向输出相关 bit 标为禁用/危险。
