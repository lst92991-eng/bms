# Int_SC8815.c/.h Review

记录日期：2026-06-22

## 1. Review 范围

- `Int/Int_SC8815.h`
- `Int/Int_SC8815.c`
- `docs/pre/int_sc8815_comm_pre.md`
- `Int/Int_SC8815_BSP.h`
- `docs/rules/hardware_rules.md`
- `docs/rules/parallel_work_mechanism.md`

## 2. Blocking 问题

无当前 blocking 问题。

## 3. 已实现内容

- 创建 `Int_SC8815.h/.c`。
- `.h` 不包含 HAL 头，只暴露状态码、状态结构、ADC/限流枚举和 12 个 public API。
- `.c` 使用 `main.h` 的 HAL GPIO 操作 PA7/PA6/PB1/PB0，未使用 HAL I2C。
- 软件 IIC 底层 GPIO 原语保持为六个：
  - `Int_SC8815_IicDelay`
  - `Int_SC8815_IicSclHigh`
  - `Int_SC8815_IicSclLow`
  - `Int_SC8815_IicSdaHigh`
  - `Int_SC8815_IicSdaLow`
  - `Int_SC8815_IicSdaRead`
- `InitSafe` 只设置安全态：`PSTOP=standby`、`#CE=disable`，不启动充电。
- 所有 public 写寄存器路径经过 `Int_SC8815_GuardWrite`。

## 4. Guard 检查

已拦截：

- 只读 ADC/status 寄存器写入。
- 保留寄存器 `0x18/0x1A/0x1B` 写入。
- `VBUSREF_*` 寄存器写入，避免反向输出配置入口。
- `VBAT_SET.VBAT_SEL=0`。
- 低于 `300mA` 的限流值。
- `RATIO` 偏离项目要求 `IBUS_RATIO=3x`、`IBAT_RATIO=6x`。
- `RATIO` 中 VBUS/VBAT 电压监测比例偏离项目默认 `12.5x`，避免 ADC 电压换算失真。
- `CTRL0` 中 `EN_OTG=1`。
- `CTRL1` 中 `DIS_TRICKLE/DIS_TERM/FB_SEL/DIS_OVP`。
- `CTRL2` 中未置位手册要求的 `FACTORY` 位。
- `CTRL3` 中 `DIS_ShortFoldBack/EN_PFM`。
- `MASK.bit0` 未按手册置 1。
- 需要 standby 修改的关键字段在非 standby 状态被改写。

## 5. 静态检查

- 未发现 `HAL_I2C`。
- 未发现 `FreeRTOS`、`vTask`、`osDelay`。
- 未发现 `printf`。
- 未发现动态内存。
- public API 数量为 12，与 pre 一致。
- 未修改 `Int/Int_SC8815_BSP.h`、`Int/Int_BQ76952*`、`docs/rules/*`。

## 6. 后续局部可读性复核

`SC8815_READABILITY_001` 已完成局部可读性补丁：

- `Int_SC8815_WriteReg()` 保持 `ReadRegRaw -> GuardWrite -> WriteRegRaw`，不再重复做 `GuardWrite()` 已覆盖的只读、保留和 `VBUSREF_*` 预检查。
- `Int_SC8815_ReadAdcCurrentMa()` 先按 IBUS/IBAT 选择 `high_reg/low_reg`，再统一读取 high/low，换算公式和错误返回路径未改变。
- 只读复核未发现 public API 膨胀、guard 放宽、软件 IIC 底层原语新增、禁用库/接口引入。

## 7. 剩余风险

- 当前仓库没有新 CubeMX `.ioc`，GPIO 宏名按 PA7/PA6/PB1/PB0 默认适配；后续若 CubeMX 生成宏不同，需要用 `INT_SC8815_*` 宏重定向。
- 当前没有 `gcc`、`clang`、`cl`，未做编译器语法检查。
- 软件 IIC 时序为简易延时，bring-up 时需用示波器确认 SCL/SDA 波形和 ACK。
- SC8815 `FB/ADIN` 具体外部连接仍需后续结合原理图复核。
- 本层不负责启动充电业务；上层启用前必须先确认 R17/R18 实物已经改为 `100kΩ/5.1kΩ`。

## 8. 结论

`Int_SC8815.c/.h` 可以作为 SC8815 INT 层首版通信入口，用于后续 bring-up 读取状态、ADC 和安全限流配置。当前不进入 COM/APP。
