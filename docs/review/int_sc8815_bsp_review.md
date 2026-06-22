# Int_SC8815_BSP.h Review

记录日期：2026-06-22

## 1. Review 范围

本次 review 覆盖：

- `Int/Int_SC8815_BSP.h`
- `docs/pre/int_sc8815_bsp_pre.md`
- `docs/agent_reports/sc8815_bsp_register_facts_2026-06-22.md`
- `docs/rules/hardware_rules.md`

## 2. Blocking 问题

无当前 blocking 问题。

## 3. Review 中发现并已修正的问题

### 3.1 PDF 表格换行导致 bit 位错位

worker 初版把几个字段按文本顺序误解为低位字段。已按 datasheet page 23-35 修正：

- `VBUSREF_I_SET2`、`VBUSREF_E_SET2` 的低 2 bit 实际位于 bit7:6，bit5:0 为 reserved。
- ADC 低 2 bit 寄存器的有效位实际位于 bit7:6，bit5:0 为 reserved。
- `CTRL2_SET` 为 bit3 `FACTORY`、bit2 `EN_DITHER`、bit1:0 `SLEW_SET`，bit7:4 reserved。
- `STATUS` 为 bit6 `AC_OK`、bit5 `INDET`、bit3 `VBUS_SHORT`、bit2 `OTP`、bit1 `EOC`，bit7/4/0 reserved。
- `MASK` 与 `STATUS` 对应，bit6/5/3/2/1 为 mask 位，bit7/4/0 reserved。

### 3.2 文档同步

已同步修正：

- `docs/agent_reports/sc8815_bsp_register_facts_2026-06-22.md`

## 4. 已检查项

- `Int_SC8815_BSP.h` 只包含 `<stdint.h>`。
- 未出现 HAL、FreeRTOS、GPIO 函数、软件 IIC 函数。
- 未新增 `.c` 文件。
- 未修改 BQ76952 文件。
- I2C 地址与手册一致：7-bit `0x74`，8-bit write `0xE8`，read `0xE9`。
- 关键寄存器地址 `0x00-0x1B` 已定义。
- 51 个关键宏经脚本核对无错误。
- `EN_OTG`、`FB_SEL`、`DIS_OVP`、`DIS_ShortFoldBack`、`EN_PFM`、`DIS_TRICKLE`、`DIS_TERM` 已标记为本项目禁止/危险配置。
- BSP 明确说明 6S 不能用内部 `CSEL/VCELL_SET`，必须用 `VBAT_SEL=1` 外部分压。

## 4.1 继续生成补强复核

2026-06-22 按用户“继续生成 sc8815 的 bsp.h”要求，已在不越过 BSP 边界的前提下补强：

- `#CE` 低有效、`PSTOP` 高 standby 的项目电平常量。
- R17/R18 为 `100kΩ/5.1kΩ` 时的 VBATS 目标计算值 `24729mV`。
- `VBAT_SET`、`RATIO`、`CTRL0_SET`、`CTRL1_SET`、`CTRL2_SET`、`CTRL3_SET` 的手册字段取值枚举。
- VBUSREF 10-bit 编码、ADC 10-bit 编码、限流 DAC、VINREG 的公式常量。
- `MASK.bit0` 虽为内部位，但手册注明 MCU 上电后应写 1，已单独记录为 `SC8815_MASK_POWER_UP_INTERNAL_SET_MASK`。
- `SC8815_PROJECT_RATIO_VALUE` 已改为字段宏组合方式，避免后续手填 `0x28` 时失去可读性。

## 5. 剩余风险

- SC8815 `FB/ADIN` 在本项目上的外部连接仍需结合原理图复核。
- R17/R18 实物是否已改为 `100kΩ/5.1kΩ`，需要 bring-up 前确认。
- BSP 只提供常量，真正的写寄存器 guard 需要在后续 `Int_SC8815.c/.h` 实现。
- 当前没有编译器可用，未做 C 编译器语法检查。

## 6. 结论

`Int/Int_SC8815_BSP.h` 可以作为后续 SC8815 软件 IIC 通信层和安全 guard 的寄存器常量入口。

下一步建议：

1. 生成 `Int_SC8815` 软件 IIC pre，严格限制软件 IIC 六个底层方法。
2. 生成 SC8815 写寄存器 guard pre，先实现禁止 OTG/反向输出/关闭保护/0A 限流的拦截。
