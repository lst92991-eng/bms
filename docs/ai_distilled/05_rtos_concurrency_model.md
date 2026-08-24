# 05 RTOS / Concurrency Model

更新日期：2026-08-22

## RTOS/Concurrency Model

当前运行时是抢占式、时间片开启的 FreeRTOS，最大优先级数 5。任务和 RTOS 内核对象使用静态存储；`configSUPPORT_DYNAMIC_ALLOCATION=0`，CMake/Keil 源清单没有 `heap_4.c`。【事实】

- 证据：`bms24v_platform/MDK-ARM/FreeRTOS/include/FreeRTOSConfig.h:19-49`；`App/App_Main.c:51-78,269-377`；`CMakeLists.txt:39-118`
- 信心：High
- 人工确认：Not needed

## 任务表

| 任务 | 周期/触发 | 优先级 | 静态栈 words | deadline/监督 | 主要职责 | 证据 |
| --- | ---: | ---: | ---: | --- | --- | --- |
| Safety | 10 ms | 4 | 320 | 自身 supervisor；3 s grace，IWDG nominal 6 s | inhibit、deadline、stack、ready、wake epoch、唯一喂狗 | `App/App_Main.c:25-49,271-277`；`App/App_Safety.c:9-16,469-560` |
| BatMan + Power | 1000 ms（同一任务顺序执行） | 3 | 1024 | BatMan 2500 ms + Power 2500 ms，分别 heartbeat | BQ 完整帧、保护/ALERT、运行期配置 proof、算法更新，随后 Power/offline 全关/重认证 step | `App/App_Main.c:25-49,107-127,278-284,336-343`；`App/App_BatMan.c:673-684`；`App/App_Power.c:586-657` |
| SC8815 | 20 ms | 2 | 512 | 100 ms | SC single-writer、INT 处理、staged sample | `App/App_Main.c:25-49,130-143,285-291` |
| CAN | 20 ms | 2 | 512 | 150 ms | RX budget、snapshot TX、event/recovery | `App/App_Main.c:25-49,145-164,292-298` |
| Maintenance | 250 ms | 1 | 768 | 非关键任务 | BatMan NVM/debug maintenance，再 OLED；串行使用 I2C2 | `App/App_Main.c:25-49,181-200,299-305` |
| Buzzer | 10 ms | 1 | 256 | 非关键任务 | 蜂鸣器节拍 | `App/App_Main.c:25-49,202-213,306-312` |
| CLI | Engineering build；阻塞等输入但受任务自身周期/驱动约束 | 1 | 512 | 非关键任务 | 物理 gate + unlock 的维护命令 | `App/App_Main.c:51-78,166-179,313-320`；`App/App_DebugCli.c:446-496` |

周期任务使用 `vTaskDelayUntil()` 语义的绝对 next-release；若已迟到，只记录 overrun 并把下一释放点移到未来，不执行无界 catch-up。【事实】

- 证据：`App/App_Main.c:86-105`
- 信心：High
- 人工确认：Not needed

## 关键任务监督

Safety 注册 BatMan、Power、SC、CAN 四个 critical health slot；BatMan 与 Power 共用同一个任务句柄，但分别在各自工作完成后 heartbeat。Safety 校验 deadline 和 stack high-watermark。BQ/SC 未 ready 或硬件保护存在时，Safety gate 不允许 power authorization；只有全部健康才刷新 IWDG。【事实】

- 证据：`App/App_Safety.h:12-19,44-65`；`App/App_Main.c:107-176,335-351`；`App/App_Safety.c:155-211,334-465,469-560`
- 信心：High
- 人工确认：Needed，任务挂起/stack fault/IWDG HIL

## 共享资源与所有权

| 资源 | 并发策略 | 超时/失败语义 | 证据 | 残余验证 |
| --- | --- | --- | --- | --- |
| BQ I2C1 | 静态 recursive mutex；outer transaction 建立绝对 deadline；嵌套不可延长 | HAL timeout = min(10 ms, remaining)；锁/预算失败返回错误，拒绝帧/配置 | `Int/Int_BQ76952.c:48-61,87-240,644-659` | I2C stuck、NACK、priority inversion HIL |
| SC 软件 I2C / SC_EVENT | `App_SC8815` single-writer；短 GPIO 原子段；整笔传输不全局关 IRQ；单调 IRQ sequence + 2-clean 后原子复验/clear | bounded stretch、9 clock recovery；失败或序号变化保持 inhibit/PSTOP | `App/App_SC8815.c:20-26,435-505`；`Int/Int_SC8815.c:107-187,557-637,743-770` | 逻辑分析仪确认 stretch/recovery/竞态 |
| I2C2 OLED/EEPROM | bus 层静态 recursive mutex 串行 OLED/EEPROM；NVM 层另有静态 recursive mutex，保护 slot/sequence/重连/flush 整体状态 | lock/transfer 失败上报或跳过本轮；不影响 Safety task 运行 | `Int/Int_I2C2Bus.c:7-61`；`App/App_BatMan_Nvm.c:49-125,713-894`；`App/App_Main.c:181-200` | 总线卡死、竞争和掉电最坏耗时 |
| UART TX | `Int_Log_Printf` 固定 384 B 缓冲 + `Com_FormatV` -> 1024 B ring -> `HAL_UART_Transmit_IT`；producer 不等待 | 超长截断；容量不足整块丢弃并计数；post-link gate 拒绝 heap/stdio | `Com/Com_Format.c:13-269`；`Int/Int_Log.h:16-41`；`Int/Int_Log.c:11-24,47-173,175-231`；`tests/host/check_forbidden_symbols.py:10-50` | HAL_BUSY/ERROR 注入与 jitter HIL |
| BatMan/SC/CAN 快照 | staging 后短临界区原子发布；消费者复制 snapshot | `valid/sequence/age` 区分陈旧数据 | `App/App_BatMan_Sample.c:352-472,505-530`；`App/App_SC8815.c:313-412`；`App/App_CanBms.c:186-244` | TSAN 不适用 MCU；结构/压力测试 |
| SOC/SOH NVM 快照 | NVM mutex 序列化持久化状态；只在导出/恢复 RAM 状态时短暂停止任务切换，恢复后才执行 EEPROM I/O | 同一快照内 SOC/SOH/provenance 一致；scheduler pause 不包围慢总线 | `App/App_BatMan_Nvm.c:49-175,713-894` | 任务竞争、WCET、写中掉电 HIL |
| Power actuator | Power 唯一业务授权；BQ/SC owner 同用一个 Safety epoch；BQ 前/中/后、SC release 前和 Power 总提交后复验 | 任一 fault/inhibit 撤销旧 epoch；SC start 失败在同周期 PSTOP + BQ all-off；critical ALERT owner 周期锁存并全关；清除 ALERT inhibit 前原子复验 sequence/pin | `App/App_Safety.c:67-117,301-465`；`App/App_BatMan_Config.c:463-547`；`App/App_BatMan.c:191-293`；`Int/Int_BQ76952.c:585-635`；`App/App_SC8815.c:106-312`；`App/App_Power.c:206-290` | 注入检查-执行竞态 HIL |
| BQ runtime proof | frame transport/protocol 失败，或 POR/CFGUPDATE/FET_EN 指纹异常时先 SC emergency stop，再 mutation config proof/ready，最后 best-effort BQ all-off；任一 Safety inhibit 也先 PSTOP；offline 每周期继续 all-off | 重在线必须在 1500 ms 外层 transaction 的 all-off 屏障下完整重写/验证 manifest，并等下一完整帧才 ready | `App/App_BatMan_Sample.c:281-336,411-472`；`App/App_BatMan.c:294-346,673-684`；`App/App_Safety.c:67-93`；`App/App_BatMan_Config.c:75-78,666-811`；`App/App_Power.c:316-467,586-657` | 断线/复位/恢复并发与 gate HIL |

## ISR 模型

- EXTI：SC/BQ 分支均先 `Int_SC8815_ForceStandby()`；随后只 latch/通知。
- BQ ALERT：ISR 只更新 pending/sequence，不进行 I2C。
- HardFault：汇编 trampoline 传递异常栈；C 路径先 ForceStandby、记录有效 PC/LR、复位。
- UART：Tx complete/error callback 只推进 ring 状态。

证据：`bms24v_platform/Core/Src/stm32g0xx_it.c:69-79,132-142,204-212`；`Int/Int_BQ76952.c:585-631`；`Int/Int_SC8815.c:743-770`；`Int/Int_Fault.c:93-122,176-215`；`Int/Int_Log.c:191-231`。
信心：High。人工确认：Needed，IRQ latency/priority HIL。

## 优先级与反转分析

Safety 最高优先级且不访问 BQ/I2C2 慢总线，因此可以监督被总线阻塞的业务任务。BQ、I2C2 和 NVM recursive mutex 由 FreeRTOS mutex 提供 priority inheritance；BatMan 以高于 SC/CAN 的优先级运行，但 BQ transaction 有绝对预算。NVM 仅在复制/恢复内存状态时暂停任务切换，EEPROM 操作发生在恢复调度之后，避免把慢器件时延扩大为全系统调度停顿。【事实 + 推断】

- 证据：`App/App_Main.c:269-351`；`bms24v_platform/MDK-ARM/FreeRTOS/include/FreeRTOSConfig.h:31-39`；`Int/Int_BQ76952.c:130-240`；`App/App_BatMan_Nvm.c:49-175,713-894`
- 信心：High
- 人工确认：Needed，实际 WCET/调度 trace

## Unknowns / HIL Gates

1. 各任务真实 WCET、stack watermark 和 99.9% 延迟分布：Unknown。
2. BQ 总线 NACK/stuck 时 BatMan deadline 与 IWDG 的实测关系：Unknown。
3. SC 软件 I2C 在 CAN/UART/EXTI 干扰下的高低电平、stretch/recovery，以及新 SC IRQ 恰落在 2-clean resolve 窗口时的 PSTOP/inhibit 时序：Unknown。
4. I2C2 OLED 全刷、NVM mutex 竞争和 EEPROM 写周期的最坏持锁/WCET：Unknown；源码只证明任务切换不会跨 EEPROM I/O 被暂停。
5. HardFault、断言、栈溢出时，从 fault 到 PSTOP/CHG/DSG 实际波形：Unknown。

这些 Unknown 不能由 host tests 或源码审阅替代。
