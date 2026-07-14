# 硬件接口预留与职责边界

记录日期：2026-06-23  
更新日期：2026-07-12

## 1. 当前结论

当前硬件逻辑按以下原则冻结：

- BQ76952 负责 BMS AFE、采样、告警、FET 和均衡底层能力；策略放在 APP/COM，不放在 INT 通信层。
- SC8815 负责 6S 充电控制；软件只做安全态、使能、状态读取、ADC 监测和保守限流，不重写芯片内部 CC/CV 微观环路。
- 当前运行目标：充电 `5A`，放电 `10A`；完整放电测试已通过当前阶段验证。
- 新版硬件已删除原 BQ WAKE/TS2 外部通道，软件不得依赖或驱动 PB3 唤醒。
- BQ 默认通信使用 CRC disabled，INT 层仍保留 CRC 支持。

本文只定义硬件接口和分层边界，不把未测试结果写成已验证事实。

## 2. 资料入口

- `docs/rules/hardware_rules.md`
- `docs/wordflow/manual_confirmations.md`
- `docs/state/project_closeout_plan.md`
- `full_netlist (4).csv`
- `full_netlist (5).csv`
- `Int/Int_BQ76952_BSP.h`
- `Int/Int_BQ76952.h`
- `Int/Int_SC8815_BSP.h`
- `Int/Int_SC8815.h`

## 3. BQ76952 接口边界

### 3.1 当前硬件接口

| 接口 | 网络/引脚 | 用途 | 当前要求 |
| --- | --- | --- | --- |
| I2C1 SCL | `PB6` | BQ76952 通信 | CubeMX I2C1；总线和 BQ 事务由 BQ INT 层管理 |
| I2C1 SDA | `PB7` | BQ76952 通信 | 与 SCL 同步确认上拉和速率 |
| I2C 地址/CRC | `0x08 / 0x10 / 0x11` | 通信格式 | 项目默认 CRC disabled；INT 保留 CRC on/off 支持 |
| ALERT/INT | `PB4` | 告警/事件 | 配 EXTI；ISR 只置 pending flag，不访问 I2C |
| WAKE/TS2 | 原 `PB3` | 历史唤醒接口 | 新版硬件已删除；不得保留为当前必需接口，不得驱动 PB3 假设唤醒 |
| CHIP_SHUT/RST_SHUT | `BMS_CHIP_SHUT` | 复位/关断 | 危险控制入口；时序放 BSP/GPIO 层并需实板验证 |
| ONLINE | `BMS_CHIP_ONLINE / REG18` | BQ 在线状态 | 作为输入/状态证据使用 |
| 电芯接口 | `CN2 1x7P` | 6S 采样 | 物理 Cell1..6 对应 BQ Cell1/2/6/9/12/16 |
| 低边采样 | `R18=5mΩ/6W` | 电流测量 | 当前硬件基线；电流方向、零点和增益继续用实测闭环 |
| TS1/TS3 | 热敏电阻 | 热保护/补偿 | NTC 型号、B 值和分压参数待确认 |

跨板接口只保留当前实物实际存在的 `ONLINE`、`ALERT`、`I2C1`、`CHIP_SHUT` 和电源相关信号。旧资料中的 WAKE 脚只作为历史证据。

### 3.2 BQ INT 层允许内容

- direct command、subcommand、Data Memory 读写。
- ConfigUpdate 进入/退出和必要轮询。
- 读取单体、Stack/Pack、电流、温度、Alarm、Safety、PF、FET status。
- host controlled balancing mask 的受控底层入口。
- CRC 支持和错误返回。
- RST_SHUT/在线状态等板级 GPIO 的薄封装。

### 3.3 BQ INT 层禁止内容

- 自动均衡策略。
- SOC/SOH 算法。
- 保护阈值业务选择。
- 充放电、预放电和故障恢复业务状态机。
- 在中断中访问 I2C。
- 依赖已删除的 PB3 WAKE 通道。

### 3.4 APP/COM 职责

COM 层负责算法、单位换算和语义快照；APP 层负责保护业务、FET 策略、PDSG 预放电、故障锁存/恢复、显示和通信上报。

## 4. SC8815 接口边界

### 4.1 当前硬件接口

| 接口 | 网络/引脚 | 用途 | 当前要求 |
| --- | --- | --- | --- |
| 软件 IIC SCL | `PA7` | SC8815 SCL | GPIO 开漏，禁止改成硬件 I2C3 |
| 软件 IIC SDA | `PA6` | SC8815 SDA | GPIO 开漏/读回能力，保留线序诊断 |
| INT | `PA5` | 充电状态/故障中断 | **必须在 CubeMX 配为 EXTI**；ISR 只置 pending flag |
| #CE | `PB1` | 芯片使能 | 低有效；安全态高电平 disable |
| PSTOP | `PB0` | 功率级停机 | 高电平 standby；低电平允许功率级工作 |
| VBATS 分压 | `R17=200kΩ`、`R18=10kΩ` | 6S 目标电压 | 名义 `25.2V`；实际截止由充电曲线测试确认 |
| 采样电阻 | `R5/R14=10mΩ` | 输入/电池电流 | 对应 `IBUS_RATIO=3`、`IBAT_RATIO=6` |

### 4.2 SC INT 层允许内容

- `InitSafe` 建立 `PSTOP=high`、`#CE=high`。
- 软件 IIC 读写、ACK、错误返回、线序诊断。
- 芯片使能/禁用和 standby/work 控制。
- STATUS、AC_OK、INDET、VBUS_SHORT、OTP、EOC 读取。
- VBUS/VBAT/ADIN/IBUS/IBAT ADC 读取。
- 输入限流、电池侧限流和受 guard 保护的寄存器写入。
- EXTI pending flag 的板级入口。

### 4.3 SC 软件禁止内容

- 软件实现 CC/CV、涓流、终止等芯片内部微观充电环路。
- 用 #CE/PSTOP 高频模拟充电阶段。
- OTG、反向输出和 VBUSREF 输出 API。
- 关闭关键 OVP、终止、短路折返和温度保护。
- 在非 standby 状态修改关键配置。
- 在 EXTI ISR 中访问软件 IIC。

## 5. 系统级接口边界

| 模块 | 硬件接口 | INT 层职责 | APP/COM 职责 |
| --- | --- | --- | --- |
| OLED | I2C2 `PA11/PA12` | 初始化、命令/数据写入 | 页面、文案、告警显示 |
| EEPROM | I2C2 `PA11/PA12` | 字节/页读写、边界保护 | 参数 schema、CRC、版本迁移 |
| CAN-FD | `PB9 TX`、`PB8 RX` | FDCAN 基础收发 | ID、帧格式、控制命令、ICD |
| USART/SWD | `PA9/PA10`、`PA13/PA14` | 调试通道 | AI Debug CLI、日志和测试采集 |
| GPIO Board | `PB13/PB14/PB5/PD3`、ALERT/ONLINE/SHUT | LED、蜂鸣器、按键、板级 GPIO | 告警策略、按键业务、故障恢复 |
| 电源路径 | `24V_IN/VBUS/BMS+/BMS_OUT+/BMS_OUT-` | 有 MCU 可测/控脚时提供薄接口 | 功率策略、充放电和预放电状态机 |

项目要求的 CAN、EEPROM 等必需外设初始化失败时，不得进入正常 RUN；由 APP 初始化阶段决定安全故障态。

## 6. Bring-up 与闭环前必须确认

- SC8815 PA5 EXTI 触发、pending flag 和任务读状态链路。
- SC8815 `PA6/PA7` 软件 IIC 波形、ACK、上拉和时序。
- SC8815 `#CE/PSTOP` 默认电平。
- SC8815 VBATS、PACK、实际截止电压和 EOC。
- SC8815 `FB/ADIN` 外部连接。
- BQ 6S 映射、5mΩ 电流读数、带数据 subcommand 和 readback。
- PDSG 生产状态机的成功、超时和失败关断。
- TS1/TS3 参数与实测温度。
- OLED 地址、EEPROM 参数和 CAN 协议。

## 7. 下一步

代码清洗只处理 `docs/state/project_closeout_plan.md` 中的 Must fix 项。清洗完成后按 `docs/test/ai_charge_curve_test_task.md` 执行充电测试；测试结果闭环后再定稿完整项目开发文档。
