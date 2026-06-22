# BQ76952 BSP 寄存器事实报告

记录日期：2026-06-22

本报告来自本地 PDF 抽取，用于 `Int_BQ76952_BSP.h` 首版。这里只记录可追溯事实和首版建议，不写驱动代码。

## 1. 读取文件

- `official_chip_docs_files/TI_BQ76952_Technical_Reference_Manual_sluuby2b.pdf`
- `official_chip_docs_files/TI_BQ769x2_Software_Development_Guide_sluaa11.pdf`
- `docs/rules/hardware_rules.md`
- 旧项目 `旧项目信息/旧项目代码/ups/ups001/MDK-ARM/Int/Int_BQ769_BSP.h`

## 2. 直接事实

### 2.1 通信模型

来源：TRM page 13-14、73-75；Software Guide page 3、6、12、19-20。

- 默认 I2C 8-bit 写地址为 `0x10`，8-bit 读地址为 `0x11`。
- 若代码使用 7-bit HAL 地址，应使用 `0x08`；若使用 STM32 HAL 常见左移地址参数，则传入 `0x10`。
- BQ76952 默认上电为 I2C fast mode，支持 100 kHz 和 400 kHz I2C。
- I2C CRC 可选，由 `Settings:Configuration:Comm Type` 控制。
- CRC 多项式为 `x^8 + x^2 + x + 1`，多项式值 `0x07`，初始值 `0x00`。
- 子命令和 Data Memory 访问使用同一套间接窗口：
  - `0x3E`：子命令或 Data Memory 地址低字节。
  - `0x3F`：子命令或 Data Memory 地址高字节。
  - `0x40`-`0x5F`：32-byte transfer buffer。
  - `0x60`：buffer checksum。
  - `0x61`：buffer length。
- 写 Data Memory 时，checksum 计算范围为 `0x3E/0x3F` 两个地址字节加 buffer 数据字节，结果取 8-bit sum 后按位取反。
- `0x61` 的 length 等于 buffer 数据长度加 4，也就是加上 `0x3E/0x3F/0x60/0x61` 四个字节。

### 2.2 Direct Commands

来源：TRM page 91-93。

| 地址 | 名称 | 类型/单位 | BSP 首版用途 |
| --- | --- | --- | --- |
| `0x00` | `Control Status` | H2 | 设备状态，只读；不建议写 |
| `0x02` | `Safety Alert A` | H1 | 保护告警 A |
| `0x03` | `Safety Status A` | H1 | 保护故障 A |
| `0x04` | `Safety Alert B` | H1 | 保护告警 B |
| `0x05` | `Safety Status B` | H1 | 保护故障 B |
| `0x06` | `Safety Alert C` | H1 | 保护告警 C |
| `0x07` | `Safety Status C` | H1 | 保护故障 C |
| `0x0A` | `PF Alert A` | H1 | 永久失效告警 A |
| `0x0B` | `PF Status A` | H1 | 永久失效状态 A |
| `0x0C` | `PF Alert B` | H1 | 永久失效告警 B |
| `0x0D` | `PF Status B` | H1 | 永久失效状态 B |
| `0x0E` | `PF Alert C` | H1 | 永久失效告警 C |
| `0x0F` | `PF Status C` | H1 | 永久失效状态 C |
| `0x10` | `PF Alert D` | H1 | 永久失效告警 D |
| `0x11` | `PF Status D` | H1 | 永久失效状态 D |
| `0x12` | `Battery Status` | H2 | 电池/配置模式状态 |
| `0x14`-`0x32` | `Cell 1-16 Voltage` | I2, mV | 电芯电压，2 byte little endian |
| `0x34` | `Stack Voltage` | I2, userV | 总堆叠电压 |
| `0x36` | `PACK Pin Voltage` | I2, userV | PACK 引脚电压 |
| `0x38` | `LD Pin Voltage` | I2, userV | LD 引脚电压 |
| `0x3A` | `CC2 Current` | I2, userA | 16-bit 电流测量 |
| `0x62` | `Alarm Status` | H2 | ALERT 锁存状态，写 1 清除 |
| `0x64` | `Alarm Raw Status` | H2 | 未锁存告警源 |
| `0x66` | `Alarm Enable` | H2 | Alarm Status 掩码 |
| `0x68` | `Int Temperature` | I2, 0.1 K | 内部温度 |
| `0x70` | `TS1 Temperature` | I2, 0.1 K | TS1 温度 |
| `0x72` | `TS2 Temperature` | I2, 0.1 K | TS2 温度 |
| `0x74` | `TS3 Temperature` | I2, 0.1 K | TS3 温度 |
| `0x7F` | `FET Status` | H1 | FET 和 ALERT 引脚状态 |

### 2.3 首版应定义的 Direct Command bit masks

来源：TRM page 94-106。

- Safety A bit：
  - bit7 `SCD`：放电短路保护。
  - bit6 `OCD2`：放电过流二级保护。
  - bit5 `OCD1`：放电过流一级保护。
  - bit4 `OCC`：充电过流保护。
  - bit3 `COV`：单体过压保护。
  - bit2 `CUV`：单体欠压保护。
- Safety B bit：
  - bit7 `OTF`：FET 过温。
  - bit6 `OTINT`：内部过温。
  - bit5 `OTD`：放电过温。
  - bit4 `OTC`：充电过温。
  - bit2 `UTINT`：内部低温。
  - bit1 `UTD`：放电低温。
  - bit0 `UTC`：充电低温。
- Safety C bit：
  - bit7 `OCD3`：放电过流三级保护。
  - bit6 `SCDL`：放电短路锁存。
  - bit5 `OCDL`：放电过流锁存。
  - bit4 `COVL`：单体过压锁存。
  - bit2 `PTO`：预充超时。
  - bit1 `HWDF`：主机看门狗故障。
- Alarm bit：
  - bit15 `SSBC`、bit14 `SSA`、bit13 `PF`、bit12 `MSK_SFALERT`、bit11 `MSK_PFALERT`。
  - bit10 `INITSTART`、bit9 `INITCOMP`、bit7 `FULLSCAN`、bit6 `XCHG`、bit5 `XDSG`。
  - bit4 `SHUTV`、bit3 `FUSE`、bit2 `CB`、bit1 `ADSCAN`、bit0 `WAKE`。
- Battery Status 首版关注 bit：
  - bit4 `WD`：上次复位是否由内部 watchdog 触发。
  - bit3 `POR`：完整复位发生，退出 CONFIG_UPDATE 后清除。
  - bit2 `SLEEP_EN`：SLEEP 是否被允许。
  - bit1 `PCHG_MODE`：是否在预充模式。
  - bit0 `CFGUPDATE`：是否处于 CONFIG_UPDATE。
- FET Status bit：
  - bit6 `ALRT_PIN`、bit5 `DDSG_PIN`、bit4 `DCHG_PIN`。
  - bit3 `PDSG_FET`、bit2 `DSG_FET`、bit1 `PCHG_FET`、bit0 `CHG_FET`。

### 2.4 Command-only Subcommands

来源：TRM page 106-108；Software Guide page 6-7、19-20。

| 子命令 | 名称 | 首版用途 |
| --- | --- | --- |
| `0x000E` | `EXIT_DEEPSLEEP` | 退出 DEEPSLEEP |
| `0x000F` | `DEEPSLEEP` | 进入 DEEPSLEEP，要求 4 s 内连续发送两次 |
| `0x0010` | `SHUTDOWN` | 进入 SHUTDOWN 序列 |
| `0x0012` | `RESET` | 复位设备 |
| `0x0022` | `FET_ENABLE` | 切换 Manufacturing Status 中的 FET_EN |
| `0x0030` | `SEAL` | 进入 sealed |
| `0x0090` | `SET_CFGUPDATE` | 进入 CONFIG_UPDATE |
| `0x0092` | `EXIT_CFGUPDATE` | 退出 CONFIG_UPDATE，并清除部分电池状态标志 |
| `0x0093` | `DSG_PDSG_OFF` | 关闭 DSG/PDSG |
| `0x0094` | `CHG_PCHG_OFF` | 关闭 CHG/PCHG |
| `0x0095` | `ALL_FETS_OFF` | 关闭 CHG/DSG/PCHG/PDSG |
| `0x0096` | `ALL_FETS_ON` | 允许 FET 在安全条件满足时打开 |
| `0x0099` | `SLEEP_ENABLE` | 允许 SLEEP |
| `0x009A` | `SLEEP_DISABLE` | 禁止 SLEEP |
| `0x29BC` | `SWAP_COMM_MODE` | 切换到 Data Memory 指定通信模式 |
| `0x29E7` | `SWAP_TO_I2C` | 立即切换到 I2C fast |

### 2.5 Subcommands with data

来源：TRM page 108-116；Software Guide page 6-7、19-20。

| 子命令 | 名称 | 首版用途 |
| --- | --- | --- |
| `0x0001` | `DEVICE_NUMBER` | 读取器件编号 |
| `0x0002` | `FW_VERSION` | 读取固件版本 |
| `0x0003` | `HW_VERSION` | 读取硬件版本 |
| `0x0057` | `MANUFACTURING_STATUS` | 读取制造状态 |
| `0x0071`-`0x0077` | `DASTATUS1`-`DASTATUS7` | 扩展采样状态，首版可先只定义 |
| `0x0083` | `CB_ACTIVE_CELLS` | 主机控制均衡 cell mask |
| `0x0084` | `CB_SET_LVL` | 按阈值设置均衡 |
| `0x0085`-`0x0087` | `CBSTATUS1`-`CBSTATUS3` | 均衡状态 |
| `0x0097` | `FET_CONTROL` | 主机控制单个 FET off bit |
| `0x0098` | `REG12_CONTROL` | REG1/REG2 控制 |

FET_CONTROL bit 来源：TRM page 120。

- bit3 `PCHG_OFF`：强制 PCHG 关闭。
- bit2 `CHG_OFF`：强制 CHG 关闭。
- bit1 `PDSG_OFF`：强制 PDSG 关闭。
- bit0 `DSG_OFF`：强制 DSG 关闭。

### 2.6 Data Memory 首版地址

来源：TRM page 123、154-176、197-201。

| 地址 | 名称 | 类型/默认值 | 首版用途 |
| --- | --- | --- | --- |
| `0x9234` | `Power Config` | H2, default `0x2982` | 电源/SLEEP 配置入口 |
| `0x9239` | `Comm Type` | U1, default `0x00` | I2C/SPI/HDQ 和 CRC 配置 |
| `0x923A` | `I2C Address` | U1, default `0x00` | I2C 地址配置 |
| `0x9303` | `DA Configuration` | H1, default `0x05` | DA/温度配置入口 |
| `0x9304` | `Vcell Mode` | H2, default `0x0000` | 配置哪些 cell 有效 |
| `0x925F` | `Protection Configuration` | H2, default `0x0002` | 保护总配置 |
| `0x9261` | `Enabled Protections A` | U1, default `0x88` | 开启 SCD/COV 等 A 类保护 |
| `0x9262` | `Enabled Protections B` | U1, default `0x00` | 开启温度类保护 |
| `0x9263` | `Enabled Protections C` | U1, default `0x00` | 开启锁存/看门狗等保护 |
| `0x9265` | `CHG FET Protections A` | U1, default `0x98` | A 类故障关闭 CHG |
| `0x9266` | `CHG FET Protections B` | U1, default `0xD5` | B 类故障关闭 CHG |
| `0x9267` | `CHG FET Protections C` | U1, default `0x56` | C 类故障关闭 CHG |
| `0x9269` | `DSG FET Protections A` | U1, default `0xE4` | A 类故障关闭 DSG |
| `0x926A` | `DSG FET Protections B` | U1, default `0xE6` | B 类故障关闭 DSG |
| `0x926B` | `DSG FET Protections C` | U1, default `0xE2` | C 类故障关闭 DSG |
| `0x926D` | `Default Alarm Mask` | H2, default `0xF800` | Alarm Enable 默认值 |
| `0x9308` | `FET Options` | H1, default `0x0D` | FET 控制策略 |
| `0x9309` | `Chg Pump Control` | U1, default `0x01` | 电荷泵 |
| `0x9335` | `Balancing Configuration` | H1, default `0x00` | 均衡配置 |
| `0x9336` | `Min Cell Temp` | I1, default `-20 C` | 均衡温度下限 |
| `0x9337` | `Max Cell Temp` | I1, default `60 C` | 均衡温度上限 |
| `0x9338` | `Max Internal Temp` | I1, default `70 C` | 均衡内部温度上限 |
| `0x9339` | `Cell Balance Interval` | U1, default `20 s` | 均衡保持/重算间隔 |
| `0x933A` | `Cell Balance Max Cells` | U1, default `1` | 同时自动均衡最大 cell 数 |
| `0x933B` | `Cell Balance Min Cell V (Charge)` | I2, default `3900 mV` | 充电均衡电压门限 |
| `0x933D` | `Cell Balance Min Delta (Charge)` | U1, default `40 mV` | 充电均衡开启压差 |
| `0x933E` | `Cell Balance Stop Delta (Charge)` | U1, default `20 mV` | 充电均衡停止压差 |

### 2.7 Data Memory bit masks

来源：TRM page 155、157-163、168、174-175。

- `Vcell Mode`：
  - bit15-bit0 分别表示 Cell 16 到 Cell 1 是否连接。
  - bit 为 1 表示对应 cell 连接并启用保护；bit 为 0 表示未连接。
- `Enabled Protections A/B/C` bit 与 Direct Command 的 Safety A/B/C bit 对齐。
- `FET Options`：
  - bit5 `FET_INIT_OFF`：上电后等待 host 命令再允许 FET 打开。
  - bit4 `PDSG_EN`：允许 PDSG 预放电。
  - bit3 `FET_CTRL_EN`：允许器件控制 FET。
  - bit2 `HOST_FET_EN`：允许 host 命令关闭 FET。
  - bit1 `SLEEPCHG`：SLEEP 中是否允许 CHG FET。
  - bit0 `SFET`：串联 FET 模式，启用 body diode protection。
- `Balancing Configuration`：
  - bit4 `CB_NO_CMD`：阻止 host 控制均衡命令。
  - bit3 `CB_NOSLEEP`：均衡时阻止 SLEEP。
  - bit2 `CB_SLEEP`：允许 SLEEP 中均衡。
  - bit1 `CB_RLX`：relax 条件允许自动均衡。
  - bit0 `CB_CHG`：充电条件允许自动均衡。

## 3. 推断

- `Int_BQ76952_BSP.h` 首版应以宏定义和 bit mask 为主，不定义读写函数；函数放到后续 `Int_BQ76952.c/.h`。
- 由于新项目是 6S，BSP 可定义 6S 相关 mask，但不能把 BQ76952 只写成 6S 专用芯片；寄存器本身仍覆盖 16 cell direct command。
- 当前硬件 VC 映射不是连续 `VC0-VC6`，`Vcell Mode` 的 6S mask 必须后续按 TI 推荐接法和原理图复核后再冻结。

## 4. 不确定项

- BQ76952 的 7-bit 地址/8-bit 地址在 HAL 中如何传参，需要等 `Int_BQ76952` 通信实现时结合 STM32 HAL 确认；BSP 先同时给出 8-bit 和 7-bit 常量并加注释。
- 是否默认启用 I2C CRC，需要用户或 bring-up 策略确认；BSP 只定义 CRC 多项式和通信模式值，不强制策略。
- 温度传感器数量、TS1/TS2/TS3 实际接法尚未完全冻结，BSP 只定义 direct commands。
- `Vcell Mode` 对当前硬件的最终 6S mask 需要硬件事实 agent 结合原理图再次确认。

## 5. 对当前模块的影响

- 可以进入 `docs/pre/int_bq76952_bsp_pre.md`。
- 可以签发 `BQ76952_BSP_HEADER_001` 任务卡，让 worker 只写 `Int/Int_BQ76952_BSP.h`。
- 首版 BSP.h 只允许写宏、枚举和注释，不允许写 HAL 访问函数，不允许引入旧 BQ76930 寄存器。

## 6. 文件改动

- 本报告由主线程写入：`docs/agent_reports/bq76952_bsp_register_facts_2026-06-22.md`。
