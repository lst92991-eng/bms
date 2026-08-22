# BMS V2 统一编码与注释规范

- 版本：1.0
- 语言：C11
- 适用范围：`firmware/`、`tests/`、自研构建脚本
- 不适用范围：CubeMX/HAL/CMSIS/vendor 生成代码；这些目录保持原始风格且禁止无必要修改

## 1. 基本原则

1. 正确性、安全性、可验证性优先于代码短小。
2. 安全路径必须确定、有界、无动态分配、无阻塞日志。
3. 模块边界通过头文件和单一所有权表达，不通过可变全局变量连接。
4. 注释解释“为什么、硬件约束、失效动作和时序”，不逐句翻译代码。
5. 每个物理量、协议字段和超时必须可看出单位。
6. 任何例外都必须在代码旁说明原因，并在 review 中显式批准。

## 2. 文件与目录命名

- 文件名：小写 snake_case，例如 `bms_safety.c`、`bq76952_driver.h`。
- 每个业务模块原则上使用一对 `.c/.h`。
- 私有实现不建立无必要头文件。
- 目录按职责命名：`domain`、`services`、`drivers`、`platform`、`protocol`、`config`、`common`。
- 禁止 `misc.c`、`utils.c`、`common2.c`、`new.c` 等无明确职责名称。

## 3. 符号命名

### 3.1 公共函数

格式：`Module_VerbNoun`

```c
BmsStatus BmsSafety_Init(const BmsSafety_Config *config);
BmsStatus Bq76952_ReadDirect(Bq76952_Context *context,
                             uint8_t command,
                             uint8_t *data,
                             size_t data_size);
```

规则：

- 模块前缀必须唯一；
- 动词准确表达动作：`Init`、`Start`、`Stop`、`Read`、`Write`、`Update`、`Evaluate`、`Request`、`Get`；
- `Get` 不产生外设副作用；
- `Request` 表示异步提交；
- 同步执行器动作必须在名称中体现，例如 `StopChargeImmediate`。

### 3.2 私有函数

私有函数使用小写 snake_case，并声明为 `static`：

```c
static bool snapshot_is_fresh(const BmsSnapshot *snapshot,
                              uint32_t now_ms);
```

### 3.3 类型

类型格式：`Module_Name`。

```c
typedef enum
{
    BMS_FAULT_SEVERITY_INFO = 0,
    BMS_FAULT_SEVERITY_WARNING,
    BMS_FAULT_SEVERITY_TRIP,
    BMS_FAULT_SEVERITY_LATCHED_TRIP,
    BMS_FAULT_SEVERITY_LOCKOUT
} BmsFault_Severity;
```

- 不为基本整数随意创建无语义别名；
- 序列化结构禁止依赖编译器 padding；
- 不把 C struct 直接写入 EEPROM 或 CAN；
- enum 写入协议前显式转换并验证范围。

### 3.4 常量和宏

```c
#define BMS_SAFETY_TASK_PERIOD_MS        (10u)
#define BMS_CELL_COUNT                   (6u)
```

- 全大写模块前缀；
- 数字常量使用括号和正确后缀；
- 函数式宏原则上禁止，优先 `static inline`；
- 宏参数必须加括号；
- 禁止宏隐藏控制流、return、锁或硬件写操作。

### 3.5 变量

- 小写 snake_case；
- 布尔变量使用 `is_`、`has_`、`can_`、`should_`、`*_enabled`；
- 物理量带单位后缀：
  - `_mv`、`_uv`；
  - `_ma`、`_ua`；
  - `_mw`；
  - `_ms`、`_us`；
  - `_deg_c`、`_deci_c`；
  - `_mah`；
  - `_percent`、`_permille`；
- 原始寄存器值使用 `_raw`；
- bit mask 使用 `_mask`；
- 时间戳使用 `_timestamp_ms`；
- 计数器使用 `_count`；
- 禁止 `data1`、`tmp2`、`flag3` 等无意义名称。

## 4. 排版

- UTF-8，无 BOM；
- LF 换行；
- 4 空格缩进，不使用 Tab；
- Allman 大括号；
- 建议行宽 100，硬上限 120；
- 一个声明一行；
- 指针星号靠变量：`const BmsSnapshot *snapshot`；
- 二元运算符两侧留空格；
- `if`、`for`、`while` 即使单行也必须使用大括号；
- 不在一行写多个语句；
- 禁止 comma operator；
- `switch` 必须有 `default`，即使 default 只记录不可达状态。

## 5. Include 规则

顺序：

1. 当前模块自己的头文件；
2. C 标准库；
3. RTOS；
4. domain/service/driver；
5. platform；
6. vendor/HAL，仅允许出现在 platform 或指定 adapter 中。

```c
#include "bms_safety.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"

#include "bms_fault.h"
#include "actuator_service.h"
```

规则：

- 头文件自包含；
- 使用 include guard；
- 禁止依赖其他头文件的间接 include；
- domain 头文件不得包含 `stm32*.h`、HAL handle 或 FreeRTOS handle；
- 公共头文件只暴露必要内容。

## 6. 模块头注释

每个自研 `.c` 文件使用：

```c
/**
 * @file bms_safety.c
 * @brief BMS 安全监督器：根据完整快照计算故障和功率路径许可。
 *
 * @ownership BmsSafetyTask 单任务调用；本模块不访问 HAL。
 * @safety 本模块失败时调用方必须维持 PSTOP=1，且不得释放主功率路径。
 * @timing BmsSafety_Evaluate() 必须在 10 ms 周期预算内完成。
 */
```

头文件使用：

```c
/**
 * @file bms_safety.h
 * @brief BMS 安全监督器公共接口。
 */
```

禁止放入冗长变更历史；历史由 Git 保存。

## 7. 公共 API 注释

安全相关公共 API 必须包含：

```c
/**
 * @brief 同步断言 SC8815 停充安全门。
 *
 * @param[in,out] actuator 执行器上下文。
 * @return BMS_STATUS_OK 表示 GPIO 已写入安全电平；其他值表示平台异常。
 *
 * @pre actuator 已完成最小 GPIO 初始化。
 * @post PSTOP 被请求为高电平；函数不释放任何功率路径。
 * @concurrency 允许 Safety Task 调用；不得从普通业务模块调用。
 * @timeout 不访问总线，不等待队列，执行时间必须有界。
 * @safety 即使软件状态未知，也必须优先执行硬件安全写入。
 */
BmsStatus Actuator_StopChargeImmediate(Actuator_Context *actuator);
```

普通 getter 或明显的内部 helper 不强制写长注释。

## 8. 注释内容规则

应该注释：

- FET/PSTOP/CE_N 的安全顺序；
- 芯片手册要求的时序；
- endian、CRC 和寄存器窗口；
- 为什么必须锁存或回差；
- 数据无效时的降级策略；
- 并发所有权和临界区；
- 标定依据和单位换算；
- 有界循环的最大次数和时间；
- 看似多余但用于安全的重复校验。

不应该注释：

```c
count++; /* count 加一 */
```

禁止：

- 与代码不一致的旧注释；
- “临时”“后面再改”而没有 issue/需求 ID；
- 用注释掉的代码保存历史；
- 含糊的“防止异常”“优化性能”而没有说明具体异常或预算。

TODO 格式：

```c
/* TODO(BMS-REQ-042): 完成 TS3 开路故障的 HIL 验证后冻结恢复时间。 */
```

没有可追踪 ID 的 TODO 不允许进入发布分支。

## 9. 函数设计

- 单一职责；
- 优先早返回处理参数和错误；
- 正常路径保持清晰；
- 建议不超过 60 个有效代码行；安全状态机可例外，但必须拆分条件计算和动作执行；
- 参数原则上不超过 5 个，更多参数使用 config/input struct；
- 不通过隐藏全局状态传递输入；
- 所有 out 参数只在成功时提交完整值；
- 失败时不得返回半更新快照；
- 无界重试禁止；
- 不在 driver 中调用业务策略；
- 不在 domain 中访问 RTOS/HAL。

## 10. 错误处理

统一状态码：

```c
typedef enum
{
    BMS_STATUS_OK = 0,
    BMS_STATUS_INVALID_ARGUMENT,
    BMS_STATUS_NOT_INITIALIZED,
    BMS_STATUS_BUSY,
    BMS_STATUS_TIMEOUT,
    BMS_STATUS_IO_ERROR,
    BMS_STATUS_PROTOCOL_ERROR,
    BMS_STATUS_CRC_ERROR,
    BMS_STATUS_RANGE_ERROR,
    BMS_STATUS_STATE_ERROR,
    BMS_STATUS_CONFIG_MISMATCH,
    BMS_STATUS_INTERNAL_ERROR
} BmsStatus;
```

规则：

- 不用 `0/-1` 表达所有错误；
- driver 保留可查询的底层错误快照；
- service 将底层错误映射为系统 fault；
- 安全动作失败不得静默忽略；
- 非关键遥测失败可降级，但必须计数；
- 使用 `assert` 检查开发期程序不变量，不用 assert 替代运行期输入检查；
- 生产 assert 进入 fail-safe 和看门狗复位流程。

## 11. 整数、浮点和单位

- 安全阈值使用定点整数；
- 电压 mV、电流 mA、时间 ms 为默认业务单位；
- 乘法前提升到足够宽类型；
- 所有可能溢出的累计使用饱和函数；
- 有符号/无符号比较显式转换；
- 解析协议数据前检查长度；
- 浮点仅用于 SOC/SOH/模型，不直接决定快速保护动作；
- 任何单位转换封装为命名函数；
- 禁止不带单位的 `threshold`、`timeout`。

## 12. 并发规则

- 一个可变资源只有一个 owner；
- 跨任务传递使用静态队列、通知、事件组或不可变快照；
- `volatile` 只表达硬件寄存器或 ISR 可见性，不提供互斥和内存事务语义；
- 临界区只保护短小内存操作；
- 临界区内禁止 HAL、printf、循环等待和队列阻塞；
- ISR 不访问 I2C、EEPROM、printf、状态机或算法；
- ISR 只清硬件源、保存必要原始状态、通知任务；
- 间接寄存器窗口的锁覆盖完整事务，不只锁单次 HAL 调用；
- 所有锁都必须有固定获取顺序；
- 安全路径不等待低优先级任务持有的 mutex。

## 13. FreeRTOS 规则

- 使用 `xTaskCreateStatic`、`xQueueCreateStatic`、静态 event group；
- 调度器启动后禁止动态创建长期对象；
- 周期任务使用 `vTaskDelayUntil`；
- 将实际 elapsed time 与期望周期分开记录；
- 每个任务报告 heartbeat、deadline miss 和 minimum stack watermark；
- 任务入口不返回；
- task notification 用于一对一轻量事件；
- queue 用于需要复制 payload 的命令；
- queue 满必须有明确策略；安全命令不得被普通命令挤掉；
- 开启栈溢出检查、malloc failure hook 和 assert；
- Safety Supervisor 是唯一看门狗许可者。

## 14. Driver 规则

- driver 不打印；
- driver 不知道业务模式；
- 所有 IO 有超时；
- 公开函数检查指针、长度、地址和状态；
- 写寄存器前保护保留位；
- 读改写必须明确原子范围；
- 总线恢复是显式 API，不在每次 ACK 失败后隐式改变产品配置；
- 驱动不无限自动重试，重试次数由 service 策略控制；
- 原始寄存器定义与业务配置分离；
- 使用 `_Static_assert` 验证数组大小、协议长度和 mask。

## 15. 状态机规则

- 状态、事件、guard、action 分离；
- 所有状态都有非法事件处理；
- 每个转换记录原因；
- 进入动作和退出动作显式；
- 有超时的状态保存 entry timestamp；
- 不能通过多个布尔变量隐式拼出不可解释状态；
- 安全输出由完整目标向量一次计算；
- 应用目标向量前后都检查条件；
- 动作失败进入明确故障状态。

## 16. 协议与持久化

- 不发送/保存裸 struct；
- 固定宽度整数；
- 显式 little-endian/big-endian helper；
- 每种记录包含 magic、version、length、sequence、CRC；
- 解码先验证长度和版本，再访问字段；
- 兼容旧版本使用独立迁移函数；
- 不恢复未完成的安全关键事务；
- CAN 无效值必须由 valid flag 表达。

## 17. 生产与工程构建

`bms_build_config.h` 至少定义：

```c
#define BMS_BUILD_PRODUCTION            (1)
#define BMS_BUILD_ENGINEERING           (0)
```

规则：

- 二者必须互斥；
- production 默认关闭 DEBUG/TRACE 和危险维护命令；
- engineering 不得降低 BQ 硬保护；
- 工程维护操作仍必须通过 supervisor；
- 构建类型、Git SHA、配置版本可通过只读诊断查询。

## 18. 测试代码规则

- 测试名称使用 `test_<module>_<condition>_<expected>`；
- Arrange/Act/Assert 清晰分段；
- 测试不依赖执行顺序；
- 每个 fault 至少测试触发、去抖、动作、恢复和锁存；
- 每个状态至少测试合法和非法事件；
- 使用 fake driver 注入 timeout/NACK/CRC/陈旧数据；
- 边界包括 0、最大值、时间回绕、整数溢出、队列满；
- 修复缺陷前先增加可复现测试；
- HIL 脚本输出机器可读结果和固件版本。

## 19. Review 必查项

- 是否修改了生成目录；
- 是否增加新的执行器入口；
- 是否破坏单一所有者；
- 是否存在未界定阻塞；
- 是否在安全任务中打印；
- 是否使用无有效性的测量；
- 是否有单位和范围错误；
- 是否对配置写入做回读；
- 是否存在中间态功率风险；
- 是否有对应需求 ID 和测试；
- 是否增加动态内存；
- 是否更新文档和追溯矩阵。

## 20. 自动化执行

仓库将使用：

- `.clang-format` 固化格式；
- `.clang-tidy` 固化静态规则；
- GCC `-Wall -Wextra -Werror -Wconversion -Wshadow -Wdouble-promotion`，对 vendor 代码单独降噪；
- `cppcheck`；
- CMake/CTest host tests；
- 脚本检查生成目录、禁止 API、文件头、TODO 和可变全局；
- PR CI 阻止不符合规范的提交进入 main。
