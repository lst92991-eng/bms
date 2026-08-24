# BMS Firmware Coding Standard

更新日期：2026-08-22

## 1. 适用范围

本规范适用于仓库自研的 `App/`、`Com/`、`Int/`、主机测试和 CubeMX `USER CODE`
区域。STM32 HAL、CMSIS、FreeRTOS 内核、启动文件和其他供应商源码保持原样，不做机械格式化。

本规范以仓库既有的 Allman 大括号、4 空格缩进、定宽整数和 `App/Com/Int` 分层为基线；
`.clang-format` 只负责可机械判定的排版，安全语义和注释质量仍由评审检查。

## 2. 模块边界

| 前缀 | 职责 | 可以依赖 | 禁止事项 |
| --- | --- | --- | --- |
| `Int_` | 引脚、总线、芯片寄存器和硬件执行器 | HAL/CMSIS、对应 BSP | 产品状态机、策略阈值、CLI |
| `Com_` | 无硬件副作用的算法、策略、编码和校验 | 标准 C、其他纯 `Com_` | HAL、RTOS、全局硬件句柄 |
| `App_` | 产品状态机、任务所有权和模块编排 | `Com_`、公开 `Int_` API | 直接访问 HAL 寄存器（安全原语除外） |

- `main.c` 只负责 CubeMX 初始化和进入 `App_Main()`。
- 每个运行期总线或状态机必须有单一 owner；跨任务调用只提交固定长度请求或读取原子快照。
- 禁止在 generated/vendor 文件中放业务代码；必要中断连接只能写在 CubeMX `USER CODE` 区域。

## 3. 命名

- 文件与公开函数：`App_Feature_*`、`Com_Domain_*`、`Int_Device_*`。
- 公开类型：`Module_NameTypeDef`；枚举值带完整模块前缀。
- 文件内状态：`s_`；只读编译期常量优先使用模块内 `enum`，跨编译单元常量放公开头文件。
- 布尔值使用肯定语义：`is_valid`、`allow_charge`、`trip_latched`；避免含义模糊的
  `ok`、`flag`、`state1`。
- 单位写入名称：`timeout_ms`、`current_ma`、`voltage_mv`、`temperature_c`、`elapsed_ticks`。
- 位掩码以 `_MASK` 结尾，固定移位以 `_SHIFT` 结尾，寄存器地址以 `_REG` 或 `_DM` 标识。

## 4. 类型、边界和算术

- 跨硬件/协议边界使用 `uint8_t/uint16_t/uint32_t/int32_t`，禁止依赖 `int` 宽度。
- 所有长度、索引、DLC、页地址和数组边界先验证再访问。
- 单位转换先提升到足够宽的中间类型，明确饱和、舍入和溢出行为。
- 线协议不得通过结构体强制转换或位域布局序列化；显式按字节编码并注明大小端。
- 枚举只表示有限状态；外部输入转枚举前必须校验范围。
- 浮点只用于 SOC/SOH 等非硬实时估算；ISR、总线原语和紧急停机不得使用浮点或格式化。

## 5. 错误处理

- 影响安全的返回值不得丢弃；错误码至少区分参数、状态、NACK、timeout、bus stuck、
  CRC、checksum、长度、deadline、配置 mismatch 和执行器 mismatch。
- 一次多步事务共享一个绝对 deadline；不能让每一步重新获得完整 timeout。
- 首个 transport/protocol 错误立即终止当前帧或事务；半帧不得发布为有效数据。
- 未知状态按不可信处理。不得把通信失败解释为低电、shutdown 或其他可释放功率的正常状态。
- 锁存故障必须定义清除权限和前置条件；禁止通用 `fault clear all`。

## 6. 并发与中断

- ISR 只做直接安全 GPIO、清硬件 pending、锁存事件或使用 `FromISR` API；禁止 I2C、
  EEPROM、printf、动态分配和等待。
- 临界区只覆盖不可分割的少量内存/GPIO操作；禁止用 PRIMASK 包围完整软件 I2C 事务。
- heartbeat 在工作单元成功或明确失败并完成安全收尾后更新，不得在任务入口更新。
- 周期任务使用绝对节拍；发生 overrun 时跳过已过期周期并计数，禁止 catch-up 忙跑。
- 每个共享快照包含 `sequence`、`captured_ms`、`valid_mask`、`age/stale` 和详细错误。

## 7. 安全执行器

- 任何停充路径第一动作直接将 PSTOP 置高；随后才更新状态、通知任务或记录日志。
- 正常释放执行器必须持有当次 supervisor authorization epoch；旧请求不能跨 trip 生效。
- 执行器状态分别记录 `desired`、`commanded`、`observed`；只有回读相符才算完成。
- BQ 选择性 FET 转移禁止先全开再补关；失败回到安全 inhibit。

## 8. 注释规范

注释解释“为什么、硬件约束、时序、单位、并发和安全后果”，不复述代码字面动作。

公开 API 使用 Doxygen：

```c
/**
 * @brief 同步关闭充电功率级。
 * @note 可在 ISR 调用；无总线访问、无等待，且重复调用安全。
 */
void Int_SC8815_ForceStandby(void);
```

寄存器和魔数注释必须包含来源、单位或换算，例如：

```c
/* 100 kHz 下 8-byte 短帧通常远小于 10 ms；最终上限以故障注入波形冻结。 */
INT_BQ76952_I2C_TIMEOUT_MS = 10u
```

禁止以下注释：

- `i++` 旁写“变量加一”；
- 无证据的“安全”“已验证”“最优”；
- 与实现不同步的历史说明；
- 被注释掉的大段旧实现。历史由 Git 保存。

## 9. 构建与仓库卫生

- 自研代码在 GCC/ARMCC 可用配置下执行 warning-as-error；Release 与 Engineering feature
  profile 必须显式区分。
- 新增纯算法或状态机必须有 host unit test；安全问题必须有失败点注入测试。
- 不提交 `.o/.obj/.elf/.axf/.hex/.bin/.map/.crf/.d/.dep/.lnp`、build、cache、日志、
  `node_modules` 或临时测试产物。
- 不把静态软件测试写成实板通过；波形、温箱、电流和故障注入保留板号、固件 SHA、
  仪器和原始证据。

## 10. 评审最小清单

1. 分层和单一 owner 是否清楚？
2. 输入长度、单位、溢出和 timeout 是否有界？
3. 每个失败点是否保持安全输出，且没有后续副作用？
4. ISR、临界区和任务优先级是否有最坏时间依据？
5. 配置是否 expected/actual 回读，执行器是否 desired/observed 回读？
6. Release 是否不存在危险 CLI、阻塞日志和临时测试入口？
7. 自动化测试和 HIL 门禁是否分别记录，Unknown 是否诚实保留？
