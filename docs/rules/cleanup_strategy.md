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

2. BQ/SC INT 层
   - INT 层主要清洗对象是 `Int_BQ76952` 和 `Int_SC8815`，因为它们承载 BQ/SC 芯片协议、寄存器保护、I2C/软 I2C 时序和硬件安全约束。
   - 第一阶段只收口重复通信辅助函数、错误返回、注释边界和命名；不能改寄存器值、保护阈值、CRC/校验算法、软 I2C 时序、PSTOP/CE_N 行为。
   - 每次触碰 BQ/SC INT 后至少编译；触碰寄存器配置或功率请求相关路径后必须上板复测 `bq`、`sc`、`power`。

3. 其他低耦合 INT/COM 模块
   - `Int_EEPROM`、`Int_CanFd`、`Com_SOC`、`Com_SOH` 等暂时靠后；它们不是当前 INT 层主要矛盾。
   - 对 I2C2 共享资源保持谨慎：EEPROM 和 OLED 共用 I2C2，不能引入长时间阻塞或抢总线风险。

4. OLED/显示层
   - OLED 属于观测输出，不应该反向影响 BMS 策略。
   - 清洗时保持显示失败不影响充放电主链路。

5. 功率链路深清洗
   - 只有在前面模块稳定后，再整理 `App_Power` 的状态机、停充原因、放电保护锁存和预放电策略。
   - 每次修改后都要通过串口命令和上板充放电确认。

6. Debug CLI 最后清洗
   - 最后根据 `docs/rules/debug_cli_removal.md` 判断是否删除或迁移。
   - 删除前必须确认已有生产诊断、产测、故障恢复和日志替代路径。

## 当前下一步

APP、COM、INT 的本轮静态清洗已完成，下一步是保留 Debug CLI 上板回归当前生产链路：冷启动、自动充电、拔充电器放电、SCD 锁存/恢复、OLED 和串口观测全部正常后，再按 `docs/rules/debug_cli_removal.md` 的阶段 A/B 退场。

在上板结果闭环前不继续修改 SOC/SOH/OCV、保护阈值、BQ/SC 寄存器配置或功率状态机。尤其要先决定 `App_Power_ClearDischargeFault()` 的生产恢复入口；该入口未迁移或未明确改为重新上电恢复前，不能删除 Debug CLI。

## 证据

- Debug CLI 当前由 `App_Main` 创建独立任务，说明它仍是运行期上板工具：`App/App_Main.c:65`、`App/App_Main.c:100`、`App/App_Main.c:117`。
- Debug CLI 删除门禁和两阶段步骤已有单独规则，不能提前执行：`docs/rules/debug_cli_removal.md` 第 2～5 节。
- BQ76952 INT 层承载 direct/subcommand/Data Memory/ConfigUpdate 通信路径：`Int/Int_BQ76952.c:298`、`Int/Int_BQ76952.c:455-579`。
- SC8815 INT 层仍保留软 I2C、临界区、线序反接兜底、寄存器写保护和功率相关引脚：`Int/Int_SC8815.c:17-488`、`Int/Int_SC8815.c:504-576`、`Int/Int_SC8815.c:833-875`。
- EEPROM 和 OLED 共用 I2C2，清洗后两者仍分别通过 `hi2c2` 探测和发送：`Int/Int_EEPROM.c:18`、`Int/Int_OLED.c:30`。
