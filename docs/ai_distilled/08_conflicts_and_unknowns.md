# Conflicts And Unknowns

更新日期：2026-07-18

本文只保留当前仍有执行价值的冲突、未知项和明确延期项。已由源码、实板或用户确认闭环的旧结论不再继续作为开放问题。

## 1. 当前开放问题

| ID | 类型 | 当前事实 | 影响 | 状态 | 下一步 |
| --- | --- | --- | --- | --- | --- |
| O-001 | 生产故障恢复入口 | `App_Power_ClearDischargeFault()` 当前主要由 Debug CLI 调用 | Debug CLI 后期删除前，必须把安全清故障能力迁移到生产入口 | Planned | 迁移到 CAN、受控维护命令或明确的重新上电恢复策略；不得无条件清除 SCD |
| O-002 | 预放电生产闭环 | PDSG 单独控制目前主要用于 CLI 探测，正常状态机尚未形成 `PDSG -> DSG` 可验证流程 | 接大电容负载时可能直接打开 DSG，缺少受控预充 | **Must fix** | 清洗阶段实现预放电状态、超时、压差判定、失败关断和回归测试 |
| O-004 | RTOS 周期写法 | 当前任务直接把毫秒数传给 `vTaskDelay()`，并使用相对延时 | 配置 tick rate 改动后单位失效；任务执行时间会叠加到周期中 | **Must fix** | 使用 `pdMS_TO_TICKS()`；周期任务改为 `vTaskDelayUntil()` |
| O-005 | 外设初始化失败策略 | CANFD、EEPROM 初始化返回值当前被忽略 | 项目要求的外设失败时仍可能继续进入 RUN | **Must fix** | 检查返回值；失败进入安全故障态并禁止正常 RUN |
| O-006 | 充电曲线与阈值标定 | 完整放电测试已通过；充电曲线、实际截止电压、充电温升及部分保护阈值尚未形成实测记录 | 无法完成最终充电闭环和正式开发文档 | Test pending | 按 `docs/test/ai_charge_curve_test_task.md` 执行并归档证据 |
| O-007 | BQ 事务阻塞时间 | BQ echo/config 轮询包含 `HAL_Delay()`，I2C timeout 较长 | 异常通信时高优先级任务可能阻塞较久，影响调度响应 | Review pending | 先完成技术说明和最坏耗时测算；本轮不直接改代码 |
| O-008 | 测试数据归档 | 实测 CSV 当前直接放在通用 `csv_data` 目录 | 测试条件、板号、固件 SHA 和结果难以追溯 | Planned | 迁移到 `test/evidence/<date>/<board-id>/`，大文件后续评估 Git LFS 或 Release artifact |
| O-009 | 项目总文档 | 当前正式项目文档仍处于初版草稿，最终充电结果和清洗结果尚未写入 | 现在定稿会把未验证事项写成事实 | Blocked | 清洗完成、充电测试通过后冻结 commit，再生成最终开发文档 |

## 2. 已解决或已确认事项

| ID | 事项 | 结论 | 证据/处理 |
| --- | --- | --- | --- |
| R-001 | RTOS 是否存在 | 已解决：当前工程为 FreeRTOS 三任务模型 | `App/App_Main.c`、`FreeRTOSConfig.h`、`CMakeLists.txt` |
| R-002 | BQ 默认 CRC | 已解决：项目与实板默认 `CRC disabled`；驱动保留 CRC 开关 | `docs/wordflow/manual_confirmations.md`、`docs/rules/hardware_rules.md` |
| R-003 | BQ WAKE/PB3 方向冲突 | 已解决：新版硬件已删除原 BQ WAKE/TS2 通道，软件不得依赖或驱动 PB3 | `docs/wordflow/manual_confirmations.md`、`docs/rules/hardware_rules.md` |
| R-004 | SC8815 VBATS 分压 | 已解决硬件值：`R17=200kΩ`、`R18=10kΩ`；实测截止仍属于测试项 | `docs/wordflow/manual_confirmations.md` |
| R-005 | BQ 低边采样电阻 | 已解决：按 `5mΩ` 作为当前硬件基线 | `docs/wordflow/manual_confirmations.md` |
| R-006 | 命令行构建入口 | 已解决：存在 GCC Debug/Release CMake Presets | `CMakePresets.json`、`CMakeLists.txt` |
| R-007 | 构建产物忽略规则 | 已解决：仓库已有 `.gitignore` 覆盖常见 GCC/Keil 产物 | `.gitignore` |
| R-008 | 完整放电测试 | 当前阶段合格，后续只保留必要回归 | 用户 2026-07-12 确认 |
| R-009 | SC8815 INT 配置 | 已解决：PA5 为下降沿 EXTI；ISR 只置 pending flag，20ms SC 任务检查 flag 后读取 STATUS/ADC | `bms24v_platform/bms24v_platform.ioc:103-107,260-262`、`bms24v_platform/Core/Src/stm32g0xx_it.c:108-116,151-157`、`Int/Int_SC8815.c:577-594`、`App/App_SC8815.c:314-344` |

## 3. 明确不处理或延期

| ID | 事项 | 决策 | 说明 |
| --- | --- | --- | --- |
| D-001 | AI Debug CLI 的 BQ 并发访问接口 | No action | 该接口仅供 AI bring-up，后期整体删除；不为它引入长期生产架构 |
| D-002 | RTOS 任务创建返回值检查 | No action | 保留当前教学展开习惯，本轮不处理 |
| D-003 | 看门狗 | Deferred | 当前阶段不加入 IWDG/WWDG，不作为本轮闭环阻塞项 |
| D-004 | Debug CLI 默认 CSV | No action | 继续作为 AI 调试和充电曲线采集入口 |
| D-005 | CI/静态分析门禁 | No action | 当前阶段不建设自动化门禁 |

## 4. 仍需实测确认

- SC8815 VBATS、PACK 总压、最高单体和实际充电截止电压。
- 充电电流曲线、恒流/恒压阶段表现、温升、EOC 和异常停止原因。
- OCC/OCD/SCD/CUV/OTC/OTD 配置值与实测值的对应关系；完整放电已通过，不要求无意义重复测试。
- TS1/TS3 NTC 型号、读数与实测温度对应关系。
- 单路均衡物理通道与温升。
- SC8815 `FB/ADIN` 外部连接和 ADC 含义。

## 5. 工作规则

1. 当前源码和 `.ioc` 描述“现在实现了什么”。
2. `docs/rules/hardware_rules.md` 与人工确认记录描述“当前硬件事实”。
3. `docs/state/project_closeout_plan.md` 描述“下一步执行什么”。
4. `docs/test/ai_charge_curve_test_task.md` 描述“充电测试如何执行和交付”。
5. 未完成测试的项目不得在最终开发文档中写成已验证。
