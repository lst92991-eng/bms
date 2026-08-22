# BMS V2 安全固件重构总计划

- 状态：Plan Baseline v1.0
- 基线：`main@42fff3c7f9fe5b3482e6850cd9009621eca48c8d`
- 工作分支：`refactor/bms-v2-safety-architecture`
- 目标平台：STM32G0B1 + BQ76952 + SC8815 + EEPROM + FDCAN + FreeRTOS
- 适用电池：当前 6S 锂离子电池包
- 核心约束：`bms24v_platform/Core`、`Drivers`、启动文件及其他 CubeMX/HAL 生成内容作为只读输入，不承载新增业务逻辑

## 1. 目标定义

本项目不是对现有代码做局部修补，而是建立一套可持续演进、可测试、可追溯、可证明失效安全的 BMS V2 固件。

目标包括：

1. 任何软件故障、通信异常、任务饥饿或未授权命令都不能让充电功率级绕过安全策略继续运行。
2. BQ76952 是独立硬件保护层，MCU 软件作为第二层监督和策略控制，不覆盖或削弱 BQ 的自主保护。
3. 充电、放电、预放电和 SC8815 功率级只有一个逻辑所有者；其他模块只能提交意图，不能直接操作执行器。
4. 所有芯片总线只有一个事务所有者，间接寄存器窗口不可被跨任务交叉访问。
5. 所有安全决策只使用带有效性、时间戳和来源的数据；无效、陈旧、缺失数据不能被正常数值替代。
6. 生产固件不保留能够直接启动功率级、强开 FET 或清除锁存故障的非授权调试入口。
7. 关键算法与状态机必须能脱离 MCU 在主机上自动测试。
8. 固件发布必须具有需求、设计、代码、测试和实板证据之间的双向追溯。

## 2. “100 分”的工程定义

“100 分”是本项目内部发布门禁，不代表获得 ISO 26262、IEC 61508、UL 1973 或其他第三方功能安全认证。

只有以下条件全部满足，版本才可以标记为工程 100/100：

- 已知 P0/P1 缺陷为 0；
- GCC Debug/Release 和 Keil 目标均无错误、无警告；
- 格式检查、静态分析和禁止模式扫描全部通过；
- 纯逻辑模块单元测试通过，关键状态转移和故障组合达到规定覆盖率；
- BQ76952 Data Memory 安全配置逐项回读一致；
- 任务截止期、最长关中断时间、栈余量、总线超时均有测量证据；
- 关键故障注入和断电恢复测试通过；
- CHG、DSG、PCHG、PDSG、PSTOP、CE_N 的全部关键转换具有示波器证据；
- 生产构建中不存在危险 CLI、动态分配依赖、阻塞式日志或未授权执行器入口；
- 发布包包含版本、配置指纹、构建信息、测试报告、实板记录和已知限制；
- 主分支仅通过审核后的 PR 合并，且 CI 状态全部成功。

若缺少实板波形、热测试或故障注入证据，代码即使全部完成，也只能标记为“软件完成”，不能标记为 100/100 发布。

## 3. 不可妥协的设计规则

### 3.1 Fail-safe 默认态

复位、初始化失败、调度器失败、任务创建失败、看门狗复位、配置不匹配和通信失联时，系统默认输出为：

- SC8815 `PSTOP = 1`；
- SC8815 不释放充电功率环路；
- BQ 主 FET 保持关闭或由 BQ 自主保护决定；
- 不发送“允许充电/允许放电”的对外状态；
- 保留故障原因并等待受控恢复。

### 3.2 单一所有者

- `BmsSafetyTask`：唯一安全策略所有者；计算允许充电、允许放电和系统模式。
- `BqServiceTask`：唯一 BQ76952 总线和间接窗口所有者。
- `ScServiceTask`：唯一 SC8815 寄存器和启动动作所有者。
- `BmsActuator`：唯一执行器门面。
- `CanTask`：唯一 FDCAN 协议所有者。
- `NvmTask`：唯一持久化写入所有者。
- `DiagTask`：唯一 UART 输出所有者。

例外只有“单向安全覆盖”：安全监督器可以直接、同步地把 `PSTOP` 置为安全电平；任何模块都不能绕过监督器释放 `PSTOP`。

### 3.3 停止优先、启动最后

停止顺序：

1. 同步断言 `PSTOP`；
2. 撤销 SC 充电请求；
3. 请求 BQ 关闭对应 FET；
4. 回读执行器状态；
5. 发布故障和停止原因。

启动顺序：

1. 验证硬件身份、配置指纹和测量有效性；
2. 验证故障、温度、电压、电流和通信条件；
3. 在 standby 中配置 SC8815；
4. 设置 BQ 目标 FET 并回读；
5. 再次检查安全条件未变化；
6. 最后释放 `PSTOP`。

禁止使用未验证的 `ALL_FETS_ON -> FET_CONTROL` 通用序列。每个 FET 转换必须具有明确前置条件、目标状态、超时、回读和实板波形证明。

### 3.4 安全逻辑不得依赖日志和低优先级任务

- 安全任务不调用 `printf`；
- 安全路径不等待 UART、CAN、OLED 或 EEPROM；
- 日志缓冲区满时丢弃日志并累计计数，不阻塞任务；
- NVM 故障不能阻止紧急停充或关断；
- CAN 故障不能改变本地保护结论。

### 3.5 生产固件零危险后门

生产构建中禁止：

- 直接 `charge on`；
- 直接 `pdsg test`；
- 直接写 BQ FET；
- 未授权 `fault clear`；
- 运行期自动交换 I2C 线序；
- 通过串口直接绕过状态机修改保护阈值。

工程构建中的维护操作也必须经过维护模式、物理授权、超时和 Safety Supervisor 仲裁。

## 4. 目标目录结构

```text
firmware/
├── app/
│   ├── bms_app.c/.h
│   ├── bms_tasks.c/.h
│   └── bms_startup.c/.h
├── domain/
│   ├── bms_types.h
│   ├── bms_snapshot.h
│   ├── bms_safety.c/.h
│   ├── bms_fault.c/.h
│   ├── bms_power_policy.c/.h
│   ├── bms_charge_policy.c/.h
│   ├── bms_balance_policy.c/.h
│   ├── bms_soc.c/.h
│   └── bms_soh.c/.h
├── services/
│   ├── bq_service.c/.h
│   ├── sc_service.c/.h
│   ├── measurement_service.c/.h
│   ├── actuator_service.c/.h
│   ├── nvm_service.c/.h
│   ├── watchdog_service.c/.h
│   ├── telemetry_service.c/.h
│   └── diagnostic_service.c/.h
├── drivers/
│   ├── bq76952/
│   ├── sc8815/
│   ├── eeprom/
│   ├── can_fd/
│   └── oled/
├── platform/
│   ├── platform_gpio.c/.h
│   ├── platform_i2c.c/.h
│   ├── platform_time.c/.h
│   ├── platform_watchdog.c/.h
│   ├── platform_uart.c/.h
│   └── platform_critical.c/.h
├── protocol/
│   └── bms_can_protocol.c/.h
├── config/
│   ├── bms_product_config.h
│   ├── bms_calibration.c/.h
│   ├── bq76952_profile.c/.h
│   └── bms_build_config.h
└── common/
    ├── bms_status.h
    ├── byte_order.h
    ├── crc16.c/.h
    ├── crc32.c/.h
    ├── saturating_math.h
    └── static_assert.h

tests/
├── unit/
├── integration/
├── model/
├── fakes/
└── hil/

docs/refactor/
├── BMS_V2_MASTER_PLAN.md
├── TARGET_ARCHITECTURE.md
├── CODING_STANDARD.md
├── SAFETY_REQUIREMENTS.md
├── VERIFICATION_PLAN.md
├── TRACEABILITY_MATRIX.csv
└── adr/
```

现有 `App/Com/Int` 在迁移期作为 legacy 代码保留；新模块通过适配层逐步接管。最终删除重复 legacy 业务代码，但不移动或改写 HAL/CMSIS/vendor 生成目录。

## 5. 运行时架构

### 5.1 任务模型

| 任务 | 建议优先级 | 周期/触发 | 唯一职责 |
|---|---:|---|---|
| `BmsSafetyTask` | 5 | 10 ms + 事件 | 安全评估、故障动作、看门狗许可 |
| `BqServiceTask` | 4 | 事件 + 100 ms/1 s | BQ 总线、采样、配置、FET 请求 |
| `ScServiceTask` | 4 | 事件 + 20 ms | SC 状态、standby 配置、受控启动 |
| `CanTask` | 2 | 20 ms/中断事件 | CAN RX/TX、协议、遥测 |
| `NvmTask` | 1 | 事件 + 慢周期 | 双槽日志、SOC/SOH、故障记录 |
| `DiagTask` | 1 | 事件 | 非阻塞日志、只读诊断 |
| Idle | 0 | 空闲 | 运行统计、低功耗入口 |

任务和队列使用静态创建。调度器启动后禁止依赖 `pvPortMalloc()`。

### 5.2 心跳与看门狗

- 每个关键任务维护单调 heartbeat 和 deadline miss 计数；
- `BmsSafetyTask` 只在 BQ、SC、安全任务自身以及调度器健康条件全部满足时允许喂 IWDG；
- IWDG 使用独立时钟，目标超时初始设为约 500 ms，最终值按 LSI 实测偏差确认；
- HardFault、assert、栈溢出和 malloc failure 不喂狗，并在允许的情况下保存最小故障上下文；
- 启动时读取 reset cause，写入诊断快照和 CAN 状态。

### 5.3 数据发布

业务模块不直接读取可变全局变量。测量服务发布不可变 `BmsSnapshot`：

```c
typedef struct
{
    uint32_t sequence;
    uint32_t timestamp_ms;
    BmsMeasurementQuality quality;
    BmsElectricalMeasurements electrical;
    BmsThermalMeasurements thermal;
    BmsBqStatus bq_status;
    BmsScStatus sc_status;
} BmsSnapshot;
```

发布采用双缓冲或短临界区复制。消费者只能读取完整快照。每个字段包含：

- 值；
- 单位；
- 有效性；
- 数据年龄；
- 来源；
- 必要时的原始值。

禁止用 IC 温度静默替代失效的电芯温度并继续允许充电。

## 6. 安全状态和故障模型

### 6.1 系统模式

```text
BOOT_SAFE
  -> SELF_TEST
  -> CONFIGURE
  -> STANDBY
  -> ACTIVE
  -> LOW_POWER
  -> SHUTDOWN_PENDING
  -> SHUTDOWN
  -> RECOVERY
  -> FAULT_LATCHED
  -> SERVICE
```

系统模式和路径允许状态分离：

- `charge_allowed`；
- `discharge_allowed`；
- `sc_charge_requested`；
- `balancing_allowed`。

这四个结论由统一策略计算，执行器只接受一个完整目标向量，避免多个模块分别控制硬件。

### 6.2 故障目录

每个故障具有固定描述：

```c
typedef struct
{
    BmsFaultId id;
    BmsFaultSeverity severity;
    BmsFaultLatchPolicy latch_policy;
    uint32_t debounce_ms;
    uint32_t recovery_ms;
    uint32_t action_mask;
} BmsFaultDescriptor;
```

严重度至少包括：

- `INFO`：记录；
- `WARNING`：告警但不改变路径；
- `DERATE`：降额；
- `TRIP`：立即禁止对应路径；
- `LATCHED_TRIP`：禁止自动恢复；
- `LOCKOUT`：只能维护流程解除。

故障来源至少覆盖：

- BQ Safety/PF；
- 单体电压、温度、电流、压差；
- BQ/SC/EEPROM/CAN 通信；
- 配置指纹；
- 数据陈旧和传感器失效；
- 任务 deadline；
- 栈余量；
- 看门狗和复位原因；
- 执行器命令与回读不一致；
- 非法状态转移；
- 维护命令授权失败。

### 6.3 shutdown/wake 证据链

只有以下证据链完整时才允许 SC 唤醒充电：

1. 主机明确请求 shutdown，或 BQ 状态确认进入 shutdown；
2. shutdown 前安全输出已确认；
3. BQ 离线或 SDM 状态被记录；
4. 外部输入有效且 SC 无故障；
5. 唤醒使用独立状态和绝对超时；
6. BQ 恢复后重新验证设备身份、配置和 FET 状态；
7. 正常策略重新授权后才释放功率路径。

普通 I2C 错误、单体异常、配置错误或未知离线不得进入 wake-charge。

## 7. BQ76952 设计

### 7.1 驱动边界

BQ 驱动只负责：

- direct command；
- subcommand；
- Data Memory；
- CRC/checksum；
- 帧长度、回显和超时；
- 原始状态码和错误计数。

驱动禁止：

- `printf`；
- 业务阈值判断；
- 自动开 FET；
- 内部无限重试；
- 跨事务保留不透明副作用。

### 7.2 事务所有权

完整的 `0x3E -> echo -> length -> buffer -> checksum` 视为单个不可分割事务，由 `BqServiceTask` 独占。其他任务通过静态队列提交请求。

请求分级：

1. 紧急 FET 安全动作；
2. 告警和状态读取；
3. 快速测量；
4. 慢速测量；
5. 配置和诊断。

任一事务都有绝对超时。超时后执行总线恢复，发布通信故障，不继续同一周期的剩余低优先级读取。

### 7.3 配置镜像和回读

BQ 配置使用表驱动 profile：

```c
typedef struct
{
    uint16_t address;
    uint8_t width;
    uint32_t expected;
    uint32_t compare_mask;
    BqConfigSafetyClass safety_class;
} BqConfigItem;
```

流程：

1. 设备身份确认；
2. 进入 ConfigUpdate；
3. 按 profile 写入；
4. 退出 ConfigUpdate；
5. 全量回读；
6. 对比并计算配置指纹；
7. 任何 safety-class 项不一致即锁存 `CONFIG_INVALID`；
8. 未通过配置验证不得释放主路径。

### 7.4 FET 控制

- 不使用通用 `ALL_FETS_ON` 作为每次转换前导；
- 每个转换有状态图和回读；
- 启动锁存解除建立独立一次性初始化流程；
- 预放电由 BQ 支持的正式机制执行，不由调试命令模拟生产路径；
- 所有 FET 命令失败、超时或回读不一致都使系统进入安全态；
- 最终实现必须通过门极波形确认不存在未请求脉冲。

## 8. SC8815 设计

- `PSTOP` 是软件紧急停充的第一安全执行器；
- 任何故障路径可同步把 `PSTOP` 拉高；
- 只有 `ScServiceTask` 在持有 Safety Supervisor 颁发的有效 permit 时才能释放 `PSTOP`；
- permit 包含快照序号、配置版本和过期时间，条件变化后自动失效；
- SC 配置只在 standby 中写入；
- 释放前必须验证通信、AC、短路、OTP、VBUS/VBAT 和限流配置；
- 运行中不周期重写模拟环路配置；
- 停止后回读状态，记录停止延迟和失败计数；
- 软件 I2C 初期保留但增加超时、总线恢复、IRQ-off 测量；条件允许时迁移到硬件 I2C；
- 运行期禁止自动翻转 SDA/SCL 线序，线序只由产品配置确定。

## 9. SOC/SOH 和均衡

### 9.1 SOC

SOC 模块是纯函数式状态机，不直接访问 HAL、RTOS 或全局变量。

基线算法：

- 有效电流上的库仑积分；
- 电流死区和时间间隔上限；
- 温度、SOH 和标定容量修正；
- 静置 OCV 修正；
- RC/内阻补偿输入；
- 满电和空电事件锚点；
- 显示 SOC 独立限速；
- 置信度和数据质量输出。

满空锚点使用“事件锁存 + 截止前条件 + 截止后确认”，不要求路径关闭后仍保持同方向电流。

若引入二阶 RC EKF，必须先有电芯 OCV-SOC、R0/R1/C1/R2/C2 随温度和 SOC 的标定数据。没有参数辨识证据时，不以复杂模型代替可验证性。

### 9.2 SOH

- 容量 SOH 只从合格的完整容量窗口学习；
- 电流采样缺口、复位或异常路径使本轮学习作废；
- 增加 DCIR 趋势接口，但没有受控脉冲数据时不输出高置信度结论；
- 保存已完成结果、循环吞吐、温度和压差历史；
- 对外同时输出数值、有效性和置信度。

### 9.3 均衡

均衡策略独立为纯策略模块。启用条件至少包括：

- 电芯采样完整有效；
- 电芯温度传感器有效；
- IC 温度有效；
- 无 Safety/PF/通信/config fault；
- 电压、温度、压差和充电状态满足标定阈值；
- 不选择 BQ 不允许同时均衡的组合；
- 目标 mask 写入后回读。

## 10. 持久化

保留并加强现有双槽、CRC、序号和写后回读设计。

新增记录类型：

- SOC 断点；
- SOH 和吞吐；
- reset cause；
- latched fault 摘要；
- 配置版本和指纹；
- 关键最大/最小值；
- 固件版本和硬件版本。

规则：

- NVM 写入由单任务完成；
- 使用事件合并和最小写入间隔；
- 断电中途写入不得破坏上一有效记录；
- 未完成学习状态不跨复位拼接；
- EEPROM 离线不阻塞安全功能；
- 数据结构显式序列化，不直接持久化 C struct 内存布局。

## 11. CAN、诊断和维护

### 11.1 CAN

- CAN 读取完整不可变快照；
- 协议字段使用固定宽度和显式字节序；
- 每个版本有 schema version；
- 数据无效时发送 invalid 标志，不发送伪正常值；
- 事件帧和周期帧分离；
- 总线拥塞采用最新状态合并，不能反压安全任务；
- bus-off 重连有退避和计数。

### 11.2 日志

- 统一结构化日志接口；
- 生产构建可关闭 DEBUG/TRACE；
- UART 使用环形缓冲和 DMA/IT；
- 安全任务只提交定长事件，不格式化长字符串；
- 记录 drop count；
- 不在 driver 中打印。

### 11.3 维护模式

生产默认只读诊断。危险操作必须同时满足：

- 工程构建；
- 物理维护输入有效；
- challenge/response 或显式解锁；
- 车辆处于安全状态；
- 命令通过 Safety Supervisor；
- 自动超时；
- 操作和结果写入审计记录。

## 12. 编码与内存规则

- 语言：C11；
- 安全判断使用整数单位，浮点仅限估算算法和显示；
- 所有物理量名称带单位后缀，例如 `_mv`、`_ma`、`_ms`、`_deg_c`；
- 禁止递归、VLA、隐式窄化、未界定循环和裸魔数；
- 禁止业务层直接引用 HAL handle；
- 头文件不暴露可变全局变量；
- 公共 API 检查参数并返回统一状态码；
- 生产运行期不使用动态内存；
- 所有队列、任务、事件组和缓冲区静态创建；
- 所有序列化字段显式编码；
- 对跨任务状态使用快照、队列或原子标志，不依赖 `volatile` 解决同步；
- 中断只采集必要状态并通知任务；
- 对有界轮询标注最大次数和最大耗时；
- 对所有安全 API 标注前置条件、后置条件、并发所有权、超时和失效动作。

详细规范见 `CODING_STANDARD.md`。

## 13. 构建与 CI 门禁

CI 至少包含：

1. GCC Debug cross-build；
2. GCC Release cross-build；
3. 主机单元测试；
4. `clang-format --dry-run --Werror`；
5. `clang-tidy`；
6. `cppcheck`；
7. 警告视为错误；
8. 栈使用文件生成；
9. Flash/RAM 大小预算；
10. 禁止模式扫描；
11. 生成目录改动检测；
12. 需求追溯完整性检测。

禁止模式包括：

- safety/domain/driver 中的 `printf`；
- scheduler 启动后的动态内存；
- domain 层包含 `stm32*.h`；
- 非 actuator 模块调用执行器 GPIO；
- 非 BQ service 访问 BQ driver；
- 非 SC service 写 SC 寄存器；
- `HAL_Delay` 出现在运行期服务；
- 未授权危险 CLI 字符串；
- 修改 HAL/CMSIS/vendor 生成文件。

## 14. 验证策略

### 14.1 主机测试

- safety state transition；
- fault debounce/recovery/latch；
- 输出动作顺序；
- shutdown/wake provenance；
- SOC 积分、静置修正、满空锚点；
- SOH 学习取消和完成；
- 均衡选择；
- NVM 编解码和断电恢复；
- CAN 编解码；
- 时间回绕、整数边界和无效数据。

### 14.2 集成测试

使用 fake BQ/SC/EEPROM/CAN 注入：

- NACK；
- 超时；
- 错误 checksum；
- 数据陈旧；
- 任务延迟；
- 执行器回读不一致；
- EEPROM 半写；
- CAN FIFO 满和 bus-off。

### 14.3 HIL/实板测试

至少覆盖：

- BQ SDA/SCL 拉低；
- SC I2C NACK；
- TS1/TS3 开路和短路；
- 充电器热插拔；
- 低压、满电、过温、SCD/OCD/OCC；
- BQ shutdown/wake；
- MCU HardFault/任务挂起；
- UART/CAN 故障；
- EEPROM 断开和写入中掉电；
- 所有功率路径转换波形。

关键量化指标：

- 软件检测到 trip 后到 `PSTOP=1` 的延迟；
- BQ/SC 单事务和完整周期的最坏执行时间；
- Safety Task WCET 和 deadline miss；
- 最长全局关中断时间；
- 各任务最小剩余栈；
- 看门狗复位时间；
- FET 非预期门极脉冲数量必须为 0。

## 15. 分阶段迁移

### Gate 0：冻结和基线

交付：

- 固定基线 SHA；
- 建立专用分支；
- 当前架构、风险、硬件映射和已知冲突清单；
- 生成文件只读清单；
- 当前 GCC 构建、尺寸和警告基线。

退出条件：现状可复现且没有未记录的本地改动。

### Gate 1：工程基础设施

交付：

- 新目录骨架；
- `clang-format`、静态分析、host-test 和 CI；
- 静态 FreeRTOS 对象；
- assert、stack overflow、malloc failure、reset cause 和 IWDG 基础；
- 生成目录保护脚本。

退出条件：旧功能仍可编译，CI 全绿。

### Gate 2：安全执行器和故障管理

交付：

- `BmsActuator`；
- 同步 `PSTOP` 紧急停止；
- `BmsFault`；
- `BmsSafetyTask`；
- 输出目标向量和动作顺序；
- 移除生产危险 CLI。

退出条件：P0 中的异步停充和 CLI 绕过问题关闭。

### Gate 3：BQ 单所有者服务

交付：

- 新 BQ driver/service；
- 有界超时和总线恢复；
- 事务请求队列；
- 快慢采样；
- 配置表和全量回读；
- FET 明确状态机。

退出条件：无跨任务间接窗口访问，配置完整验证，FET 转换单元测试通过。

### Gate 4：SC 单所有者服务

交付：

- SC service；
- permit 机制；
- standby 配置与最后释放；
- 运行期线序固定；
- 中断和采样预算。

退出条件：任何异常均同步停充，启动只能经 supervisor。

### Gate 5：快照和功率策略

交付：

- 完整测量质量模型；
- 不可变快照；
- charge/discharge/balance policy；
- shutdown/wake 证据链；
- 所有状态回差和锁存表驱动。

退出条件：无效或陈旧数据不能被当作正常数据，普通通信故障不能进入 wake-charge。

### Gate 6：SOC/SOH/NVM

交付：

- 纯逻辑 SOC/SOH；
- 事件式锚点；
- NVM schema 升级和迁移；
- 断电恢复和置信度。

退出条件：主机测试覆盖边界、复位和采样缺口。

### Gate 7：CAN、日志和维护

交付：

- 协议版本化；
- 非阻塞日志；
- 只读生产诊断；
- 维护授权流程。

退出条件：CAN/UART 故障不影响安全任务。

### Gate 8：Legacy 移除和统一风格

交付：

- 移除被替代的 `App/Com/Int` 重复实现；
- 统一命名、注释和 include；
- 无可变全局快照；
- 文档和代码完全一致。

退出条件：旧入口无引用，功能全部由 V2 模块承载。

### Gate 9：HIL 和实车验证

交付：

- 故障注入报告；
- 波形；
- 长稳、热测试、充放电循环；
- CAN/日志证据；
- 时间和资源预算报告。

退出条件：所有安全需求有实板证据。

### Gate 10：发布冻结

交付：

- 发布 tag；
- 固件二进制和哈希；
- 配置指纹；
- 测试报告；
- 追溯矩阵；
- 已知限制；
- 回滚版本。

退出条件：评分表 100/100，且没有未关闭的 P0/P1。

## 16. 提交和审核策略

- 所有工作在 `refactor/bms-v2-safety-architecture` 或其阶段子分支完成；
- 每个 Gate 独立 PR 或可审核提交组；
- 不把大规模重命名和行为修改混在同一提交；
- 先加测试，再替换实现，再删除 legacy；
- 每次提交说明：需求 ID、风险、行为变化、验证结果；
- main 不直接推送；
- 禁止提交构建产物、日志、缓存、固件包和临时测试文件；
- 实板证据放在受控文档目录或发布资产中，不污染源码目录。

## 17. 首批执行顺序

第一批不等待全量重构，直接关闭最高风险：

1. 建立编码规范、安全需求和验收矩阵；
2. 加入 CI/host-test/生成目录保护；
3. 实现同步 `PSTOP` 安全覆盖；
4. 建立生产/工程构建隔离并移除危险生产 CLI；
5. 加入 IWDG、heartbeat、stack overflow 和 fault hooks；
6. 将 BQ 首次通信失败后的剩余采样改为立即中止；
7. 建立 BQ 单所有者事务服务；
8. 替换 FET 通用 `ALL_FETS_ON` 序列；
9. 建立完整配置回读；
10. 再迁移 SOC/SOH、CAN 和 NVM。

## 18. 当前未确定项

以下内容必须通过硬件资料或实板确认后冻结：

- SC8815 软件 I2C 是否可迁移到空闲硬件 I2C；
- BQ FET 启动锁存解除的最安全命令序列；
- CHG/DSG/PCHG/PDSG 实际门极时序；
- TS1/TS3 的热敏电阻型号、Beta 值和安装位置；
- 电芯 OCV-SOC 和二阶 RC 参数；
- IWDG LSI 实际频率和最终超时；
- 充放电电流、温度和恢复阈值的最终标定值；
- 生产维护授权使用的物理输入；
- 硬件是否具备独立于 MCU 的 SC8815 停止门控。

这些未知项不会阻止软件骨架搭建，但会阻止最终 100/100 冻结。
