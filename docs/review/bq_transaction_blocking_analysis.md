# BQ76952 事务阻塞与运行期重认证分析

创建日期：2026-07-12
更新日期：2026-08-22
对应审查项：011
当前结论：单笔事务无界阻塞和“通信恢复即沿用旧配置”的软件缺口已整改；整条重认证 WCET 与真实故障到安全输出延迟仍为 `HIL blocked`。

## 1. 历史问题边界

原审查指出：`batman_task` 优先级高于 SC/CAN/维护任务，若同步 BQ I2C、echo 或 ConfigUpdate 轮询没有统一预算，低优先级任务可能长期得不到运行；BQ shutdown/复位后若直接沿用 MCU 侧 `config valid` 缓存，还可能在 BQ RAM 配置已丢失时错误恢复功率。

当前实现仍使用同步 BQ API，但已增加静态 recursive mutex、绝对 deadline、运行期配置指纹失效和全关重认证；当前公开恢复 API 是 `App_BatMan_ReauthenticateAfterReset()`。【事实】

- 证据：`Int/Int_BQ76952.c:24-38,48-61,103-240`；`App/App_BatMan_Config.c:666-811`
- 信心：High
- 人工确认：Not needed

## 2. 当前事务边界

### 2.1 单笔 I2C 与 transaction deadline

每次 HAL I2C 最多使用 10 ms，且 timeout 还会被外层剩余预算截短。direct、command、indirect 和 ConfigUpdate 的默认绝对预算分别为 12/30/50/150 ms；嵌套调用只增加 recursive depth，不延长最外层 deadline。调度器运行时，协议等待使用 `vTaskDelay()`；启动前才使用 `HAL_Delay()`。【事实】

- 证据：`Int/Int_BQ76952.c:24-38,103-240`
- 信心：High
- 人工确认：Needed，NACK/SDA stuck 与调度 trace

### 2.2 echo 与 ConfigUpdate 轮询

echo、进入 ConfigUpdate 和退出 ConfigUpdate 都以“剩余绝对时间”为循环条件；底层错误首错返回，状态不收敛则在预算耗尽时返回 timeout。旧文档中的“固定 100 次 × 100 ms”不再描述当前实现。【事实】

- 证据：`Int/Int_BQ76952.c:341-366,1105-1195`
- 信心：High
- 人工确认：Needed，异常设备/总线 HIL

### 2.3 完整采样帧

BatMan 用 100 ms 外层 transaction 读取完整 staging frame；任一字段或预算失败即拒绝整帧，不把新旧字段混合发布。【事实】

- 证据：`App/App_BatMan_Sample.c:10-20,462-490`；`Int/Int_BQ76952.c:636-659`
- 信心：High
- 人工确认：Needed，最坏总线故障 WCET

## 3. 故障发生时的硬件动作顺序

完整帧失败，或完整帧观察到 `BatteryStatus.POR/CFGUPDATE`、`ManufacturingStatus.FET_EN=0` 时，配置状态立即转为 `CONFIG_RECOVERY_REQUIRED`。`App_BatMan_EnforceRuntimeProof()` 的失败顺序是：

1. `App_SC8815_EmergencyStop()`，第一硬件动作直接保持 PSTOP 安全；
2. `App_Safety_ReportBqReady(false)`，撤销 authorization epoch；
3. `App_BatMan_KeepMainFetsOff()`，有界 best-effort 请求并回读 BQ 四路主 FET 全关。

Safety 新增任一 inhibit 时也先调用直接 PSTOP，再更新软件 inhibit/epoch。Power 观察 BQ offline 时每个任务周期都会再次执行同步全关，而不是只清 `allowed` 标志。【事实】

- 证据：`App/App_BatMan_Sample.c:281-336,411-472`；`App/App_BatMan.c:306-346,673-684`；`App/App_Safety.c:67-93`；`App/App_Power.c:586-657`
- 信心：High
- 人工确认：Needed，fault-to-PSTOP 与 BQ gate 波形

这条顺序保证慢 BQ I2C 之前先停 SC 功率级；但当前 MCU 没有独立 BQ 主 FET gate-off 脚，BQ 总线完全失效时，`KeepMainFetsOff()` 只能失败并依赖 BQ 自主保护/IWDG/硬件安全目标。【事实 + 硬件上限】

## 4. `App_BatMan_ReauthenticateAfterReset()` 完整屏障

通信恢复或合法 shutdown wake 后不会直接恢复授权。当前重认证按以下顺序执行：

1. 标记 `CONFIG_RECOVERY_REQUIRED`；
2. `ALL_FETS_OFF` 并确认四路 FET 全关；
3. 精确核对 BQ76952 DeviceNumber；
4. 进入 ConfigUpdate，完整重写 Data Memory manifest，再退出；
5. 逐项完整读回 manifest；
6. 禁止 sleep、清启动告警；
7. 在 all-off blocker 下安全建立 `FET_EN`，并再次确认全关；
8. 终检 AlarmRaw、BatteryStatus、ManufacturingStatus、FET_STATUS；POR/CFGUPDATE/FET_EN/告警/任一 gate 不符即失败并保持 recovery required；
9. 重认证成功后仍等待下一完整有效 BatMan frame，才允许重新报告 ready。

- 证据：`App/App_BatMan_Config.c:666-811`；`App/App_Power.c:316-467`
- 信心：High
- 人工确认：Needed，BQ brownout/重连/中途再次断线 HIL

## 5. 仍未关闭的时延风险

`App_BatMan_ReauthenticateAfterReset()` 先建立 1500 ms 外层 transaction，再在同一 recursive owner/deadline 内执行全部身份、ConfigUpdate、manifest、FET 和终检步骤；嵌套 API 不能延长外层 deadline，也不能被其他 BQ 公开 API 插入。该预算小于 BatMan/Power 的 2500 ms 监督 deadline。【事实】

这证明整条重认证会在 1500 ms 内成功或失败，不证明正常/异常板上的实际 WCET、低优先级 SC/CAN/维护任务 jitter 已满足产品要求。重认证之前 SC 已硬件 standby、Safety ready 已撤销，最高优先级 Safety 仍可运行；实际分布与 1500 ms 上限是否符合系统安全预算仍需 HIL/设计评审。【事实 + Unknown】

- 证据：`App/App_BatMan_Config.c:75-78,666-811`；`Int/Int_BQ76952.c:130-240`；`App/App_Main.c:25-49,107-164`；`App/App_Safety.c:469-560`
- 信心：High
- 人工确认：Needed

## 6. 必须归档的 HIL

| 场景 | 观测量 | 最低通过条件 |
| --- | --- | --- |
| BQ NACK/SDA/SCL stuck | SCL/SDA、PSTOP、CHG/DSG/PCHG/PDSG VGS、task trace | PSTOP 第一动作；事务在预算内返回；无无限补跑/死锁 |
| 运行期 BQ brownout/POR | POR/CFGUPDATE/FET_EN、config state、Safety epoch、gate | 立即 recovery required/撤权/全关；旧配置缓存不被复用 |
| 重认证中途再次断线 | transaction result、PSTOP、四路 gate | 任一步失败保持 standby/all-off；不会提前 ready |
| 完整重认证 | 各阶段 timestamp、manifest expected/actual、frame sequence | 1500 ms 外层 deadline 内收敛；终检通过后仍等下一完整帧；实际 WCET 满足批准预算 |
| SC INT 与 BQ 重认证并发 | SC INT、PSTOP、任务切换、BQ I2C | INT/任一 inhibit 立即 PSTOP；低优先级 jitter 有量化上限 |

## 7. 结论

- `Software closed`：单笔 HAL/transaction 有界、1500 ms 整体重认证 deadline、recursive ownership、首错拒帧、配置证明失效、PSTOP-first、全关屏障和下一完整帧门禁。
- `HIL blocked`：重认证实际 WCET/1500 ms 预算适用性、真实总线故障时 fault-to-PSTOP/BQ gate、任务 jitter 和 brownout/reconnect 行为。
- `Hardware limit`：无独立 MCU 主 FET gate-off，软件全关命令不能替代独立硬件切断链。

因此该项已从“先测量、暂不整改”更新为“软件架构整改完成，等待冻结 commit 的 HIL 量化”。
