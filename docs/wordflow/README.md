# BMS 项目文档入口

更新日期：2026-07-12

## 当前权威基线

- 仓库：`lst92991-eng/bms`
- 分支：`main`
- 审查基线提交：`edd408238fbaddc425cfde24bafd67febbe5e68e`
- 当前状态：必要代码清洗待完成，随后执行充电曲线测试，最终文档尚未冻结。

## 当前阅读顺序

1. `docs/state/project_closeout_plan.md`：项目问题决策、清洗范围、测试和最终文档门禁。
2. `docs/rules/hardware_rules.md`：当前硬件事实与硬约束。
3. `docs/logic/hardware_interface_reservation.md`：App/Com/Int 和硬件接口边界。
4. `docs/rules/cleanup_strategy.md`：本轮允许修改和禁止扩张的范围。
5. `docs/test/ai_charge_curve_test_task.md`：清洗完成后的充电曲线测试任务。
6. `docs/wordflow/manual_confirmations.md`：用户明确确认的硬件和项目事实。
7. `docs/ai_distilled/08_conflicts_and_unknowns.md`：当前开放、已解决和延期事项。

## 关于 `bms24v_project_document_v0.md`

`bms24v_project_document_v0.md` 是早期初版草稿，其中原“`new_bms` 仓库 / `wordflow` 分支”的页眉信息属于历史写作上下文，不再代表当前项目基线。该文件保留用于章节结构、证据索引和最终文档素材，不作为当前状态入口。

最终完整开发文档将在以下条件满足后重新冻结版本号和页眉：

- 预放电、SC8815 EXTI、RTOS 周期和外设启动策略清洗完成。
- 充电曲线、实际截止电压、温升和阈值核对完成。
- 完整放电合格结论完成必要回归。
- 固定 release commit、硬件版本、板号和测试日期。

在此之前，任何文档不得把未测试项写成“已经通过”。
