# 代码清洗顺序策略

## 目标

清洗顺序必须服务于上板调试和业务稳定性。`App_DebugCli` 是当前 AI 上板抓日志、发命令和定位问题的入口，必须最后清洗，不能在生产链路稳定前提前拆除。

## 当前约束

- Debug CLI 仍用于 `bqfast on/off`、`vofa on/off`、`fault clear`、`sc`、`bq`、`power` 等上板验证命令。
- 生产业务不能依赖 Debug CLI 命令才能完成充电、放电、保护恢复或均衡。
- 清洗过程中不改变充放电、保护、采样、SC8815 请求、BQ FET 控制的可观测行为。
- 每轮清洗后至少执行 `cmake --build --preset gcc-debug`；触碰功率链路时必须上板复测。

## 推荐顺序

1. 生产链路边界
   - 先整理 `App_BatMan_*`、`App_Power`、`App_SC8815` 之间的职责边界。
   - 保留当前采样、保护、充电、放电策略的行为。
   - 只做命名、注释、薄封装删除、参数归属收口。

2. 低耦合 INT/COM 模块
   - 优先清洗 `Int_EEPROM`、`Int_CanFd`、`Com_SOC`、`Com_SOH` 等不直接驱动功率 FET 的模块。
   - 对 I2C2 共享资源保持谨慎：EEPROM 和 OLED 共用 I2C2，不能引入长时间阻塞或抢总线风险。

3. OLED/显示层
   - OLED 属于观测输出，不应该反向影响 BMS 策略。
   - 清洗时保持显示失败不影响充放电主链路。

4. 功率链路深清洗
   - 只有在前面模块稳定后，再整理 `App_Power` 的状态机、停充原因、放电保护锁存和预放电策略。
   - 每次修改后都要通过串口命令和上板充放电确认。

5. Debug CLI 最后清洗
   - 最后根据 `docs/rules/debug_cli_removal.md` 判断是否删除或迁移。
   - 删除前必须确认已有生产诊断、产测、故障恢复和日志替代路径。

## 当前下一步

下一轮优先清洗 `Int_EEPROM` 或 `Int_CanFd` 这类低耦合模块，原因是它们不会直接改变 BQ/SC/FET 功率行为。清洗目标是统一命名、错误返回、边界检查和注释质量，而不是新增持久化业务。

## 证据

- Debug CLI 当前由 `App_Main` 创建独立任务，说明它仍是运行期上板工具：`App/App_Main.c:57-69`、`App/App_Main.c:96-97`、`App/App_Main.c:114`。
- Debug CLI 删除步骤已有单独规则，不能提前执行：`docs/rules/debug_cli_removal.md:15-31`。
- EEPROM 和 OLED 共用 I2C2，清洗 EEPROM/OLED 时要注意总线阻塞：`Int/Int_EEPROM.c:5`、`Int/Int_OLED.c:30`。
- EEPROM 当前只在启动时初始化，未进入功率控制闭环，适合作为下一轮低风险清洗对象：`App/App_Main.c:83-86`。
