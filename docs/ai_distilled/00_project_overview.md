# 00 Project Overview

更新日期：2026-08-22
证据基线：当前工作区源码；HAL/CMSIS/CubeMX 生成代码仅作为配置证据，不作为本轮重构对象。

## 总体结论

当前仓库已经形成“静态 FreeRTOS 任务 + 单一功率授权者 + 独立安全监督 + 受限驱动事务 + 故障留痕”的 BMS 软件架构。它不再是轮询原型，也不存在运行期创建任务或 FreeRTOS 动态分配路径：`App_Main()` 创建静态任务后启动调度器，`configSUPPORT_DYNAMIC_ALLOCATION` 为 0。【事实】

- 证据：`bms24v_platform/Core/Src/main.c:83-128`；`App/App_Main.c:269-377`；`bms24v_platform/MDK-ARM/FreeRTOS/include/FreeRTOSConfig.h:19-49`
- 信心：High
- 人工确认：Not needed

软件层面的 Pro 评审 P0/P1/P2 已建立逐项整改证据，但“软件实现完整”不等于“无人值守安全固件已定型”。充电、放电、短路、温度、预放电、唤醒、复位期间 gate 波形和故障注入仍需要当前固件与目标板组合的 HIL 证据。【事实 + 发布判断】

- 证据：`docs/review/pro_remediation_scorecard_2026-08-22.md`；`docs/ai_distilled/10_review_checklist.md`
- 信心：High
- 人工确认：Needed

## 产品与硬件范围

| 对象 | 当前证据结论 | 证据 | 信心 | 人工确认 |
| --- | --- | --- | --- | --- |
| 电池系统 | 24 V 标称、6S、NMC 21700 项目基线 | `docs/rules/hardware_rules.md:23-29` | High | Needed，确认量产 BOM/电芯型号 |
| MCU | STM32G0B1CBTx，CubeMX/HAL 工程 | `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx:10-21`；`bms24v_platform/bms24v_platform.ioc:43-44` | High | Not needed |
| AFE/保护 | BQ76952，I2C1，PB4 ALERT | `Int/Int_BQ76952_BSP.h:12-23`；`bms24v_platform/Core/Inc/main.h:76-79` | High | Needed，确认 Comm Type 与硬件 CRC 模式 |
| 充电控制 | SC8815，PA6/PA7 软件 I2C，PA5 INT，PB0 PSTOP，PB1 CE_N | `bms24v_platform/Core/Inc/main.h:60-69`；`Int/Int_SC8815_BSP.h:10-31` | High | Needed，确认比值/限流与实板一致 |
| 人机/存储/通信 | OLED + M24C64 共用 I2C2；FDCAN1；USART1；LED/蜂鸣器/按键 | `docs/rules/hardware_rules.md:88-109`；`bms24v_platform/Core/Src/i2c.c:84-107,172-179` | High | Needed，确认 OLED/EEPROM 实际地址 |

## 启动与运行时

复位后，`USER CODE BEGIN 1` 在 `HAL_Init()` 前直接配置 PB0/PB1 为高，先建立 SC8815 `PSTOP=1/#CE=1`；因此 HAL 时基/IRQ 和系统时钟初始化之前已有软件可建立的 SC 安全电平。时钟完成后在 CubeMX `USER CODE BEGIN SysInit` 内再执行 `MX_GPIO_Init() -> Int_SC8815_ForceStandby() -> MX_I2C1_Init() -> App_BatMan_EarlySafeOutputs()`；只有获得 BQ `ALL_FETS_OFF` 证据后，才继续 RTC/FDCAN/I2C2/UART/TIM、日志和业务初始化。`App_Main()` 再次强化两路安全输出并把两次 BQ early-safe 结果共同交给 Safety；最后静态创建任务。【事实】

- 证据：`bms24v_platform/Core/Src/main.c:83-128`；`Int/Int_SC8815.c:21-46`；`App/App_Main.c:215-267,269-377`
- 信心：High
- 人工确认：Needed，reset-to-GPIO/HAL/clock 与 BQ gate 波形

关键周期为 Safety 10 ms、SC8815 20 ms、CAN 20 ms、维护 250 ms、BatMan 1000 ms；周期调度使用绝对唤醒时刻并限制追赶，避免任务超时后无界补跑。【事实】

- 证据：`App/App_Safety.c:9-16`；`App/App_Main.c:25-49,86-105`
- 信心：High
- 人工确认：Not needed

## 安全边界

`App_Power` 是充放电、预放电和唤醒的业务授权者；`App_Safety` 独立维护 inhibit、task deadline、stack watermark、wake epoch 和 IWDG 喂狗资格。BQ 主 FET 与 SC start 必须携带同一 Safety epoch：Power 获取一次令牌，BQ owner 在写前、I2C/回读后和返回前复验，Power 在两路 actuator 后再总复验；critical ALERT owner 周期同时锁存 Safety 并请求 BQ 四路 FET 全关。ISR 在 SC/BQ 告警到来时先执行 `Int_SC8815_ForceStandby()`，再递交事件。【事实】

- 证据：`App/App_Power.c:206-290`；`App/App_Safety.h:85-92`；`App/App_Safety.c:67-117,301-465`；`App/App_BatMan_Config.c:463-547`；`App/App_BatMan.c:191-293`；`Int/Int_BQ76952.c:585-635`；`bms24v_platform/Core/Src/stm32g0xx_it.c:204-212`
- 信心：High
- 人工确认：Not needed

运行期 BQ 完整帧失败，或完整帧观察到 `POR/CFGUPDATE/FET_EN` 指纹异常，会立即把配置状态降为 `CONFIG_RECOVERY_REQUIRED`；失败路径的第一硬件动作是 `App_SC8815_EmergencyStop()`，随后撤销 Safety ready，再 best-effort 请求主 FET 全关。Safety 设置任一 inhibit 也先直接 PSTOP；Power 在 BQ offline 的每个周期继续尝试全关。重新在线不直接恢复授权：必须在同一 1500 ms 外层 BQ transaction 内执行 `ALL_FETS_OFF -> DeviceNumber -> manifest 重写/退出 -> 全量验证 -> safe FET enable/all-off -> 状态终检`，之后还要等下一完整有效帧才重新 ready。【事实】

- 证据：`App/App_BatMan_Sample.c:411-472`；`App/App_BatMan.c:294-346,673-684`；`App/App_Safety.c:67-93`；`App/App_BatMan_Config.c:75-78,666-811`；`App/App_Power.c:586-657`
- 信心：High
- 人工确认：Needed，运行期断线/POR/CFGUPDATE/FET_EN 丢失与恢复 HIL

硬件仍存在一个不能由软件消除的上限：当前 MCU 引脚表没有独立的 BQ 主 CHG/DSG gate 关断 GPIO；DFETOFF/DCHG 被温度资源占用，CFETOFF 未连接。SC 能在系统时钟前由 GPIO 直接进入安全态；BQ 的最早主 FET 全关却必须等系统时钟、I2C1 和命令/回读成功。因此从复位到 BQ early-safe 确认，以及 CPU/I2C 失效期间，主 FET 只能依赖 BQ 自身默认/自主保护或后续 IWDG 复位流程。【事实 + 推断】

- 证据：`bms24v_platform/Core/Src/main.c:83-110`；`Int/Int_SC8815.c:21-46`；`bms24v_platform/Core/Inc/main.h:60-79`；`official_chip_docs_files/BQ76930_to_BQ76952_逻辑替换设计说明.md:189-200`；`official_chip_docs_files/full_netlist (4).csv:29-45`
- 信心：High
- 人工确认：Needed，原理图/ERC 与示波器确认

若产品安全目标要求“MCU 单点失效时立即、独立地关闭主充放电 FET”，需要下一版硬件增加独立 CFETOFF/DFETOFF/BOTHOFF 或外部安全关断链，软件无法补齐该硬件独立性。【推断】

## 当前发布定位

| 门槛 | 状态 | 说明 |
| --- | --- | --- |
| 软件实现/静态契约 | 100/100（代码轴） | 平台红队在冻结工作区发现的代码级 P0/P1=0；关键控制路径、锁、超时、监督、故障记录、Release CLI 隔离和有界日志均有源码/契约证据 |
| 主机单元/结构测试 | Closed in current workspace | Host 5/5；含算法、平台、BQ、Power/SC 和 repo hygiene |
| GCC 构建与门禁 | Closed in current workspace | 三套 clean ARM build 均通过，各 4/4；Release Flash 68,412 B、RAM 20,704 B；size/symbol/repo hygiene 门禁通过 |
| 仓库交付卫生 | Software closed | tracked forbidden artifact=0；本交付版本已退跟踪 1098 项历史产物 |
| Keil/ARMCC 等价构建 | Unknown | 有工程元数据，没有本轮可复核的 ARMCC 构建日志 |
| 实板安全 HIL | HIL blocked | 缺当前固件的短路/过流/温度/预放电/唤醒/IWDG 波形与故障注入证据 |
| 无人值守量产冻结 | Blocked | HIL 与硬件独立关断目标未闭环前不得宣称 100 分量产安全 |

双轴结论：软件实现/静态契约可记为 `100/100`；“无人值守量产发布”不得记为 100，不能用代码门禁替代充放电、短路、温度、PDSG、唤醒、watchdog HIL，也不能消除无独立 MCU 主 FET 关断脚的硬件上限。【事实 + 判定】

## 文档导航

- 架构：`01_architecture.md`
- 模块：`02_module_inventory.md`
- 规范：`03_coding_conventions.md`
- 硬件映射：`04_hardware_software_matrix.csv`
- 并发：`05_rtos_concurrency_model.md`
- 存储：`06_memory_map.md`
- 构建：`07_build_config_matrix.md`
- 冲突/未知：`08_conflicts_and_unknowns.md`
- 变更影响：`09_change_impact_playbook.md`
- 发布检查：`10_review_checklist.md`
- 逐条取证：`evidence_index.md`
- Pro 整改评分卡：`../review/pro_remediation_scorecard_2026-08-22.md`
