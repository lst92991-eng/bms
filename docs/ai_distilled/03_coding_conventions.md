# 03 Coding Conventions

更新日期：2026-08-22

这些规则来自当前仓库的重复实现和安全约束；不是仅凭通用嵌入式经验生成。若局部旧代码与本规范冲突，以新的安全架构与同层多数写法为准。

## Coding Convention Inference

### 1. 分层与命名

| 规则 | 证据 | 信心 | 人工确认 |
| --- | --- | --- | --- |
| 业务行为放 `App_*`，芯片/板级访问放 `Int_*`，纯算法放 `Com_*` | `CMakeLists.txt:39-118`；`App/App_Main.c:215-267` | High | Not needed |
| 模块使用配对 `.c/.h`，公开符号以模块名作前缀 | `App/App_Safety.c` + `.h`；`Int/Int_BQ76952.c` + `.h` | High | Not needed |
| 生成文件只做初始化/IRQ 转发，不放策略状态机 | `bms24v_platform/Core/Src/main.c:83-114`；`bms24v_platform/Core/Src/stm32g0xx_it.c:204-212` | High | Not needed |
| 类型优先 `stdint.h` 固定宽度、`bool`、具名 enum/struct；物理量在名称标单位 | `App/App_Power.h:7-69`；`App/App_Safety.h:12-65`；`Com/Com_SOC.h:92-119` | High | Not needed |

### 2. 文件与函数结构

- 每个模块只拥有一类状态；模块内部状态 `static`，通过窄 API 或 snapshot 暴露。证据：`App/App_SC8815.c:20-47`；`Int/Int_Log.c:11-24`。
- `main.c` 只做生成初始化、早期安全序列和 `App_Main(early_safe_evidence)` 入口；HAL 前先直接建立 SC safe GPIO，时钟后固定 GPIO→SC standby→I2C1→BQ all-off，且 BQ 序列必须早于日志/其余外设。证据：`bms24v_platform/Core/Src/main.c:83-128`；`Int/Int_SC8815.c:21-46`。
- 复杂模块按 facade/config/sample/NVM/debug 拆分，避免 BQ 单文件承载全部业务。证据：`App/App_BatMan.c:12-18`；`App/App_BatMan_Config.c`；`App/App_BatMan_Sample.c`；`App/App_BatMan_Nvm.c`。
- 安全相关函数应早返回、失败即安全态；所有 enable 路径需要显式授权，disable 路径不得依赖缓存命中。证据：`App/App_Power.c:208-255`；`App/App_SC8815.c:512-583`。

信心：High。人工确认：Not needed。

### 3. 注释规范

注释解释“为什么、硬件语义、时序/并发/单位、安全意图”，不复述语句本身。

推荐：

```c
/* PSTOP 必须先进入安全态，再修改限流寄存器，避免中间配置驱动功率级。 */
```

公共 API 使用 Doxygen 风格，至少说明：

- `@brief`：动作与安全语义；
- `@param`：单位、范围、active level、所有权；
- `@return`：失败含义和调用者必须采取的动作；
- 并发约束：task-only/ISR-safe、是否需要 transaction、有效期/epoch；
- 硬件约束：寄存器字段、换算、等待时间来自何处。

证据：`App/App_BatMan.h:151-177`；`App/App_Safety.h:21-35,85-94`；`Int/Int_Fault.h:49-63`。
信心：High。人工确认：Not needed。

禁止：

- 把 FreeRTOS 任务写成“主循环”；
- 把 active-high PSTOP 写成“拉低急停”；
- 把候选寄存器值写成“已标定”；
- 用注释承诺测试未证明的 HIL 行为；
- 保留失效、与代码相反或只描述历史实现的注释。

### 4. 错误处理与安全态

| 场景 | 规则 | 证据 |
| --- | --- | --- |
| HAL/I2C/寄存器访问 | 检查返回值；使用有限 timeout；剩余预算不足立即失败 | `Int/Int_BQ76952.c:87-240`；`Int/Int_SC8815.c:545-617` |
| 多步采样 | staging；任一步失败拒绝整帧；保留错误与 age，不混合新旧字段；采样层不先修改配置证明 | `App/App_BatMan_Sample.c:352-530` |
| 多步配置 | manifest 写入后逐项读回；首错停止；mismatch 置 config invalid | `App/App_BatMan_Config.c:204-295` |
| 运行期 BQ 证明 | 采样只发布原始指纹；Safety facade 先停 SC，再置 recovery required、撤 ready，最后 best-effort BQ 全关；恢复必须在 1500 ms 整体 transaction 内完成重认证且等下一完整帧 | `App/App_BatMan_Sample.c:281-336,411-472`；`App/App_BatMan.c:294-346,673-684`；`App/App_Safety.c:67-93`；`App/App_BatMan_Config.c:75-78,666-811`；`App/App_Power.c:586-657` |
| 使能功率 | BQ 主 FET 与 SC start 携带同一 epoch；BQ owner 前/中/后、SC PSTOP release 前及 Power 总提交后均复验；SC start 失败同周期回滚 BQ 全关 | `App/App_Power.c:206-290`；`App/App_BatMan_Config.c:463-547`；`App/App_SC8815.c:106-312` |
| 致命异常 | 先 ForceStandby，再记录 fault，最后 reset | `Int/Int_Fault.c:176-215` |
| 日志拥塞 | 应用只用 `Int_Log_Printf`；固定 384 B 栈缓冲由无堆 `Com_FormatV` 限界格式化后投递静态 ring；截断或容量不足可计数，不等待 UART | `Com/Com_Format.c:13-269`；`Int/Int_Log.h:16-41`；`Int/Int_Log.c:11-24,47-173,175-231` |

信心：High。人工确认：Needed，故障注入验证。

### 5. 并发规则

- 任务必须静态创建；禁止运行期 `pvPortMalloc`/`xTaskCreate`。证据：`App/App_Main.c:51-78,269-377`；`bms24v_platform/MDK-ARM/FreeRTOS/include/FreeRTOSConfig.h:40-49`。
- 周期任务使用绝对唤醒时刻和 catch-up 上限；所有时间统一通过 `pdMS_TO_TICKS()` 或明确 tick 类型。证据：`App/App_Main.c:25-49,86-105`。
- ISR 只做确定性安全动作、latch/sequence/event；禁止 I2C、`Int_Log_Printf`、长循环和阻塞等待。证据：`bms24v_platform/Core/Src/stm32g0xx_it.c:204-212`；`Int/Int_BQ76952.c:585-631`；`Int/Int_SC8815.c:743-770`。
- 跨任务外设共享使用静态 mutex 和有界锁等待；BQ 多步操作使用递归 transaction + 不可延长的绝对截止时间。证据：`Int/Int_BQ76952.c:48-61,131-240,644-659`；`Int/Int_I2C2Bus.c:7-61`。
- snapshot 更新放在短临界区；总线传输不能由全局 PRIMASK/暂停调度包围。SC_EVENT 的 sequence 复验、inhibit clear 与 owner 状态转换必须共用一个 IRQ-disabled 短临界区；sequence 变化则保持急停。NVM 可短暂停止任务切换抓取 SOC+SOH 内存快照，但恢复调度后才能做 EEPROM I/O。证据：`App/App_SC8815.c:313-505`；`Int/Int_SC8815.c:107-187,557-637,743-770`；`App/App_BatMan_Nvm.c:128-175,726-894`。
- Runtime 单写者必须清楚：Power 是业务授权者，BatMan 是 BQ owner，SC task 是 SC runtime 写者，Safety 是 IWDG 刷新者。critical BQ ALERT 必须在同一 BatMan owner 周期锁 Safety 并全关 BQ FET；清除 inhibit 前必须在关中断区复验单调 ALERT sequence 与 active-low pin。证据：`App/App_BatMan.c:191-293`；`Int/Int_BQ76952.c:585-635`；`App/App_SC8815.c:10-13`；`App/App_Safety.c:469-560`。

### 6. 状态与可观测性

关键控制必须区分：

- `desired`：策略希望的状态；
- `commanded`：已发给硬件的状态；
- `observed`：寄存器/引脚回读状态；
- `provenance`：谁、基于何种证据发起；
- `sequence/age/valid`：数据是否完整、新鲜、可消费。

证据：`App/App_BatMan.h:57-73`；`App/App_SC8815.h:7-36`；`App/App_Power.h:45-69`；`App/App_BatMan_Sample.c:505-530`。
信心：High。人工确认：Not needed。

### 7. 常量与单位

- 阈值和 timeout 使用 `enum`/`static const`/模块宏集中定义，不散落 magic number。
- 名称携带 `_MV`、`_MA`、`_MS`、`_CDEG`、`_TICKS` 等单位；换算使用足够宽的中间类型并检查饱和/范围。
- 硬件配置常量必须有数据表/BOM/实测来源；无法确认写 `Unknown`，不得把候选默认值升级为事实。

证据：`App/App_Power.c:30-60`；`Int/Int_BQ76952.c:24-38`；`Int/Int_SC8815_BSP.h:21-31,132-161`。
信心：High。人工确认：Needed，参数评审。

### 8. 构建与提交

- GCC 使用 `-Wall -Wextra -Werror`；Release 关闭工程 CLI，Engineering 明确开启。证据：`CMakeLists.txt:9-15,119-138`。
- 所有构建必须经过 Flash/RAM 预算；当前阈值 120 KiB Flash、132 KiB RAM。证据：`CMakeLists.txt:9-15,140-176`。
- 修改 App/Com/Int 后同步 CMake 与 Keil 文件清单并运行 host tests；不能只验证一个工具链元数据。
- 禁止提交 `logs/`、build、map、axf/elf/hex/bin、对象、pack/cache、`.log.lock` 或临时测试产物；tracked path 必须通过 fail-closed repo hygiene CTest。证据：`.gitignore:1-41`；`tests/host/check_repo_hygiene.py:11-105`；`AGENTS.md`。

## Forbidden Patterns

1. `HAL_Delay()` 出现在周期业务任务或 ISR；驱动内芯片规定等待也必须消耗同一绝对预算。
2. enable 动作由 CLI、CAN 或显示模块直接调用底层 actuator。
3. 使用“读旧缓存相同则返回”跳过安全关闭命令。
4. 分步更新共享采样结构，让消费者看到混合帧。
5. 整笔软件 I2C 事务关闭全局中断。
6. 未读回就宣布配置成功。
7. 由任意任务直接刷新 IWDG。
8. 为消除告警而清除锁存硬件保护。
9. 在 Release 中保留写功率路径的调试命令。
10. 把主机测试当作实板保护、波形、温升和 EMC 证据。
11. 在 BQ 通信恢复后沿用启动期 `config valid` 缓存，或把一次 ACK 当作 POR 后配置仍有效的证明。
12. 应用模块直接调用 libc `printf/fprintf/snprintf`；格式化日志统一走 `Int_Log_Printf + Com_FormatV`，`retarget` 只作源码兼容层，Release 链接后必须通过 heap/stdio symbol gate。
