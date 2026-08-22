# BMS V2 验证与证据计划

- 版本：1.0
- 状态：Active
- 对应需求：`docs/refactor/SAFETY_REQUIREMENTS.md`
- 追溯表：`docs/refactor/TRACEABILITY_MATRIX.csv`

## 1. 验证目标

验证工作必须回答四个问题：

1. 逻辑在正常条件下是否给出正确结果；
2. 输入无效、超时、并发、复位和边界条件下是否进入确定安全态；
3. 软件命令是否在真实硬件上产生预期时序，且没有未请求的中间态；
4. 构建、配置、测试和实板证据是否能够关联到唯一固件版本。

测试通过不只意味着“功能能运行”，还必须证明失败路径有界、可诊断、可恢复或正确锁存。

## 2. 验证层级

### 2.1 静态分析与架构检查（A/CI）

覆盖：

- include 依赖方向；
- HAL/CubeMX/vendor 目录保护；
- 动态内存、阻塞日志、HAL_Delay、越层 HAL handle；
- warnings-as-errors；
- clang-format、clang-tidy、cppcheck；
- 无追踪 TODO/FIXME/HACK；
- 生产危险命令和禁止 API；
- Flash/RAM/栈预算；
- 需求—设计—测试追溯完整性。

证据：GitHub Actions run、编译日志、静态分析输出和 commit SHA。

### 2.2 主机单元测试（UT）

对象：无 HAL/RTOS 的 domain/common/protocol/NVM codec。

要求：

- 每个 fault 测试触发、去抖、动作、恢复、锁存、清除和计数饱和；
- 每个状态测试合法转换、非法事件、超时和时间回绕；
- 每个算法测试零值、上下边界、无效输入、采样缺口和整数溢出；
- 每个编解码器测试错误长度、错误版本、CRC、截断和随机数据；
- 缺陷修复前先加入能够复现缺陷的测试。

初始质量目标：

- domain 分支覆盖率 >= 90%；
- safety/fault/actuator-plan 决策覆盖率 100%；
- 所有 P0 guard 的 true/false 路径均有测试。

覆盖率工具在 host test 稳定后加入；在此之前不得用测试数量代替覆盖率。

### 2.3 软件集成测试（IT）

使用 fake platform/driver 验证 service 和任务交互：

- BQ direct/indirect 事务；
- SC 软件 I2C；
- 请求优先级和队列满；
- snapshot 双缓冲；
- actuator 目标应用顺序；
- watchdog heartbeat；
- NVM 双槽和掉电；
- CAN 拥塞、bus-off 和 malformed request；
- 非阻塞日志丢弃策略。

所有 fake 必须支持注入：成功、NACK、timeout、CRC、陈旧值、错误回读、重复事件、乱序事件和时间推进。

### 2.4 硬件在环与故障注入（HIL）

HIL 不是人工“看起来正常”，而是可重复脚本和结构化结果。每条用例记录：

- 测试 ID；
- 固件 Git SHA；
- 硬件版本/序列号；
- BQ/SC 配置指纹；
- 测试仪器；
- 输入步骤；
- 预期值和容差；
- 实测值；
- 原始日志/CSV/波形引用；
- pass/fail；
- 执行人和日期。

### 2.5 波形验证（OSC）

至少同步采集：

- `PSTOP`；
- `CE_N`；
- `CHG`；
- `DSG`；
- `PCHG`；
- `PDSG`；
- 必要时 `PACK`、`LD`、`VBUS`、`BMS_OUT+`。

关键转换：

1. 上电复位；
2. 初始化失败；
3. standby -> charge；
4. charge -> stop；
5. 全关 -> discharge；
6. discharge -> stop；
7. 预放电 -> DSG；
8. SCD/OCD/CUV/OTD；
9. BQ shutdown；
10. charger wake；
11. MCU watchdog reset；
12. BQ/SC 通信故障。

验收：未请求的 FET 门极脉冲数量必须为 0。

### 2.6 整车验证（VEH）

整车只在 Gate 0~8 软件和台架测试通过后执行。覆盖：

- 充电器插拔；
- 负载阶跃；
- CAN 查询和掉线；
- 长稳；
- 多次上电/关机；
- 低 SOC 和满电；
- 电机干扰；
- 线束扰动；
- 温升与环境温度边界。

## 3. 测试 ID 规范

```text
UT-FLT-xxx   故障管理
UT-SAF-xxx   安全策略
UT-PWR-xxx   充放电策略
UT-SOC-xxx   SOC
UT-SOH-xxx   SOH
UT-BAL-xxx   均衡
UT-NVM-xxx   持久化 codec
UT-CAN-xxx   CAN codec

IT-BQ-xxx    BQ service/driver
IT-SC-xxx    SC service/driver
IT-ACT-xxx   执行器
IT-WDG-xxx   健康监督
IT-NVM-xxx   EEPROM service
IT-CAN-xxx   CAN task
IT-DIAG-xxx  日志/维护

HIL-BQ-xxx
HIL-SC-xxx
HIL-THERM-xxx
HIL-NVM-xxx
HIL-CAN-xxx
HIL-WDG-xxx

OSC-PWR-xxx
VEH-SYS-xxx
```

测试 ID 一旦进入发布证据不可复用或改变语义。

## 4. 首批主机测试清单

| Test ID | 目标 | 当前状态 |
|---|---|---|
| UT-FLT-001 | Trip 去抖未达到阈值时不激活 | Implemented |
| UT-FLT-002 | Trip 达到阈值时激活并产生动作 | Implemented |
| UT-FLT-003 | 自动恢复保持时间 | Implemented |
| UT-FLT-004 | 手动锁存恢复后仍有效 | Implemented |
| UT-FLT-005 | 原始条件未恢复时拒绝 clear | Implemented |
| UT-FLT-006 | 多故障动作 mask 聚合 | Implemented |
| UT-FLT-007 | 描述表 ID 不一致时拒绝初始化 | Implemented |
| UT-FLT-008 | occurrence_count 饱和不回绕 | Planned |
| UT-FLT-009 | 32 位时间戳回绕不影响 elapsed 输入 | Planned |
| UT-SAF-001 | 无有效快照时输出全安全目标 | Planned |
| UT-SAF-002 | 温度传感器均无效时禁止充电/均衡 | Planned |
| UT-SAF-003 | BQ config invalid 时禁止所有释放 | Planned |
| UT-SAF-004 | SC permit 过期时禁止释放 PSTOP | Planned |
| UT-SAF-005 | 普通 BQ offline 不进入 wake-charge | Planned |
| UT-SAF-006 | 完整 shutdown provenance 才允许 wake-charge | Planned |
| UT-ACT-001 | Stop plan 第一动作是 PSTOP safe | Planned |
| UT-ACT-002 | Start plan 最后一动作是释放 PSTOP | Planned |
| UT-ACT-003 | 任一步失败后不执行后续启动动作 | Planned |

## 5. 首批集成/HIL 清单

| Test ID | 注入/场景 | 关键验收 |
|---|---|---|
| IT-BQ-001 | direct NACK | 当前快照终止且无后续低优先级读 |
| IT-BQ-002 | indirect echo mismatch | 有界超时且事务不串帧 |
| IT-BQ-003 | 配置 readback mismatch | CONFIG_INVALID 锁存 |
| IT-BQ-004 | 高优先级 FET off 与普通采样并发 | FET off 优先处理 |
| IT-SC-001 | SC NACK | PSTOP 保持高 |
| IT-SC-002 | permit 过期 | 不释放 PSTOP |
| IT-SC-003 | SDA stuck low | 恢复有界，失败保持安全 |
| IT-WDG-001 | BQ task heartbeat 停止 | 撤销 permit 并触发 watchdog |
| IT-NVM-001 | 写入半途掉电 | 上一槽仍可恢复 |
| IT-CAN-001 | TX FIFO 满 | 不阻塞 Safety Task |
| HIL-THERM-001 | TS1/TS3 开路 | 禁止充电和均衡 |
| HIL-BQ-001 | BQ SDA 拉低 | 软件停止 SC 且最终 watchdog 恢复 |
| HIL-SC-001 | SC SDA 拉低 | PSTOP 不受总线故障影响 |
| HIL-WDG-001 | 人为挂起 BQ service | 在预算内安全停充和复位 |
| OSC-PWR-001 | Reset 到初始化完成 | 无主动充电脉冲 |
| OSC-PWR-002 | Trip 到 PSTOP | 记录实际延迟且满足预算 |
| OSC-PWR-003 | 全关到 DSG/预放电 | 无 ALL_FETS_ON 非预期脉冲 |

## 6. 资源和实时性验证

每个发布候选必须输出：

- Flash 使用量和剩余；
- RAM 静态区、heap 配置和实际 heap 使用；
- 每个任务配置栈和 minimum watermark；
- Safety/BQ/SC/CAN/NVM/Diag 的 WCET；
- deadline miss count；
- 最大 IRQ-off 时间；
- BQ direct/indirect 最大事务时间；
- SC 软件 I2C 最大事务时间；
- trip -> PSTOP 延迟；
- watchdog 实际超时。

测量必须在 Debug 和 Release 中区分；发布结论以 Release 固件为准。

## 7. 配置验证

BQ Data Memory profile 每个项目记录：

```text
address | width | expected | readback | mask | safety_class | pass
```

配置指纹由确定顺序的安全配置项计算。HIL 记录、CAN 诊断和发布说明都必须包含该指纹。

SC 关键寄存器同样建立只读验证快照，至少覆盖：

- ratio；
- external VBAT selection；
- IBUS limit；
- IBAT limit；
- OTG/reverse disabled；
- protection enable；
- ADC enable；
- standby/running state。

## 8. 证据保存

源码仓库只保存小型、可审阅文本：

```text
validation/
├── manifests/
├── reports/
├── expected/
└── scripts/
```

大型 CSV、示波器工程、图片、固件包和长日志作为 GitHub Release/Actions artifact 或受控存储附件；仓库内 manifest 保存 SHA-256、文件名、测试 ID 和外部引用。

禁止提交临时日志、构建目录、cache、ELF/HEX/BIN/MAP 到源码历史。

## 9. 缺陷处理

- 测试失败先创建唯一缺陷 ID；
- 不通过修改期望值掩盖失败；
- P0/P1 缺陷修复必须增加回归测试；
- HIL 波形异常必须保留失败证据；
- 关闭缺陷时关联修复 commit、测试 run 和实板记录；
- 无法复现不能自动降级优先级。

## 10. Gate 退出证据

| Gate | 最小退出证据 |
|---|---|
| 0 | 固定 SHA、审阅报告、硬件事实、计划 |
| 1 | CI 全绿、host test、生成目录保护、静态资源基础 |
| 2 | 同步 PSTOP、安全故障 manager、危险 CLI 隔离测试 |
| 3 | BQ 单 owner、配置回读、FET 转换测试 |
| 4 | SC 单 owner、permit、软件 I2C 故障测试 |
| 5 | 快照质量、统一 policy、shutdown provenance 测试 |
| 6 | SOC/SOH/NVM 边界、掉电和采样缺口测试 |
| 7 | CAN/日志/维护故障隔离测试 |
| 8 | legacy 无引用、全仓结构和风格检查 |
| 9 | HIL、OSC、长稳、热和整车报告 |
| 10 | 追溯矩阵 100%、P0/P1=0、发布 manifest 和回滚包 |

## 11. 100 分判定

只有所有 hard-stop 条件满足后才计算分数。以下任一项失败，最高评分为 99 且禁止发布：

- P0/P1 未关闭；
- 关键需求无证据；
- CI 或测试失败；
- BQ 配置不一致；
- 执行器绕过路径存在；
- 危险生产 CLI 存在；
- watchdog/heartbeat 未验证；
- 关键 FET/PSTOP 波形缺失；
- 温度传感器失效仍允许充电；
- 通信故障可误进入 wake-charge。

达到 100 分表示满足本项目定义的工程发布基线，不代表第三方功能安全认证。
