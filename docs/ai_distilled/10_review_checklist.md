# 10 Review Checklist

更新日期：2026-08-22

状态含义：`Closed`=证据已完全满足该软件/文档检查项；`Software closed`=实现与主机证据具备、实板结果另列；`HIL blocked`=必须实板；`Unknown`=无可复核证据；`Conflict`=来源未统一。

## Architecture Summary

- [x] `Software closed`：`main.c` 仅生成初始化、USER 早期安全序列和 `App_Main()`；业务在 App/Int/Com。`HAL_Init()` 前直接拉高 SC PSTOP/CE_N；时钟后 GPIO→SC standby→I2C1→BQ all-off 早于其余 Cube 外设和日志。证据：`bms24v_platform/Core/Src/main.c:83-128`；`Int/Int_SC8815.c:21-46`；`App/App_Main.c:215-225`。
- [x] `Closed`：静态 FreeRTOS 任务，动态分配关闭。证据：`App/App_Main.c:51-78,269-377`；`bms24v_platform/MDK-ARM/FreeRTOS/include/FreeRTOSConfig.h:40-49`。
- [x] `Software closed`：Power 是唯一业务授权者；BQ/SC owner 同用一个 Safety epoch，BQ 前/中/后、SC PSTOP release 前及 Power 总提交后复验；SetOutput 四类失败统一 `ForceOutputsOff`；critical ALERT owner 周期锁 Safety 并全关 BQ FET；BQ ALERT inhibit 仅在单调 sequence 和 active-low pin 的关中断复验后清除；Safety 是唯一 IWDG 刷新者。证据：`App/App_Power.c:206-290`；`App/App_BatMan_Config.c:463-547`；`App/App_BatMan.c:191-293`；`Int/Int_BQ76952.c:585-635`；`App/App_SC8815.c:106-312`；`App/App_Safety.c:469-560`。
- [x] `Closed`：Release CLI implementation stubbed，Engineering 受物理 gate + timeout unlock。证据：`App/App_DebugCli.c:3,236-264,446-496,569-589`。
- [ ] `HIL blocked`：验证 reset/异常/告警期间 SC/BQ 输出始终满足产品 safety goal；BQ 在 clock/I2C1/command 前没有独立 MCU gate-off，不能由软件证据关闭。

## BQ76952

- [x] `Closed`：地址、CRC default、expected DeviceNumber 与 6S sparse VC map 显式定义。证据：`Int/Int_BQ76952_BSP.h:12-23,200-208,228-249`。
- [x] `Software closed`：recursive transaction mutex + absolute deadline；单 HAL I2C timeout 受剩余预算限制。证据：`Int/Int_BQ76952.c:130-240,636-659`。
- [x] `Software closed`：完整 staging frame 原子发布；任一读失败拒绝整帧。证据：`App/App_BatMan_Sample.c:352-472,505-530`。
- [x] `Software closed`：DM manifest 写入/读回；mismatch 形成 config invalid；init 保持 FET all-off。证据：`App/App_BatMan_Config.c:99-173,242-310`；`App/App_BatMan.c:455-664`。
- [x] `Software closed`：FET 控制区分 desired/commanded/observed；不使用 `ALL_FETS_ON` 作为选择性控制前置步骤；PreReset 只在成功读回后更新 `commanded_off_mask`。证据：`App/App_BatMan_Config.c:314-665`；`Int/Int_BQ76952.c:1066-1103`。
- [x] `Software closed`：ALERT/protection latch 不能由通用清除 API 无条件解除。证据：`App/App_Safety.c:301-386`；`App/App_BatMan.c:191-293`。
- [x] `Software closed`：采样层只发布原始 POR/CFGUPDATE/FET_EN 指纹；Safety facade 判断证明丢失后第一硬件动作停 SC，再置 `CONFIG_RECOVERY_REQUIRED`、撤销 Safety ready，最后 best-effort BQ all-off；Safety 任一 inhibit 也先 PSTOP；Power 在 offline 每周期继续 all-off。证据：`App/App_BatMan_Sample.c:281-336,411-472`；`App/App_BatMan.c:294-346,673-684`；`App/App_Safety.c:67-93`；`App/App_Power.c:586-657`。
- [x] `Software closed`：重在线必须在 1500 ms 不可插入的外层 transaction 中完成 all-off 屏障、DeviceNumber、manifest 重写/退出、全量验证、safe FET/all-off 和终检，并等下一完整帧才 ready。证据：`App/App_BatMan_Config.c:75-78,666-811`；`App/App_Power.c:316-467`。
- [ ] `Conflict`：5 mOhm shunt 与旧 0.5 mOhm 文档冲突已记录，仍需物理标定。证据：`08_conflicts_and_unknowns.md:C-004`。
- [ ] `Unknown`：TS NTC/Beta/通道和最终 protection parameter approval。
- [ ] `HIL blocked`：COV/CUV/OCD/SCD/PF/温度 fault injection；ALERT latency；CHG/DSG/PDSG gate waveform。

## SC8815 / charging

- [x] `Software closed`：PA6/PA7 固定 line swap，禁止 runtime 自动换线。证据：`Int/Int_SC8815.c:61-89,1105-1139`。
- [x] `Software closed`：软件 I2C bounded clock-stretch、bus busy 检查、9-pulse recovery；整笔事务不关闭全局中断。证据：`Int/Int_SC8815.c:107-187,557-637`。
- [x] `Software closed`：高-低-高 ADC coherency、配置写读回、bounded current limit。证据：`Int/Int_SC8815.c:642-676,771-839,1028-1103`。
- [x] `Software closed`：PSTOP 释放前最后校验 Safety epoch；INT/错误先强制 standby；SC_EVENT 单调 sequence、owner 重申 inhibit，连续 2 clean 后在 IRQ-disabled 临界区内原子复验/clear，序号变化保持急停。证据：`App/App_SC8815.c:106-312,435-505,508-620`；`Int/Int_SC8815.c:731-770`；`tests/host/test_power_soc_contract.py:70-85`。
- [x] `Software closed`：IBUS/IBAT ratio 与 Release default/max 已对齐硬件规则：3x/6x、1.5/3 A、1/5 A。证据：`Int/Int_SC8815_BSP.h:21-31`；`docs/rules/hardware_rules.md:46-60`；`tests/host/test_power_soc_contract.py:58-67`。
- [ ] `Conflict`：生成宏/硬件规则标 PA7=SCL、PA6=SDA，但 fixed-swapped driver 实际使用 PA6=SCL、PA7=SDA。证据：`08_conflicts_and_unknowns.md:C-001`。
- [ ] `HIL blocked`：逻辑分析仪验证 line order/stretch/stuck recovery；充电限流、截止、温升、INT-to-PSTOP。

## Power / wake / temperature / pre-discharge

- [x] `Software closed`：任何 invalid BQ/config/cell/temp 都禁止充放电；BQ offline 不只改 allowed flag，而是每周期尝试全关。证据：`App/App_Power.c:586-671`。
- [x] `Software closed`：Release offline BQ wake 禁止；Engineering 只接受 typed evidence + 500 ms epoch。证据：`App/App_Power.c:19-28,177-204,293-467`；`App/App_Safety.c:387-468`。
- [x] `Software closed`：温度策略有边界/滞回；invalid TS 不回退到 IC temperature 继续运行。证据：`App/App_Power.c:468-486,586-686`；`App/App_BatMan_Sample.c:228-280,301-336,475-504`。
- [x] `Software closed`：full/empty anchor 需要关断后小电流、delta、稳定窗口等条件。证据：`App/App_Power.c:488-528,692-814`。
- [ ] `HIL blocked`：charger insert/remove、shutdown/offline wake、温箱边界/传感器开短路。
- [ ] `HIL blocked`：PDSG success/timeout/failure/retry、外部电容上限、R40 脉冲热与 DSG handoff。

## Supervisor / faults / watchdog

- [x] `Software closed`：critical task deadline 与 stack HWM；heartbeat 在工作完成后报告。证据：`App/App_Main.c:107-176`；`App/App_Safety.c:156-211`。
- [x] `Software closed`：只有健康 Safety supervisor 刷新 IWDG。证据：`App/App_Safety.c:469-560`。
- [x] `Software closed`：assert、stack overflow、malloc hook、HardFault 汇入 fault handler；异常先 ForceStandby。证据：`Int/Int_Fault.c:176-215,256-272`。
- [x] `Software closed`：backup record 保存 reset cause/PC/LR/checksum。证据：`Int/Int_Fault.c:63-174`。
- [ ] `HIL blocked`：挂起 BatMan/SC/CAN、制造 deadlock/stack overflow/assert/HardFault，测复位时间与所有 power outputs。
- [ ] `Unknown`：LSI 实际频率、IWDG 容差、backup domain 掉电保持。

## SOC / SOH / EEPROM

- [x] `Software closed`：带载启动不直接信任 OCV；seed/anchor provenance 显式。证据：`Com/Com_SOC.h:19-40,92-127`。
- [x] `Software closed`：SOC/SOH 双槽、CRC、sequence、写后读回。证据：`App/App_BatMan_Nvm.c:17-38,204-539`。
- [x] `Software closed`：静态 recursive NVM mutex 串行 slot/sequence/reconnect/flush；SOC/SOH/provenance 取自同一任务原子快照，scheduler pause 不跨 EEPROM I/O。证据：`App/App_BatMan_Nvm.c:49-175,713-894`；`tests/host/test_platform_contract.py:155-186`。
- [x] `Closed`：native `algorithm_unit` 已注册到 host-test-only CTest。证据：`tests/host/CMakeLists.txt:29-55`。
- [ ] `Unknown`：目标电芯 OCV/SOH/容量/温漂标定。
- [ ] `Unknown`：长时间断电后的 SOC freshness 策略。
- [ ] `HIL blocked`：EEPROM 写中掉电、slot 恢复、endurance、完整循环/anchor 验证。

## CAN / UART / diagnostics

- [x] `Closed`：CAN V1 只读，不含 MOS/PDSG/fault clear/参数写。证据：`docs/protocol/bms_canfd_protocol.md:1-11`。
- [x] `Software closed`：CAN RX budget、event queue、deferred TX/reinit 都有界。证据：`App/App_CanBms.c:23-40,362-550,679-819`。
- [x] `Software closed`：应用格式化日志统一 `Int_Log_Printf + Com_FormatV`，384 B 固定栈缓冲投递 1024 B UART IT ring，截断/容量不足均可计数；post-link gate 拒绝 heap/stdio。当前 Release gate 输出 `symbol gate: no heap/stdio symbols; bounded log formatter present`。证据：`Com/Com_Format.c:13-269`；`Int/Int_Log.c:11-24,47-231`；`CMakeLists.txt:194-201`；`tests/host/check_forbidden_symbols.py:10-50`；`tests/host/test_algorithms.c:376-408`。
- [ ] `HIL blocked`：CAN flood/bus-off、UART HAL_BUSY/error/saturation。

## Build / test / repository

- [x] `Closed`：GCC debug/release/engineering presets；Release macro OFF。证据：`CMakePresets.json:5-35`。
- [x] `Closed`：`-Wall -Wextra -Werror`、map/hex/bin、Flash/RAM budget。证据：`CMakeLists.txt:141-190`。
- [x] `Closed in current workspace`：Host 5/5、三套 clean ARM 各 4/4；CI host job 与三 GCC preset matrix 配置存在。证据：`tests/host/CMakeLists.txt:1-55`；`.github/workflows/firmware-ci.yml:8-36`。
- [x] `Software closed`：repo hygiene CTest 已注册并通过，tracked forbidden artifact=0；本交付版本已退跟踪 1098 项历史产物，其中本地需要的二进制/`.log.lock` 只从 Git index 移除并由 ignore 覆盖。证据：`.gitignore:1-41`；`tests/host/check_repo_hygiene.py:11-105`；`tests/host/CMakeLists.txt:8-13`；交付提交相对父版本的 deletion manifest。
- [x] `Closed in current workspace`：60 个 C/H format、30 个变更 C 文件 clang analyzer、HardFault objdump、Keil XML/member gates 均通过；平台红队代码级 P0/P1=0。人工确认：Needed，归档命令/工具版本/日志。
- [ ] `Unknown`：远端 CI 在冻结 commit SHA 上的绿色 run。
- [ ] `Unknown`：Keil/ARMCC clean build 与 map。
- [ ] `Unknown`：MISRA/CERT、全量静态分析与 coverage/MC-DC 报告。

## Hardware-Software Interface Matrix checks

- [x] `Closed`：GPIO/I2C/FDCAN/UART/TIM mappings 已列入 `04_hardware_software_matrix.csv`。
- [ ] `Needed`：逐条对照最终原理图、BOM、实物改焊和 CubeMX；任何 Conflict 先裁决再上功率。
- [ ] `Hardware blocker`：若 safety goal 要求 MCU 单点失效立即关主 FET，当前无独立 MCU gate-off 脚，需硬件改版。证据：`bms24v_platform/Core/Inc/main.h:60-79`；`official_chip_docs_files/full_netlist (4).csv:29-45`。

## Final release decision

双轴结论：`软件实现/静态契约：100/100`（冻结工作区可复现，平台红队代码级 P0/P1=0）；`无人值守量产发布：不得宣称 100`。在以下证据全部归档前，不得把当前仓库标记为“100 分无人值守量产固件”：

1. C-001 的 SC 物理 line order 冲突关闭，并完成 ratio/limit 精度标定；
2. BQ shunt/NTC/DM/保护阈值签字与标定；
3. 当前 commit 的充/放/短路/温度/PDSG/wake/watchdog，以及运行期 BQ 断线/POR/重认证 HIL；
4. 全程 gate/PSTOP/current/temperature 波形与 fault log；
5. GCC + Keil clean build + host tests + CI SHA + size/map；
6. 硬件独立主 FET 关断安全目标的接受或改版决定。
