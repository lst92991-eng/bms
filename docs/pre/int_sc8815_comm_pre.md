# Int_SC8815.c/.h 通信层预设计

记录日期：2026-06-22

## 1. 本轮目标

生成 SC8815 INT 通信层首版：

- `Int/Int_SC8815.h`
- `Int/Int_SC8815.c`

本轮只实现 PA6/PA7 GPIO 软件 IIC、SC8815 寄存器读写、写寄存器 guard、无业务策略的 ADC/状态/限流辅助函数。不实现 COM/APP，不实现充电业务流程，不实现 FreeRTOS task。

## 2. 输入文件

- `Int/Int_SC8815_BSP.h`
- `docs/pre/int_sc8815_bsp_pre.md`
- `docs/review/int_sc8815_bsp_review.md`
- `docs/agent_reports/sc8815_bsp_register_facts_2026-06-22.md`
- `docs/rules/hardware_rules.md`
- `README.md`

## 3. 硬件边界

- SC8815 固定 7-bit I2C 地址 `0x74`。
- 硬件 IIC3 接反不可用，本模块禁止使用 HAL I2C。
- 软件 IIC 引脚：`PA7=SCL`、`PA6=SDA`。
- 软件 IIC 依赖外部上拉，GPIO 应配置为开漏输出；写高表示释放总线。
- `#CE=PB1`，低有效；安全态为高电平 disable。
- `PSTOP=PB0`，高电平 standby；配置关键寄存器前应保持 standby。
- ISR 不在本轮实现；后续 ISR 只允许置标志，不允许访问 IIC。

## 4. 软件 IIC 六个底层方法

本轮软件 IIC 的底层 GPIO 原语严格限制为六个 `static` 方法：

```c
static void Int_SC8815_IicDelay(void);
static void Int_SC8815_IicSclHigh(void);
static void Int_SC8815_IicSclLow(void);
static void Int_SC8815_IicSdaHigh(void);
static void Int_SC8815_IicSdaLow(void);
static bool Int_SC8815_IicSdaRead(void);
```

允许在这六个底层方法之上实现私有协议 helper，例如 start/stop/write byte/read byte。禁止新增第七个底层 GPIO/IIC 原语。

## 5. Public API

首版 public API 控制在 12 个：

```c
Int_SC8815_StatusTypeDef Int_SC8815_InitSafe(void);
Int_SC8815_StatusTypeDef Int_SC8815_SetChipEnabled(bool enabled);
Int_SC8815_StatusTypeDef Int_SC8815_SetStandby(bool standby);
Int_SC8815_StatusTypeDef Int_SC8815_ReadReg(uint8_t reg, uint8_t *value);
Int_SC8815_StatusTypeDef Int_SC8815_WriteReg(uint8_t reg, uint8_t value);
Int_SC8815_StatusTypeDef Int_SC8815_UpdateReg(uint8_t reg, uint8_t clear_mask, uint8_t set_mask);
Int_SC8815_StatusTypeDef Int_SC8815_ReadStatus(Int_SC8815_StatusFlagsTypeDef *status);
Int_SC8815_StatusTypeDef Int_SC8815_SetAdcEnabled(bool enabled);
Int_SC8815_StatusTypeDef Int_SC8815_ReadAdcRaw(Int_SC8815_AdcChannelTypeDef channel, uint16_t *raw);
Int_SC8815_StatusTypeDef Int_SC8815_ReadAdcVoltageMv(Int_SC8815_AdcChannelTypeDef channel, uint32_t *mv);
Int_SC8815_StatusTypeDef Int_SC8815_ReadAdcCurrentMa(Int_SC8815_CurrentChannelTypeDef channel, uint32_t *ma);
Int_SC8815_StatusTypeDef Int_SC8815_SetCurrentLimitMa(Int_SC8815_CurrentLimitTypeDef type, uint16_t current_ma);
```

不提供 OTG、反向输出、FB 设置、关闭保护、关闭涓流、关闭自动终止等 API。

## 6. 错误码

- `INT_SC8815_OK`
- `INT_SC8815_ERROR`
- `INT_SC8815_ERROR_PARAM`
- `INT_SC8815_ERROR_ACK`
- `INT_SC8815_ERROR_GUARD`
- `INT_SC8815_ERROR_TIMEOUT`
- `INT_SC8815_ERROR_STATE`
- `INT_SC8815_ERROR_RANGE`

## 7. 写寄存器 guard

所有写寄存器路径必须经过 guard，包括 `WriteReg`、`UpdateReg` 和限流设置函数。

必须拦截：

- 写只读 ADC/status 寄存器。
- 写保留寄存器 `0x18/0x1A/0x1B`。
- 写 VBUSREF 相关寄存器 `0x01-0x04`，因为本项目禁止 OTG/反向输出。
- `CTRL0.EN_OTG=1`。
- `CTRL1.DIS_TRICKLE=1`。
- `CTRL1.DIS_TERM=1`。
- `CTRL1.FB_SEL=1`。
- `CTRL1.DIS_OVP=1`。
- `CTRL3.DIS_ShortFoldBack=1`。
- `CTRL3.EN_PFM=1`。
- 电流限流低于 `300mA`。
- `RATIO` 中 `IBUS_RATIO` 为 `00/11`，或偏离本项目 `IBUS_RATIO=3x`、`IBAT_RATIO=6x`。
- `VBAT_SET` 中 `VBAT_SEL` 被清零。
- 需要 PSTOP 高电平配置的关键字段，在当前记录状态不是 standby 时写入。

## 8. 禁止事项

- 禁止使用 HAL I2C。
- 禁止使用 FreeRTOS、printf、动态内存。
- 禁止写 COM/APP 业务逻辑。
- 禁止启动充电流程；`InitSafe` 只进入安全态。
- 禁止在本轮处理中断回调。
- 禁止把 `VBUSREF_*` 作为可用输出电压配置。

## 9. 任务卡

任务卡编号：`SC8815_COMM_001`

允许读取：

- 本 `pre` 文档。
- `Int/Int_SC8815_BSP.h`
- `docs/rules/hardware_rules.md`
- `docs/review/int_sc8815_bsp_review.md`

允许写入：

- `Int/Int_SC8815.h`
- `Int/Int_SC8815.c`
- `docs/review/int_sc8815_comm_review.md`
- `docs/state/main_thread_checkpoint.md`

禁止修改：

- `Int/Int_SC8815_BSP.h`
- `Int/Int_BQ76952*`
- `README.md`
- `docs/rules/*`
- 旧项目目录
- COM/APP 文件

验收标准：

- `.h` 不包含 HAL 头。
- `.c` 不包含 HAL I2C、FreeRTOS、printf。
- 软件 IIC 底层 GPIO 原语正好六个。
- 所有 public API 返回状态。
- 所有写寄存器路径经过 guard。
- 不提供 OTG/反向输出 API。
