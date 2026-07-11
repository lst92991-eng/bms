# BQ76952 事务阻塞问题分析

创建日期：2026-07-12  
对应审查项：011  
当前决策：先测量和解释，不在充电测试前直接重构。

## 1. 问题是什么

`batman_task` 的优先级为 3，高于 `sc8815_task` 的 2 和 `debug_cli_task` 的 1。BQ 驱动当前使用同步 HAL I2C、`HAL_Delay()` 和轮询等待。调用期间 BQ 任务没有进入 FreeRTOS Blocked 状态，而是一直保持为最高优先级 Ready/Running 任务。

因此，即使系统 tick 和中断仍能发生，调度器每次仍会选择最高优先级的 `batman_task` 继续运行。SC8815 任务和 CLI 任务要等 BQ 调用返回后才能获得 CPU。

这不是“死锁”，而是高优先级任务的同步阻塞导致低优先级任务延迟。

## 2. 当前代码中的阻塞来源

### 2.1 同步 I2C timeout

`Int_BQ76952` 的 HAL I2C timeout 为 `100ms`。一次异常传输可能在返回前占用接近该超时时间。

### 2.2 subcommand 固定等待

读取 subcommand 或 Data Memory 时，写入命令后执行 `HAL_Delay(2ms)`，随后读取 transfer buffer。

### 2.3 echo 轮询

`Int_BQ76952_WaitEcho()` 最多轮询 100 次，每次 echo 不匹配后 `HAL_Delay(1ms)`。

需要注意：如果 I2C 直接返回 HAL error，函数会在第一次失败时退出，并不是必然出现 `100 × 100ms`。真正的长轮询场景是 I2C 读成功、但 BQ echo 长时间不匹配。

### 2.4 ConfigUpdate 轮询

进入和退出 ConfigUpdate 最多各轮询 100 次，每次状态位未达到目标时等待 1ms。正常情况下通常很快；若状态位长期不变化，则可能占用约百毫秒量级，再叠加每次 I2C 读取时间。

### 2.5 运行期重新配置

初始化时的长操作发生在调度器启动前，对任务调度没有影响。更需要关注的是 `App_BatMan_RecoverAfterWake()`：BQ shutdown 唤醒后会在运行期重新进入 ConfigUpdate，并重写整套 Data Memory 基线。该路径包含多次同步 I2C、transfer、echo 和 ConfigUpdate 轮询，可能明显延迟 SC8815 任务。

## 3. 可能出现的现象

- SC8815 INT 已触发，但 `sc8815_task` 延迟读取 STATUS。
- SC fault、OTP 或 VBUS short 的软件可见时间晚于预期。
- CSV 输出出现时间间隔变长或缺口。
- `App_Power_Task()` 的本周期执行时间拉长。
- BQ shutdown 唤醒恢复期间，SC 请求和状态刷新看起来停顿。
- 严重通信异常时，1 秒任务周期可能被穿透。

## 4. 为什么当前不立即修改

- 完整放电测试已经通过当前阶段验证。
- 充电测试前大改 BQ 事务模型可能引入新的寄存器时序或功率链路回归。
- 当前缺少实际执行时间证据，无法判断是正常的几十毫秒开销，还是已经接近控制周期。
- Debug CLI 后期会删除，部分并发面会自然缩小。

因此本轮先测量，再决定是否需要异步化或降优先级处理。

## 5. 明天应如何测量

### 5.1 软件时间戳

在以下入口记录开始/结束 tick：

- `App_BatMan_Task()`
- `App_BatMan_RecoverAfterWake()`
- `Int_BQ76952_ReadSubcommand()`
- `Int_BQ76952_EnterConfigUpdate()`
- `Int_BQ76952_ExitConfigUpdate()`

记录：调用名称、开始 tick、结束 tick、耗时、返回值、HAL error。

### 5.2 GPIO 示波

选一个不影响硬件的调试 GPIO：

- BQ 任务进入时拉高，退出时拉低。
- SC INT ISR 产生另一个脉冲。
- SC 任务开始读取状态时产生第三个脉冲。

逻辑分析仪可直接量出：

```text
SC INT 触发
→ BQ 高优先级事务结束
→ SC 任务开始处理
```

这比仅看串口日志更可靠，因为串口本身也可能阻塞。

### 5.3 测试场景

1. 正常周期采样。
2. BQ 地址无响应或临时断开 I2C。
3. echo 不匹配/transfer 失败。
4. ConfigUpdate 进入或退出超时。
5. BQ shutdown 后插入充电器并执行恢复重配。
6. 充电期间触发 SC INT，同时执行 BQ 周期采样。

## 6. 判断标准

当前没有正式安全响应预算，所以先记录分布：

- 正常 BQ 周期耗时：min / average / max。
- 单次 direct read、subcommand 和 Data Memory 事务耗时。
- shutdown 恢复重配总耗时。
- SC INT 到 `sc8815_task` 开始处理的延迟。
- CSV 实际帧间隔。

出现以下任一情况，应进入代码整改：

- BQ 任务执行时间接近或超过 1 秒周期。
- SC INT 到任务处理的延迟达到不可接受水平。
- 充电曲线出现与 BQ 事务同步的明显采样断点。
- BQ 异常导致 SC 状态机长时间无法运行。
- shutdown 恢复重配期间无法及时撤销 SC 充电请求。

## 7. 后续可能的改法

按侵入程度从低到高：

1. 把 `HAL_Delay()` 改为可让出 CPU 的任务等待，但必须保证 BQ 事务不会被其他 BQ 调用插入。
2. 缩短和分类 timeout，区分正常 direct read 与配置事务。
3. 将恢复重配拆成分步状态机，每个周期执行有限数量事务。
4. 建立单一 BQ service task，其他模块通过请求/响应访问 BQ。
5. 对必须连续的 subcommand/transfer 事务加互斥和事务所有权。

本项目当前只要求完成测量。没有实测超预算证据时，不在充电测试前实施第 3～5 类重构。
