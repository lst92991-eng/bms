# Int_SC8815_BSP.h 预设计

记录日期：2026-06-22

## 1. 本轮目标

创建 SC8815 的 BSP 头文件首版：

- 目标文件：`Int/Int_SC8815_BSP.h`
- 本轮只写头文件，不写 `.c`。
- 只定义寄存器地址、默认值、bit mask、比例/公式常量、项目安全限制。
- 不实现软件 IIC、不实现寄存器读写、不实现 guard 函数。

## 2. 输入文件

- `docs/agent_reports/sc8815_bsp_register_facts_2026-06-22.md`
- `docs/rules/hardware_rules.md`
- `README.md`
- `official_chip_docs_files/Southchip_SC8815_Datasheet_User_Provided.pdf`
- `official_chip_docs_files/Southchip_SC8815_EVM_User_Guide_20241217.pdf`

## 3. Source Gate 结论

- SC8815 是新项目充电控制器，不参考旧 BQ 寄存器。
- README 明确 SC8815 硬件 IIC 接反，后续通信必须是软件 IIC。
- README 要求 SC/BQ 都要有详细 BSP，每一位旁边要有注释。

## 4. Hardware Fact Gate 结论

- SC8815 只允许用于 6S 三元锂充电方向。
- 禁止 OTG、反向输出、反向供电。
- SC8815 7-bit I2C 地址固定 `0x74`。
- 软件 IIC 引脚：`PA7=SCL`、`PA6=SDA`。
- `#CE=PB1` 低有效，`PSTOP=PB0` 高电平 standby。
- 输入采样电阻 `10mΩ`，电池采样电阻 `10mΩ`。
- 项目要求 `IBUS_RATIO=3`，`IBAT_RATIO=6`。
- R17/R18 最终应为 `100kΩ/5.1kΩ`，使 VBATS 外部分压目标约 `24.73V`。

## 5. BSP.h 必须包含

- I2C 地址。
- 项目硬件常量。
- 所有已抽取寄存器地址 `0x00-0x1B`。
- 寄存器 POR 默认值。
- 每个寄存器 bit mask 和 bit shift。
- 保留位 mask，提醒读改写保留。
- 充电方向安全限制宏。
- ADC/限流/电压计算公式常量。
- 推荐/草案配置值：
  - `VBAT_SEL=1` 外部分压。
  - `EN_OTG=0`。
  - `IBUS_RATIO=3x`。
  - `IBAT_RATIO=6x`。
  - 电流限流不得为 0，最低不低于 300mA。
- 禁止/危险 bit：
  - `EN_OTG`
  - `FB_SEL`
  - `DIS_OVP`
  - `DIS_ShortFoldBack`
  - `EN_PFM`
  - `DIS_TRICKLE`
  - `DIS_TERM`

## 6. 命名规则

- 文件名：`Int_SC8815_BSP.h`
- 宏前缀：`SC8815_`
- 寄存器地址：`SC8815_REG_<NAME>`
- 默认值：`SC8815_DEFAULT_<NAME>`
- bit mask：`SC8815_<REGISTER>_<BIT>_MASK`
- bit shift：`SC8815_<REGISTER>_<BIT>_SHIFT`
- 项目限制：`SC8815_PROJECT_*`

## 7. 禁止事项

- 禁止写 `.c`。
- 禁止写 HAL/GPIO/software IIC 函数。
- 禁止把 OTG/反向输出作为可用功能包装。
- 禁止把内部 1S-4S `CSEL/VCELL_SET` 当作本项目 6S 目标电压来源。
- 禁止忽略 reserved bit；后续写寄存器必须读改写保留位。

## 8. 任务卡

任务卡编号：`SC8815_BSP_HEADER_001`

允许读取：

- 本 `pre` 文档。
- `docs/agent_reports/sc8815_bsp_register_facts_2026-06-22.md`
- `docs/rules/hardware_rules.md`

允许写入：

- `Int/Int_SC8815_BSP.h`

禁止修改：

- `Int/Int_BQ76952*`
- `README.md`
- `docs/rules/*`
- `docs/state/*`
- 任何 `.c` 文件

验收标准：

- 只 include `<stdint.h>`。
- 每个寄存器和关键 bit 有中文注释。
- OTG/反向输出相关 bit 明确标为本项目禁止。
- 文件中不出现 HAL、FreeRTOS、GPIO 函数。

## 9. 继续生成补强任务

任务卡编号：`SC8815_BSP_HEADER_002`

用户要求：继续生成 `SC8815` 的 `BSP.h`。

目标仍然限制在 `Int/Int_SC8815_BSP.h`：

- 补充手册明确给出的字段取值枚举，例如 `IRCOMP`、`CSEL`、`VCELL_SET`、`RATIO`、`FREQ_SET`、`DT_SET`、`TRICKLE_SET`、`SLEW_SET`、`EOC_SET`。
- 补充硬件安全电平常量：`#CE` 低有效、`PSTOP` 高 standby。
- 补充 VBUSREF、限流 DAC、VINREG、ADC 的公式常量。
- 补充 `MASK.bit0` 上电后应写 1 的手册事实。
- 推荐配置值尽量由字段宏组合，减少手填十六进制。

继续禁止：

- 不写 `Int_SC8815.c/.h` 通信层。
- 不写软件 IIC。
- 不创建 public API。
- 不把 OTG/反向输出封装为可用功能。
