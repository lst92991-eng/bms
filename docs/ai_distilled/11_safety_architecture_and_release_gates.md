# BMS Safety Architecture And Release Gates

更新日期：2026-08-22

## 1. 文档定位

本文定义本仓库软件整改后的安全不变量、模块所有权、故障响应和发布门禁。它是实现与测试的验收依据，不是功能安全认证证书，也不替代系统级危害分析、硬件 FMEDA、充电标定或实板故障注入。

当前产品适用领域（机器人、工业储能、道路车辆或其他）尚未由仓库证据冻结，因此适用法规和安全完整性等级为 `Unknown`。在产品领域确认前，软件采用保守的 fail-safe 默认值，但不得声称符合 ISO 26262、IEC 62619、UL 1973 或其他具体认证。

## 2. 安全不变量

### INV-01：停充动作不依赖任务、队列或总线

- 任一禁止充电、超期、断言、异常中断或致命故障路径，第一项硬件动作必须是同步将 SC8815 `PSTOP` 置高。
- 该动作必须幂等、无 I2C、无队列、无内存分配、无日志和无等待，并允许在中断上下文调用。
- 停充后必须递增授权代次或锁存 trip；旧的启动请求不得在下一任务周期重新释放 PSTOP。

硬件依据：`PSTOP=1` 为 standby，PB0 上电默认由生成 GPIO 代码置高，见 `bms24v_platform/Core/Src/gpio.c:54-88`；板级规则记录 PB0 外部上拉，见 `docs/rules/hardware_rules.md:38-45`。

### INV-02：未知状态禁止释放功率

满足以下全部条件前，不得允许充电、放电或均衡：

1. BQ Data Memory manifest 已写入并逐项回读一致；
2. 最新 BQ 帧完整、未超预算、未过期；
3. 六串电芯均有效；
4. 至少一个电芯温度探头有效，控制温度取有效探头中的最高值；
5. 无 Safety/PF/配置/执行器失配/任务超期故障；
6. 相应 SC/BQ 执行器命令已执行并通过可用状态回读；
7. Power supervisor 给出的授权代次仍有效。

任何 NACK、timeout、CRC、echo、length、checksum、半帧、配置 mismatch 或状态过期均按“不可信”处理，不得用上一帧数值继续释放功率。

### INV-03：只有单一模块拥有功率授权

- `App_Power` 是充电、放电、预放电和 shutdown/wake 的唯一业务授权者。
- `App_SC8815` 是唯一可以正常释放 PSTOP 的模块；其他模块只能同步 trip 或提交请求。
- `App_BatMan` 是 BQ 事务、Data Memory、均衡和 FET 命令的唯一运行期 owner。
- CLI、CAN、OLED、日志、NVM 和蜂鸣器不得直接改变功率执行器。

### INV-04：BQ 间接窗口不可交叉

一次 BQ indirect transaction 从写入 `0x3E/0x3F` 开始，到 echo、length、data、checksum 完成或失败为止，必须持有同一事务锁并共享同一个绝对 deadline。任一步失败立即结束，不得继续访问后续对象。

### INV-05：禁止非原子的全 FET 释放序列

- 运行期选择性 FET 转移不得使用 `ALL_FETS_ON -> FET_CONTROL` 两条命令序列。
- 每次转移维护 `desired / commanded / observed` 三个状态；只有回读满足“禁止位绝不打开”时才可确认成功。
- 任一步通信或回读失败均保持安全 inhibit，软件不得仅凭命令返回值标记 synced。

TI TRM 说明 `ALL_FETS_ON` 会允许全部未被其他条件阻塞的 FET，而 `FET_CONTROL` 的 4 个低位分别阻止对应 FET，见本项目本地 TRM `TI_BQ76952_Technical_Reference_Manual_sluuby2b.pdf` 第 34、116、168 页。

### INV-06：只有确认过的 shutdown 轨迹可以唤醒充电

允许 SC 建立 BQ 唤醒能量路径必须依次具有：host shutdown 请求、主 FET 关断确认、shutdown 命令成功、SDM 或预期掉线证据、有效充电输入。普通通信失败、CRC 错误、配置失效、采样越界或未知离线必须进入 fault，不得进入 wake-charge。

### INV-07：看门狗只证明调度健康

- 只有最高优先级 Safety Supervisor 可以喂 IWDG。
- BatMan、Power、SC 和 CAN 的有界工作单元全部在 deadline 内完成，且栈水位满足门限时才允许刷新。
- heartbeat 必须在工作单元完成后更新，不能在入口“报到”。
- 任一关键任务超期先同步停充和记录故障，再停止喂狗。

### INV-08：诊断永不阻塞安全路径

UART 输出必须通过有界非阻塞缓冲发送；UART BUSY、断线或缓冲满只能增加 drop/error 计数，不能延长安全任务的总线 timeout 或 deadline。Release 默认不创建工程 CLI；危险命令必须经过编译配置、物理使能、解锁、超时和 Power 授权五道门。

## 3. 目标任务与所有权

| 优先级 | 任务 | 有界职责 | 禁止事项 |
| --- | --- | --- | --- |
| 4 | Safety Supervisor | 10 ms 检查 alert、heartbeat、deadline、栈和 watchdog；trip/授权 | I2C、printf、NVM、OLED |
| 3 | BatMan + Power critical cycle | 发布完整 BQ 帧，执行纯策略和受控执行器转移 | OLED、长日志、EEPROM 全页操作 |
| 2 | SC owner | 处理 SC INT、配置/回读、短周期状态推进 | 绕过 supervisor 清除 trip |
| 2 | CAN | 有界 RX/TX 和 bus-off 状态机 | 直接功率控制 |
| 1 | Maintenance | NVM、OLED、日志、工程 CLI、蜂鸣器 | 任何未经授权的执行器写入 |

如果后续把 BatMan 与 Power 拆成独立任务，必须先建立带 sequence/valid-mask/age 的原子快照，不能让 Power 读取一组跨帧全局变量。

## 4. 故障响应矩阵

| 故障类别 | 立即 PSTOP | 禁充 | 禁放 | 禁均衡 | 锁存 | 允许 wake |
| --- | --- | --- | --- | --- | --- | --- |
| BQ transport/protocol/deadline/半帧 | 是 | 是 | 是 | 是 | 未知离线锁存 | 否 |
| BQ CONFIG_INVALID | 是 | 是 | 是 | 是 | 是 | 否 |
| BQ Safety active | 是 | 是 | 按 BQ fault 路由 | 是 | 按故障类别 | 否 |
| BQ PF active | 是 | 是 | 是 | 是 | 是 | 否 |
| 双电芯温度探头无效 | 是 | 是 | 是 | 是 | 是 | 否 |
| 单探头有效 | 否，仅按温度阈值 | 使用有效探头最高温 | 使用有效探头最高温 | 使用有效探头最高温 | 记录降级 | 否 |
| SC 通信/short/OTP | 是 | 是 | 否，若 BQ 路径可信 | 是 | 视故障 | 否 |
| UART/OLED/NVM 故障 | 否 | 否 | 否 | 否 | 诊断降级 | 否 |
| 关键任务超期/stack/assert/HardFault | 是 | 是 | MCU 无独立 BQ gate，见限制 | 是 | fault record | 否 |
| 已确认 shutdown 后预期离线 | 是，正常停充 | 是，直到有效输入 | 是 | 是 | 生命周期状态 | 是，且仅有效输入 |

## 5. 参数依据与当前保守策略

- 电芯型号：EVE INR21700/50E，典型容量 5000 mAh，标准充电 1 A/4.20 V/100 mA 截止，最大连续放电 15 A；证据为 `《EVE INR21700 50E 规格书》...pdf` 第 2–4 页。
- 充电温区：0–15°C 不超过 1 A；15–45°C 规格允许最高 5 A，但本项目最终充电器、热设计和曲线尚未验证，因此量产上限为 `Unknown`；45°C 以上当前软件保持停充属于保守策略。
- 当前仓库已有 1/3/5/7 A 放电记录，但 9 A 以上、完整充电、均衡温升和保护故障注入尚未完成，见 `docs/review/board_regression_2026-07-07.md`。
- BQ COV、温度和过流配置必须同时满足电芯规格、硬件采样误差和系统风险分析；代码中的 expected manifest 只能证明“写入值一致”，不能证明“参数已标定正确”。

## 6. 软件发布门禁

以下项目全部通过，才能标记 `SOFTWARE_RELEASE_CANDIDATE`：

1. ARMCC5 与 GCC Debug/Release 均 0 error、0 warning；
2. Host unit tests、fault-injection tests 和 CTest 全部通过；
3. Release map 中无危险 CLI 命令、无阻塞 `HAL_UART_Transmit` 路径；
4. 静态配置 manifest 100% 回读一致；逐项 mismatch 测试均禁止 power release；
5. BQ/SC 第 N 笔 NACK、timeout、CRC、echo、length、checksum 故障均首错中止；
6. watchdog、stack overflow、malloc failure、assert、HardFault 注入均先停充并留下 reset record；
7. 仓库中无受跟踪 build/log/cache/binary 临时产物；
8. 所有具体结论、阈值和硬件映射均有文件/行号或文档页证据。

## 7. 实板发布门禁

以下项目必须在限流电源、电子负载、温箱和示波器/逻辑分析仪上完成，软件静态审阅不能替代：

1. 从 fault/alert/deadline 产生到 PSTOP 高的最大延迟；目标值需产品安全分析批准；
2. PSTOP、CHG、DSG、PCHG、PDSG、Q4 gate、SC 电感电流和包电流同步波形；
3. 上电、选择性 FET 转移及每一步 I2C 断线时不存在未请求 gate 脉冲；
4. BQ SDA/SCL stuck、SC SDA/SCL stuck/clock stretch、EEPROM stuck、UART BUSY、CAN bus-off；
5. TS1/TS3 开路、短路、偏差和温箱标定；
6. watchdog reset、BOR、HardFault 时 BQ 四路 FET 的真实行为；
7. 0–15°C 充电限流、完整 CC/CV/EOC、45°C 边界、均衡温升；
8. NVM 撕裂写入、旧格式迁移、SOC 恢复与满/空完成事件；
9. 每个任务 WCET、最小 stack watermark、deadline miss 长稳统计。

## 8. 已知硬件限制

当前原理图审阅显示 BQ 的 CFETOFF/DFETOFF 没有连接到 MCU 独立安全输出，而被复用或未连接；CPU 卡死时软件只能立即控制 SC8815 PSTOP，不能通过独立 GPIO 强制关闭 BQ 主 FET。IWDG 复位后由启动流程重新建立 BQ 全关状态可以缩短暴露时间，但不能等价于独立硬件 safety gate。

因此，在验证 watchdog/reset 期间的真实 CHG/DSG 波形之前，“无人值守 100 分”必须保持 `Blocked by HIL`；若系统安全目标要求 MCU 单故障下立即断开主充放电路径，下一版硬件应增加独立 CFETOFF/BOTHOFF 或外部安全关断链路。
