# Int_SC8815.c/.h 通信层预设计

记录日期：2026-06-22

更新日期：2026-06-23

本版按最新边界收敛：业务逻辑在 APP，组合逻辑在 COM，INT 只保留“直接操作 SC8815 硬件能力”的接口。SC8815 的充电管理由芯片硬件自动完成，软件只负责安全态、使能/待机、状态读取、ADC 监测和限流边界，不写微观充电状态机。

2026-06-23 用户已与硬件确认电池侧充电电流 `5A` 可以；为保险，软件初期运行先按 `3A`。该结论只更新电池侧 `IBAT` 目标和上限，不改变输入侧 `IBUS=3A` 上限。

## 1. 本轮目标

生成 SC8815 INT 通信层首版：

- `Int/Int_SC8815.h`
- `Int/Int_SC8815.c`

本轮只实现 PA6/PA7 GPIO 软件 IIC、SC8815 私有寄存器访问、写寄存器 guard、无业务策略的 ADC/状态/限流硬件操作函数。不实现 COM/APP，不实现充电业务流程，不实现 FreeRTOS task。原始寄存器读写不作为正式最小 public API 暴露。

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
- 输入采样 `R5=10mΩ`、`IBUS_RATIO=3`，输入侧寄存器量程约 `3A`，项目输入侧软件上限保持 `3000mA`。
- 电池采样 `R14=10mΩ`、`IBAT_RATIO=6`，电池侧寄存器量程约 `6A`，项目电池侧目标 `5000mA` 在量程内。
- 电池侧充电限流策略：bring-up 仍从 `300mA` 或 `500mA` 开始；初期运行默认 `3000mA`；最终软件上限 `5000mA`。

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

### 5.1 正式最小 public API

```c
Int_SC8815_StatusTypeDef Int_SC8815_InitSafe(void);
Int_SC8815_StatusTypeDef Int_SC8815_SetChipEnabled(bool enabled);
Int_SC8815_StatusTypeDef Int_SC8815_SetStandby(bool standby);
Int_SC8815_StatusTypeDef Int_SC8815_ReadStatus(Int_SC8815_StatusFlagsTypeDef *status);
Int_SC8815_StatusTypeDef Int_SC8815_SetAdcEnabled(bool enabled);
Int_SC8815_StatusTypeDef Int_SC8815_ReadAdcRaw(Int_SC8815_AdcChannelTypeDef channel, uint16_t *raw);
Int_SC8815_StatusTypeDef Int_SC8815_ReadAdcVoltageMv(Int_SC8815_AdcChannelTypeDef channel, uint32_t *mv);
Int_SC8815_StatusTypeDef Int_SC8815_ReadAdcCurrentMa(Int_SC8815_CurrentChannelTypeDef channel, uint32_t *ma);
Int_SC8815_StatusTypeDef Int_SC8815_SetCurrentLimitMa(Int_SC8815_CurrentLimitTypeDef type, uint16_t current_ma);
```

保留理由：

- `InitSafe`：建立硬件安全态，确保 `PSTOP=high`、`#CE=high`，不启动充电。
- `SetChipEnabled`：只控制 `#CE` 这个硬件使能脚，不决定充电策略。
- `SetStandby`：只控制 `PSTOP` 这个硬件待机脚，用于配置前后的硬件状态切换。
- `ReadStatus`：读取芯片状态位，供 COM/APP 判断；INT 不解释成业务动作。
- `SetAdcEnabled`、`ReadAdcRaw`：直接控制/读取芯片 ADC 能力。
- `ReadAdcVoltageMv`、`ReadAdcCurrentMa`：保留在 INT，因为换算依赖本板硬件比例、电流采样电阻和 BSP 常量，仍属于硬件适配，不是业务策略。
- `SetCurrentLimitMa`：设置硬件限流阈值，必须经过 guard；具体限流值策略由上层决定。

### 5.2 bring-up/debug 可选 API

默认不启用，仅在 `INT_SC8815_ENABLE_BRINGUP_API` 打开时允许出现：

```c
Int_SC8815_StatusTypeDef Int_SC8815_ReadReg(uint8_t reg, uint8_t *value);
Int_SC8815_StatusTypeDef Int_SC8815_WriteReg(uint8_t reg, uint8_t value);
Int_SC8815_StatusTypeDef Int_SC8815_UpdateReg(uint8_t reg, uint8_t clear_mask, uint8_t set_mask);
```

限制：

- `ReadReg` 可用于 bring-up 查看寄存器，但正式业务不应直接依赖寄存器地址。
- `WriteReg`/`UpdateReg` 即使处于 debug 宏下，也必须经过 guard。
- debug 宏默认关闭，正式构建不暴露这三个接口。

不作为正式 public API 的原因：

- 原始寄存器写接口会让上层绕过“SC 由硬件自动充电、软件只做安全边界”的设计意图。
- 即使有 guard，`WriteReg/UpdateReg` 仍会鼓励 APP 直接编码寄存器位，导致业务策略下沉到 INT/寄存器层。
- 正式接口应按硬件操作意图命名，例如使能、待机、状态、ADC、限流，而不是按寄存器读写命名。

### 5.3 明确不提供的接口

不提供 OTG、反向输出、FB 设置、关闭保护、关闭涓流、关闭自动终止、微观充电状态机等 API。

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

所有写寄存器路径必须经过 guard，包括私有 `WriteRegRaw`、私有寄存器更新 helper、debug 条件下的 `WriteReg/UpdateReg` 和正式 `SetCurrentLimitMa`。

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
- 输入侧限流高于 `3000mA`。
- 电池侧充电限流高于 `5000mA`。
- `RATIO` 中 `IBUS_RATIO` 为 `00/11`，或偏离本项目 `IBUS_RATIO=3x`、`IBAT_RATIO=6x`。
- `VBAT_SET` 中 `VBAT_SEL` 被清零。
- 需要 PSTOP 高电平配置的关键字段，在当前记录状态不是 standby 时写入。

## 8. 禁止事项

- 禁止使用 HAL I2C。
- 禁止使用 FreeRTOS、printf、动态内存。
- 禁止写 COM/APP 业务逻辑。
- 禁止启动充电流程；`InitSafe` 只进入安全态。
- 禁止在 INT 内根据状态位推进充电阶段。
- 禁止在本轮处理中断回调。
- 禁止把 `VBUSREF_*` 作为可用输出电压配置。
- 禁止在正式 public API 暴露 `ReadReg`、`WriteReg`、`UpdateReg`。

## 9. 任务卡

任务卡编号：`SC8815_COMM_MIN_API_002`

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
- 正式 public API 不暴露 `ReadReg`、`WriteReg`、`UpdateReg`。
- `ReadReg`、`WriteReg`、`UpdateReg` 若保留，必须受 `INT_SC8815_ENABLE_BRINGUP_API` 保护且默认关闭。
- 所有写寄存器路径经过 guard。
- 不提供 OTG/反向输出 API。
