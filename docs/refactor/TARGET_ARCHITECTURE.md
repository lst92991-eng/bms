# BMS V2 目标软件架构

- 版本：1.0
- 依赖方向：`app -> services -> domain/drivers -> platform -> HAL`
- 核心原则：策略与 IO 分离、单一所有者、不可变快照、同步安全覆盖、静态资源、可主机测试

## 1. 系统上下文

```text
                  +-------------------+
24 V charger --->|      SC8815       |----> BMS charge power stage
                  +---------+---------+
                            |
                            | status / ADC / PSTOP / CE_N
                            v
+--------+       +---------------------+       +-------------------+
| Cells  |------>|      BQ76952        |------>| CHG/DSG/PCHG/PDSG |
+--------+       +----------+----------+       +-------------------+
                           |
                           | I2C / ALERT
                           v
                 +---------------------+
                 |    STM32G0B1 MCU    |
                 |  BMS V2 firmware    |
                 +--+--------+------+--+
                    |        |      |
                  CAN FD   EEPROM  UART/OLED
```

BQ76952 负责独立硬件监测和保护；MCU 不得削弱 BQ 自主保护。MCU 负责产品策略、执行器许可、SOC/SOH、通信、记录和诊断。

## 2. 分层

### 2.1 App

职责：

- 创建静态 RTOS 对象；
- 按安全顺序启动服务；
- 连接任务和消息通道；
- 处理系统级启动失败。

禁止：

- 直接访问寄存器；
- 写功率 GPIO；
- 包含保护算法；
- 维护可变遥测全局变量。

### 2.2 Domain

纯逻辑层，可在 PC 上编译：

- 安全状态机；
- 故障管理；
- charge/discharge/balance policy；
- SOC/SOH；
- 测量有效性；
- 状态转换和动作规划。

Domain 不包含：

- HAL/FreeRTOS；
- printf；
- I2C/CAN/EEPROM；
- GPIO；
- 系统 tick 读取。时间作为输入传入。

### 2.3 Services

异步运行和资源所有权层：

- BQ service；
- SC service；
- actuator service；
- measurement publish；
- watchdog；
- NVM；
- telemetry；
- diagnostics。

Service 负责把 domain 的意图变成有界 IO，并把结果重新发布为事件或快照。

### 2.4 Drivers

芯片协议层：

- BQ76952 direct/subcommand/Data Memory；
- SC8815 register/ADC；
- EEPROM page read/write；
- FDCAN frame；
- OLED raw drawing。

Driver 返回原始错误，不决定产品行为。

### 2.5 Platform

对 HAL 的唯一适配层：

- I2C 事务；
- GPIO；
- UART；
- watchdog；
- time；
- critical section；
- reset cause；
- board pin mapping。

除 platform 和极少数 generated adapter 外，自研代码不得引用 HAL handle。

## 3. 编译期依赖规则

允许：

```text
app       -> services, domain, config
services  -> domain, drivers, platform, common, config
protocol  -> domain, common
 drivers  -> platform, common
 domain   -> common, config
 platform -> HAL/CMSIS/generated
```

禁止：

```text
domain   -> FreeRTOS/HAL/drivers/services
 driver   -> app/domain policy
 platform -> domain/services
 protocol -> HAL
```

CI 通过 include 扫描和 CMake target 分离执行这些规则。

## 4. 任务与所有权

### 4.1 BmsSafetyTask

输入：

- 最新完整 `BmsSnapshot`；
- BQ/SC service 事件；
- shutdown/maintenance 请求；
- task health；
- 当前时间。

输出：

- `BmsActuator_Target`；
- fault transition；
- BQ/SC 高优先级请求；
- watchdog permit；
- telemetry event。

规则：

- 10 ms 周期；
- 不直接访问 I2C、UART、CAN、EEPROM；
- 可以通过 actuator service 的同步安全 API 断言 PSTOP；
- 不等待普通队列；
- 任何内部异常映射为安全输出。

### 4.2 BqServiceTask

唯一拥有：

- I2C1 上的 BQ76952 地址；
- 0x3E~0x61 间接窗口；
- BQ 配置和 FET 命令；
- BQ 快慢采样计划。

请求接口：

```c
typedef enum
{
    BQ_REQUEST_EMERGENCY_FET_OFF = 0,
    BQ_REQUEST_APPLY_FET_TARGET,
    BQ_REQUEST_READ_ALERT_STATUS,
    BQ_REQUEST_FAST_SAMPLE,
    BQ_REQUEST_SLOW_SAMPLE,
    BQ_REQUEST_VERIFY_CONFIG,
    BQ_REQUEST_DIAGNOSTIC_READ
} BqService_RequestType;
```

实现采用高优先级 mailbox + 普通静态队列。每次处理普通请求前必须检查紧急 mailbox。

### 4.3 ScServiceTask

唯一拥有：

- SC8815 软件/硬件 I2C；
- SC 寄存器配置；
- CE_N 的正常控制；
- PSTOP 的释放动作；
- SC status/ADC sampling。

Safety Task 可以通过 `Actuator_StopChargeImmediate()` 单向断言 PSTOP。ScService 每次尝试释放前必须检查 permit token。

### 4.4 CanTask

- 读取只读发布快照；
- 接收查询/控制请求；
- 所有控制请求转发给 Safety Task；
- 不直接改变功率状态；
- 发送拥塞不能阻塞其他任务。

### 4.5 NvmTask

- 单一 EEPROM 写 owner；
- 合并写请求；
- 双槽/日志提交；
- readback 验证；
- 不参与实时安全动作。

### 4.6 DiagTask

- 消费结构化日志事件；
- UART DMA/IT 发送；
- 维护只读查询；
- production 无危险控制命令。

## 5. 消息和数据对象

### 5.1 测量质量

```c
typedef enum
{
    BMS_DATA_INVALID = 0,
    BMS_DATA_VALID,
    BMS_DATA_STALE,
    BMS_DATA_OUT_OF_RANGE,
    BMS_DATA_UNAVAILABLE
} BmsData_Quality;

typedef struct
{
    BmsData_Quality quality;
    uint32_t timestamp_ms;
    uint32_t source_flags;
} BmsData_Meta;
```

每组相关测量使用共同采样序号，防止将不同周期数据拼成一帧。

### 5.2 快照

```c
typedef struct
{
    uint32_t sequence;
    uint32_t published_at_ms;

    BmsCellMeasurements cells;
    BmsPackMeasurements pack;
    BmsThermalMeasurements thermal;
    BmsBqDiagnostics bq;
    BmsScDiagnostics sc;
    BmsEstimatorOutputs estimator;
} BmsSnapshot;
```

发布：双缓冲 + 短临界区索引切换。

读取：

1. 复制 active index；
2. 复制完整快照；
3. 校验 sequence 未变化；
4. 如变化则重试一次；
5. 仍不一致则返回 unavailable。

### 5.3 执行器目标向量

```c
typedef struct
{
    bool allow_charge_path;
    bool allow_discharge_path;
    bool request_sc_charge;
    bool allow_balancing;
    uint32_t reason_mask;
    uint32_t snapshot_sequence;
    uint32_t valid_until_ms;
} BmsActuator_Target;
```

目标向量是完整值，不是若干独立 setter，避免部分更新。

### 5.4 执行结果

```c
typedef struct
{
    BmsActuator_Target requested;
    BmsActuator_Observed observed;
    BmsStatus status;
    uint32_t applied_at_ms;
    uint32_t latency_us;
} BmsActuator_Result;
```

Safety Supervisor 使用 result 决定是否进入下一状态。

## 6. 安全控制闭环

```text
Snapshot published
       |
       v
BmsSafety_Evaluate(snapshot, fault_state, time)
       |
       +--> fault transitions
       |
       +--> desired actuator target
                    |
                    v
          BmsActuator_ApplyTarget
                    |
          +---------+----------+
          |                    |
   immediate safe GPIO     BQ/SC requests
          |                    |
          +---------+----------+
                    v
             observed result
                    |
                    v
          Safety confirms or trips
```

安全逻辑分成两部分：

1. `Evaluate`：纯函数，输入状态和快照，输出故障与目标；
2. `Apply/Confirm`：service 执行 IO，并反馈实际状态。

同一个函数不得同时混合大量策略判断和 HAL 写操作。

## 7. 执行器状态转换

### 7.1 停充

```text
Any state
  -> assert PSTOP high synchronously
  -> invalidate SC permit
  -> request SC standby
  -> request BQ charge path target
  -> read back
  -> publish stop result
```

第一步不依赖任何总线。后续失败不允许重新释放 PSTOP。

### 7.2 启动充电

```text
STANDBY
  -> snapshot valid and fresh
  -> no charge-inhibit fault
  -> BQ config valid
  -> temperature sensors valid
  -> SC input valid
  -> SC standby configuration verified
  -> BQ charge path target applied and observed
  -> safety re-evaluation with same/newer snapshot
  -> issue short-lived permit
  -> release PSTOP
  -> observe SC running
  -> CHARGING
```

任一步失败：PSTOP high + fault/result event。

### 7.3 放电路径

放电允许由 Safety Supervisor 计算。BQ service 执行明确 FET 状态转换。预放电必须由正式 BQ 机制处理，并通过 PACK/LD/FET 状态确认，禁止以测试接口作为生产流程。

## 8. 故障管理架构

### 8.1 Raw condition 与 fault 分离

```text
measurement/status
    -> raw condition
    -> debounce/recovery timer
    -> active fault
    -> severity/latch
    -> action mask
```

同一个 raw condition 不应在多个模块分别做不同去抖。

### 8.2 Fault state

```c
typedef struct
{
    uint64_t active_mask;
    uint64_t latched_mask;
    uint64_t history_mask;
    uint32_t occurrence_count[BMS_FAULT_COUNT];
    uint32_t first_seen_ms[BMS_FAULT_COUNT];
    uint32_t last_change_ms[BMS_FAULT_COUNT];
} BmsFault_State;
```

若 RAM 预算不足，计数和时间字段可只为 P0/P1 fault 保留，但协议 ID 不能动态变化。

### 8.3 Fault clear

clear 是请求，不是直接清零：

1. 验证调用权限；
2. 验证 raw condition 已恢复；
3. 验证恢复保持时间；
4. 验证当前系统处于可清除状态；
5. 清除允许清除的 latch；
6. 保留 history 和 occurrence count；
7. 记录审计事件。

## 9. BQ service 内部架构

```text
BqServiceTask
├── request arbitration
├── transport state
├── direct command driver
├── indirect transaction driver
├── fast acquisition plan
├── slow acquisition plan
├── config manager
├── FET transition controller
└── bus health counters
```

### 9.1 采样计划

Fast snapshot 候选：

- current；
- stack/pack；
- alarm/safety summary；
- 与安全和 SOC 相关的快速量。

Slow snapshot：

- 6 路单体；
- TS1/TS3/IC；
- FET/manufacturing/battery status；
- PF；
- balance status；
- 诊断统计。

应优先评估 BQ 提供的 Data Status block，以减少事务数量并提高同帧一致性；采用前必须依据当前器件版本和 TRM 验证字段布局。

### 9.2 错误策略

- 单次 NACK：终止当前事务，记录；
- 当前快照首个 transport error：终止剩余低优先级采样；
- 有界重试只由 service 决定；
- 连续失败达到阈值：BUS_OFFLINE fault；
- bus recovery 后重新验证设备身份；
- 不在失败时自动改变 CRC 模式或地址。

## 10. SC service 内部架构

```text
ScServiceTask
├── fixed board wiring configuration
├── bounded bus driver
├── standby configuration
├── permit validation
├── status/ADC acquisition
├── controlled start
├── controlled stop
└── interrupt event handling
```

Permit 示例：

```c
typedef struct
{
    uint32_t token;
    uint32_t snapshot_sequence;
    uint32_t config_fingerprint;
    uint32_t expires_at_ms;
} ScCharge_Permit;
```

释放 PSTOP 前检查：

- token 与当前 supervisor generation 一致；
- 未过期；
- snapshot sequence 未倒退；
- config fingerprint 一致；
- 没有 stop override；
- SC observed state 安全。

## 11. 看门狗和健康监督

每个任务：

```c
typedef struct
{
    uint32_t heartbeat;
    uint32_t last_run_ms;
    uint32_t deadline_miss_count;
    uint32_t min_stack_words;
    BmsTask_HealthState state;
} BmsTask_Health;
```

Safety Task 评估：

- BQ service 是否在截止期内；
- SC service 是否在截止期内；
- 自身周期是否正常；
- scheduler tick 是否推进；
- execution overrun 是否超过阈值；
- critical fault 是否处于未确认动作状态。

只有全部通过时调用 platform watchdog refresh。

## 12. NVM 架构

记录头：

```text
magic | schema_version | record_type | length | sequence | payload | crc32
```

提交：

1. 选择 inactive slot；
2. 编码到本地 buffer；
3. 写入；
4. 回读；
5. CRC/语义验证；
6. 更新 RAM active slot；
7. 不擦除上一有效槽。

不同记录类型使用独立保存节奏，避免 SOC 高频变化导致 SOH/故障记录过度写入。

## 13. 诊断和可观测性

系统必须能查询：

- firmware/build/config fingerprint；
- system state；
- active/latched faults；
- snapshot age/quality；
- BQ/SC last error and counters；
- actuator requested/observed state；
- task heartbeats/deadline/stack；
- reset cause；
- log drop count；
- NVM active slot/sequence；
- SOC/SOH validity/confidence。

这些信息通过 CAN 和工程只读 CLI 输出，不能要求打开危险调试接口。

## 14. 测试替换点

所有外部依赖通过接口注入：

```c
typedef struct
{
    BmsStatus (*read)(void *context,
                      uint16_t address,
                      uint8_t *data,
                      size_t size,
                      uint32_t timeout_ms);
    BmsStatus (*write)(void *context,
                       uint16_t address,
                       const uint8_t *data,
                       size_t size,
                       uint32_t timeout_ms);
    BmsStatus (*recover)(void *context);
    void *context;
} BmsI2c_Interface;
```

Host test 使用 fake interface；target build 注入 STM32 platform interface。Domain 无需 fake HAL。

## 15. 迁移边界

迁移期适配原则：

- 新 V2 模块不依赖 legacy 可变全局；
- legacy 数据通过一次性 adapter 转成 `BmsSnapshot`；
- 新 actuator 接管后，legacy 直接执行器 API 禁止调用；
- 每替换一个 legacy 模块，先加入行为测试，再切换 CMake，再删除旧引用；
- 不进行“全仓一次性重命名 + 行为重写”，避免不可审核 diff。

## 16. 架构验收

目标架构完成时必须满足：

- 生成目录零业务改动；
- domain 可用 host compiler 独立构建；
- 所有硬件资源有唯一 owner；
- 没有可变遥测全局；
- 没有生产危险 CLI；
- 没有安全任务阻塞日志；
- 没有运行期动态对象；
- 关键状态机和 fault 具有自动测试；
- 需求追溯矩阵完整；
- 所有执行器动作有回读；
- P0/P1 HIL 和波形全部通过。
