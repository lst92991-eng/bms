# BMS V2 安全需求基线

- 版本：1.0
- 状态：Draft for implementation
- 说明：本文件定义软件工程验收要求；阈值最终值仍需硬件、电芯和热设计标定。

## 1. 标识和验证方法

需求 ID：`BMS-SR-xxx`

验证方法：

- `A`：分析/代码审查；
- `UT`：主机单元测试；
- `IT`：软件集成测试；
- `HIL`：硬件在环/故障注入；
- `OSC`：示波器或逻辑分析仪；
- `VEH`：整车验证。

优先级：

- `P0`：不满足则禁止上电功率测试；
- `P1`：不满足则禁止发布；
- `P2`：质量和可维护性门禁。

## 2. 启动与默认安全态

| ID | 优先级 | 需求 | 验证 |
|---|---|---|---|
| BMS-SR-001 | P0 | MCU 复位到应用接管期间，SC8815 PSTOP 必须保持安全电平，固件不得产生主动充电脉冲。 | A, OSC |
| BMS-SR-002 | P0 | 初始化任何步骤失败时，不得释放 SC 充电功率级或未经确认的 BQ 主 FET。 | UT, IT, HIL |
| BMS-SR-003 | P0 | 调度器未启动、关键任务创建失败或静态对象初始化失败时，系统必须保持安全输出并进入确定故障路径。 | UT, IT |
| BMS-SR-004 | P1 | 启动必须记录 reset cause、固件版本、配置版本和上次锁存故障摘要。 | IT, HIL |
| BMS-SR-005 | P0 | 未完成 BQ 设备身份确认和安全配置回读前，不得允许正常充放电路径。 | UT, IT, HIL |

## 3. 执行器和功率路径

| ID | 优先级 | 需求 | 验证 |
|---|---|---|---|
| BMS-SR-010 | P0 | 所有充电停止路径必须首先同步断言 PSTOP，不得依赖队列、UART、CAN、EEPROM 或低优先级任务。 | A, UT, HIL, OSC |
| BMS-SR-011 | P0 | 从 Safety Supervisor 判定 trip 到完成 PSTOP GPIO 安全写入的代码路径必须无总线访问、无动态分配、无格式化日志。 | A, UT |
| BMS-SR-012 | P0 | 只有经过 Safety Supervisor 授权的 SC service 可以释放 PSTOP。 | A, UT, IT |
| BMS-SR-013 | P0 | 任何非执行器模块直接控制 PSTOP、CE_N、CHG、DSG、PCHG 或 PDSG 均视为构建失败。 | A, CI |
| BMS-SR-014 | P0 | FET 状态转换不得使用会产生未请求中间态的通用 ALL_FETS_ON 序列。 | A, UT, OSC |
| BMS-SR-015 | P0 | 每次 BQ FET 命令必须具有超时、状态回读和不一致故障。 | UT, IT, HIL |
| BMS-SR-016 | P0 | 充电启动必须在 SC standby 完成配置、BQ 路径确认和第二次安全条件检查后，最后释放 PSTOP。 | UT, IT, OSC |
| BMS-SR-017 | P0 | 停止动作失败、超时或回读不一致时，系统必须保持 PSTOP 安全态并锁存执行器故障。 | UT, HIL |
| BMS-SR-018 | P1 | CHG、DSG、PCHG、PDSG、PSTOP、CE_N 的全部关键转换必须保存可关联固件版本的波形证据。 | OSC |

## 4. BQ76952 通信和配置

| ID | 优先级 | 需求 | 验证 |
|---|---|---|---|
| BMS-SR-020 | P0 | BQ76952 总线和 0x3E~0x61 间接访问窗口必须由单一服务所有。 | A, CI, IT |
| BMS-SR-021 | P0 | 间接访问的写地址、等待回显、读长度、读数据和校验必须作为不可分割事务执行。 | A, UT, IT |
| BMS-SR-022 | P0 | 所有 BQ I2C 操作必须有绝对超时；任何循环等待都必须具有最大次数和最坏时间。 | A, UT, HIL |
| BMS-SR-023 | P0 | 快照采样发生首次 transport failure 后，当前采样周期必须终止剩余低优先级读取并发布无效快照。 | UT, IT, HIL |
| BMS-SR-024 | P1 | BQ bus recovery 不得隐式改变产品 I2C 地址、CRC 模式或线序。 | A, UT |
| BMS-SR-025 | P0 | 所有 safety-class Data Memory 项必须在退出 ConfigUpdate 后逐项回读。 | UT, IT, HIL |
| BMS-SR-026 | P0 | 任一 safety-class 配置不匹配必须锁存 CONFIG_INVALID，并禁止释放功率路径。 | UT, IT, HIL |
| BMS-SR-027 | P1 | BQ 配置必须具有版本和可对外查询的配置指纹。 | UT, IT |
| BMS-SR-028 | P1 | BQ driver 不得调用 printf、业务状态机或动态内存。 | A, CI |

## 5. SC8815 通信和控制

| ID | 优先级 | 需求 | 验证 |
|---|---|---|---|
| BMS-SR-030 | P0 | SC 配置只能在 PSTOP 安全态下执行。 | A, UT, IT |
| BMS-SR-031 | P0 | 释放 PSTOP 前必须验证通信、输入、短路、过温、限流和关键寄存器状态。 | UT, IT, HIL |
| BMS-SR-032 | P0 | 运行期 ACK 失败不得自动交换 SDA/SCL 线序。 | A, UT, HIL |
| BMS-SR-033 | P1 | SC 总线操作必须有超时和显式总线恢复。 | UT, HIL |
| BMS-SR-034 | P1 | 运行中不得周期重写会扰动模拟控制环路的配置寄存器。 | A, IT |
| BMS-SR-035 | P0 | SC fault、通信失联或 permit 过期时必须撤销启动许可并保持 PSTOP 安全态。 | UT, IT, HIL |

## 6. 测量质量和保护决策

| ID | 优先级 | 需求 | 验证 |
|---|---|---|---|
| BMS-SR-040 | P0 | 所有保护输入必须携带 valid/stale/source/timestamp 信息。 | A, UT |
| BMS-SR-041 | P0 | 无效或陈旧单体电压不得被当作正常电压参与允许充放电结论。 | UT, IT |
| BMS-SR-042 | P0 | 双电芯温度传感器均失效时必须禁止充电和均衡；不得静默使用 IC 温度替代并继续充电。 | UT, HIL |
| BMS-SR-043 | P1 | 传感器失效、越界、陈旧和通信失联必须具有不同 fault ID。 | UT, IT |
| BMS-SR-044 | P0 | 单体映射必须由产品配置定义，并通过已知电压注入逐通道验证。 | A, HIL |
| BMS-SR-045 | P1 | 电流极性、采样电阻和增益必须通过双向已知负载标定。 | HIL |
| BMS-SR-046 | P0 | 快速保护阈值使用整数定点单位；SOC/SOH 浮点结果不得直接替代硬保护。 | A, CI |
| BMS-SR-047 | P1 | 每个安全测量必须定义最大允许数据年龄。 | A, UT |

## 7. 故障、锁存和恢复

| ID | 优先级 | 需求 | 验证 |
|---|---|---|---|
| BMS-SR-050 | P0 | 每个故障必须定义严重度、去抖、动作、恢复、锁存和遥测映射。 | A, UT |
| BMS-SR-051 | P0 | SCD、执行器不一致、配置无效和非法状态转移必须至少为 latched trip。 | UT, IT |
| BMS-SR-052 | P0 | fault clear 必须确认触发条件已消失，且不能由生产 CLI 直接清除。 | UT, IT, HIL |
| BMS-SR-053 | P1 | 故障首次发生和恢复边沿必须记录时间、快照序号和动作结果。 | UT, IT |
| BMS-SR-054 | P1 | 重复故障计数必须饱和，不能整数回绕。 | UT |
| BMS-SR-055 | P0 | 未知状态必须映射到安全动作，不得进入默认正常分支。 | UT |

## 8. Shutdown/Wake

| ID | 优先级 | 需求 | 验证 |
|---|---|---|---|
| BMS-SR-060 | P0 | 只有具备已确认 shutdown provenance 的离线状态才允许 SC wake-charge。 | UT, IT, HIL |
| BMS-SR-061 | P0 | 普通 BQ I2C 故障、配置故障或异常电芯采样不得触发 wake-charge。 | UT, HIL |
| BMS-SR-062 | P0 | wake-charge 必须有绝对超时；超时后停止 SC 并锁存唤醒失败。 | UT, HIL |
| BMS-SR-063 | P0 | BQ 恢复后必须重新验证设备身份、配置、告警和 FET 状态。 | UT, IT, HIL |
| BMS-SR-064 | P1 | shutdown 前 NVM flush 失败不得阻止安全关断，但必须记录。 | UT, HIL |

## 9. RTOS、时间和看门狗

| ID | 优先级 | 需求 | 验证 |
|---|---|---|---|
| BMS-SR-070 | P0 | 所有任务、队列和同步对象使用静态分配；调度器启动后不得依赖动态分配。 | A, CI |
| BMS-SR-071 | P0 | 开启 FreeRTOS stack overflow 检测和 malloc failure hook。 | A, IT |
| BMS-SR-072 | P0 | 只有 Safety Supervisor 在全部关键任务健康时允许刷新 IWDG。 | UT, IT, HIL |
| BMS-SR-073 | P0 | 任一关键任务 heartbeat 过期必须停止充电许可并最终触发 watchdog reset。 | UT, HIL |
| BMS-SR-074 | P1 | 每个周期任务记录 deadline miss 和最小栈余量。 | IT, HIL |
| BMS-SR-075 | P1 | 时间差计算必须正确处理 32 位 tick 回绕。 | UT |
| BMS-SR-076 | P0 | 安全任务不得被阻塞式 UART、EEPROM、CAN 或 OLED 调用阻塞。 | A, CI, IT |
| BMS-SR-077 | P1 | 最长全局关中断时间必须测量并满足预算。 | HIL, OSC |

## 10. 日志、CAN 和维护接口

| ID | 优先级 | 需求 | 验证 |
|---|---|---|---|
| BMS-SR-080 | P0 | driver 和 safety path 不得调用阻塞式 printf/HAL_UART_Transmit。 | A, CI |
| BMS-SR-081 | P1 | 日志使用有界缓冲；缓冲满时丢日志并累计 drop count。 | UT, IT |
| BMS-SR-082 | P0 | 生产构建不得包含直接启动充电、强开 FET 或绕过 supervisor 的命令。 | A, CI, binary scan |
| BMS-SR-083 | P1 | CAN 必须发送数据有效性和协议版本。 | UT, IT |
| BMS-SR-084 | P1 | CAN FIFO 满、bus-off 或接收洪泛不得反压安全任务。 | IT, HIL |
| BMS-SR-085 | P0 | 工程危险操作必须具备物理授权、软件解锁、状态检查、自动超时和审计。 | UT, IT |

## 11. SOC、SOH 和均衡

| ID | 优先级 | 需求 | 验证 |
|---|---|---|---|
| BMS-SR-090 | P1 | SOC 在电流无效或 interval 超限时不得继续无条件库仑积分。 | UT |
| BMS-SR-091 | P1 | SOC 满空锚点必须基于事件锁存和截止前有效条件，不得要求停止后持续同方向电流。 | UT, IT |
| BMS-SR-092 | P1 | SOC/SOH 输出必须同时提供有效性和置信度。 | UT |
| BMS-SR-093 | P1 | SOH 容量学习遇到复位、电流采样缺口或异常功率路径必须取消本轮学习。 | UT |
| BMS-SR-094 | P0 | 均衡必须要求有效电芯温度、有效单体电压、无安全故障和配置有效。 | UT, IT, HIL |
| BMS-SR-095 | P1 | 均衡 mask 必须遵守 BQ76952 的硬件组合限制并进行回读。 | UT, IT |

## 12. 持久化

| ID | 优先级 | 需求 | 验证 |
|---|---|---|---|
| BMS-SR-100 | P1 | 所有 NVM 记录包含 magic、version、length、sequence 和 CRC。 | UT |
| BMS-SR-101 | P1 | NVM 采用至少双槽或日志式提交，写入中掉电不得破坏上一有效记录。 | UT, HIL |
| BMS-SR-102 | P1 | 写后必须回读验证；失败不能更新内存中的 active slot。 | UT, HIL |
| BMS-SR-103 | P1 | 持久化使用显式序列化，不直接保存 C struct 内存。 | A, CI |
| BMS-SR-104 | P1 | EEPROM 离线不得阻塞安全动作或使系统错误释放功率路径。 | IT, HIL |

## 13. 构建、代码和追溯

| ID | 优先级 | 需求 | 验证 |
|---|---|---|---|
| BMS-SR-110 | P1 | HAL/CMSIS/vendor 生成目录必须由 CI 检查为未修改。 | CI |
| BMS-SR-111 | P1 | 自研代码 GCC Debug/Release 必须 warnings-as-errors。 | CI |
| BMS-SR-112 | P1 | domain 层不得依赖 HAL 或 FreeRTOS。 | A, CI |
| BMS-SR-113 | P1 | 公共安全 API 必须记录前置条件、后置条件、并发所有权、超时和失效动作。 | A, CI |
| BMS-SR-114 | P1 | 每个 P0/P1 需求必须链接到设计模块和测试 ID。 | A, CI |
| BMS-SR-115 | P1 | 发布时不得存在无追踪 ID 的 TODO、FIXME 或临时旁路。 | CI |
| BMS-SR-116 | P2 | 固件必须报告 Flash/RAM 使用和各任务栈预算。 | CI, HIL |
| BMS-SR-117 | P1 | main 只能通过审核 PR 合并，禁止未经门禁的直接发布。 | Process |

## 14. 初始时序预算

以下是软件设计目标，不替代 BQ 自主硬保护。最终值以 HIL 测量冻结。

| 项目 | 初始目标 |
|---|---:|
| Safety Task 周期 | 10 ms |
| Safety Task 正常 WCET | <= 2 ms |
| 软件 trip 判定后 PSTOP GPIO 写入 | 同一调用路径，目标 < 1 ms |
| SC 中断到 service 处理 | <= 20 ms |
| BQ 快速快照最大年龄 | 300 ms |
| BQ 慢速快照最大年龄 | 2000 ms |
| BQ 单次 direct I2C 事务 | 有界，目标 <= 5 ms |
| BQ 间接事务 | 有界，目标 <= 20 ms，按命令分类 |
| Watchdog nominal timeout | 约 500 ms，按 LSI 实测冻结 |
| 关键任务 heartbeat 最大年龄 | 200 ms |
| Production 安全日志提交 | 非阻塞，固定时间 |

## 15. 发布阻断条件

出现以下任一情况，评分不能达到 100：

- 任一 P0/P1 需求无验证证据；
- 任一执行器路径可被绕过；
- FET 转换无波形；
- BQ 配置无全量回读；
- 无 watchdog/heartbeat；
- 有阻塞日志进入安全任务；
- 有运行期动态内存；
- 有危险生产 CLI；
- 温度传感器失效仍允许充电；
- 普通通信故障可进入 wake-charge；
- 单元测试或 CI 不通过；
- HAL 生成目录被业务改写；
- 存在未关闭 P0/P1 issue。
