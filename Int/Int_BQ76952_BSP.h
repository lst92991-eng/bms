#ifndef INT_BQ76952_BSP_H
#define INT_BQ76952_BSP_H

#include <stdint.h>

/*
 * BQ76952 BSP 首版常量表。
 * 来源：BQ76952 TRM / BQ769x2 Software Development Guide，以及本项目硬件规则。
 * 本文件只定义地址、子命令、bit mask、单位和风险注释；不实现 HAL/FreeRTOS/I2C 访问函数。
 */

/*---------------------------------------------------------------------------
 * 芯片基本参数
 *---------------------------------------------------------------------------*/
#define BQ76952_CELL_COUNT_MAX                  (16u)     /* BQ76952 最多支持 16 串，不能把芯片写死为 6S。 */
#define BQ76952_PROJECT_SERIES_CELL_COUNT       (6u)      /* 本项目目标电池为 6S 三元锂 21700，仅作硬件事实说明。 */
#define BQ76952_SHUNT_RESISTOR_UOHM             (500u)    /* 低边采样电阻 0.5 mΩ/6W；电流换算需按实际校准确认。 */
#define BQ76952_FULL_CHARGE_MV                  (25200u)  /* 6S 满充 25.2V；这是系统参数，不是 BQ 保护阈值。 */

#define BQ76952_PROJECT_CELL_FULL_MV            (4200u)   /* 6S ternary Li-ion: one cell full voltage is 4.2V. */
#define BQ76952_SC8815_TARGET_CHARGE_MV         (24730u)  /* SC8815 target charge voltage: about 24.73V. */
#define BQ76952_BALANCE_RESISTOR_OHM            (62u)     /* Passive balance bleed resistor on each cell branch. */
#define BQ76952_BALANCE_RESISTOR_POWER_MW       (1000u)   /* Balance resistor rated power: 1W. */

/*---------------------------------------------------------------------------
 * I2C / CRC / transfer buffer
 *---------------------------------------------------------------------------*/
#define BQ76952_I2C_7BIT_ADDR_DEFAULT           (0x08u)   /* 默认 7-bit I2C 地址；若底层 API 要 7-bit 地址则传此值。 */
#define BQ76952_I2C_8BIT_WRITE_ADDR_DEFAULT     (0x10u)   /* 默认 8-bit 写地址；STM32 HAL 常用左移地址参数通常传此值。 */
#define BQ76952_I2C_8BIT_READ_ADDR_DEFAULT      (0x11u)   /* 默认 8-bit 读地址；不要误当 7-bit 地址使用。 */
#define BQ76952_I2C_CRC_DEFAULT_ENABLED         (0u)      /* Bring-up default: current BQ board is responding in non-CRC I2C mode. */
#define BQ76952_I2C_STANDARD_MODE_HZ            (100000u) /* 支持 100 kHz I2C；上拉和线长需硬件确认。 */
#define BQ76952_I2C_FAST_MODE_HZ                (400000u) /* 默认 fast mode 可用 400 kHz；bring-up 阶段可先降速。 */

#define BQ76952_CRC8_POLY                       (0x07u)   /* I2C CRC 多项式 x^8+x^2+x+1；是否启用由 COM 按项目通信格式设置。 */
#define BQ76952_CRC8_INIT                       (0x00u)   /* I2C CRC 初始值 0x00；本项目默认启用，INT 层仍保留开关。 */

#define BQ76952_SUBCMD_ADDR_LSB                 (0x3Eu)   /* 子命令或 Data Memory 地址低字节窗口。 */
#define BQ76952_SUBCMD_ADDR_MSB                 (0x3Fu)   /* 子命令或 Data Memory 地址高字节窗口。 */
#define BQ76952_TRANSFER_BUFFER_START           (0x40u)   /* 32-byte transfer buffer 起始地址。 */
#define BQ76952_TRANSFER_BUFFER_END             (0x5Fu)   /* 32-byte transfer buffer 结束地址。 */
#define BQ76952_TRANSFER_BUFFER_SIZE            (32u)     /* transfer buffer 数据区长度，单位 byte。 */
#define BQ76952_TRANSFER_CHECKSUM               (0x60u)   /* buffer checksum；Data Memory 写入时为 sum 后按位取反。 */
#define BQ76952_TRANSFER_LENGTH                 (0x61u)   /* buffer length；写 Data Memory 时等于数据长度 + 4。 */
#define BQ76952_TRANSFER_LENGTH_OVERHEAD        (4u)      /* length 额外包含 0x3E/0x3F/0x60/0x61 四个字节。 */

/*---------------------------------------------------------------------------
 * Direct Commands
 *---------------------------------------------------------------------------*/
#define BQ76952_CMD_CONTROL_STATUS              (0x00u)   /* H2，推荐只读设备状态；不要当控制寄存器写入。 */
#define BQ76952_CMD_SAFETY_ALERT_A              (0x02u)   /* H1，保护告警 A；bit 与 Safety A mask 对齐。 */
#define BQ76952_CMD_SAFETY_STATUS_A             (0x03u)   /* H1，保护故障 A；读取故障锁存/当前状态。 */
#define BQ76952_CMD_SAFETY_ALERT_B              (0x04u)   /* H1，保护告警 B；温度类告警入口。 */
#define BQ76952_CMD_SAFETY_STATUS_B             (0x05u)   /* H1，保护故障 B；温度类故障入口。 */
#define BQ76952_CMD_SAFETY_ALERT_C              (0x06u)   /* H1，保护告警 C；锁存/看门狗等告警入口。 */
#define BQ76952_CMD_SAFETY_STATUS_C             (0x07u)   /* H1，保护故障 C；锁存/看门狗等故障入口。 */
#define BQ76952_CMD_PF_ALERT_A                  (0x0Au)   /* H1，永久失效告警 A；PF 处理需谨慎。 */
#define BQ76952_CMD_PF_STATUS_A                 (0x0Bu)   /* H1，永久失效状态 A；不要在 bring-up 中随意清除。 */
#define BQ76952_CMD_PF_ALERT_B                  (0x0Cu)   /* H1，永久失效告警 B；用于诊断。 */
#define BQ76952_CMD_PF_STATUS_B                 (0x0Du)   /* H1，永久失效状态 B；用于诊断。 */
#define BQ76952_CMD_PF_ALERT_C                  (0x0Eu)   /* H1，永久失效告警 C；用于诊断。 */
#define BQ76952_CMD_PF_STATUS_C                 (0x0Fu)   /* H1，永久失效状态 C；用于诊断。 */
#define BQ76952_CMD_PF_ALERT_D                  (0x10u)   /* H1，永久失效告警 D；用于诊断。 */
#define BQ76952_CMD_PF_STATUS_D                 (0x11u)   /* H1，永久失效状态 D；用于诊断。 */
#define BQ76952_CMD_BATTERY_STATUS              (0x12u)   /* H2，电池/配置模式状态；CFGUPDATE/POR 等在此读取。 */

#define BQ76952_CMD_CELL1_VOLTAGE               (0x14u)   /* I2，Cell 1 电压，单位 mV，little-endian。 */
#define BQ76952_CMD_CELL2_VOLTAGE               (0x16u)   /* I2，Cell 2 电压，单位 mV，little-endian。 */
#define BQ76952_CMD_CELL3_VOLTAGE               (0x18u)   /* I2，Cell 3 电压，单位 mV；6S 硬件未必使用此通道。 */
#define BQ76952_CMD_CELL4_VOLTAGE               (0x1Au)   /* I2，Cell 4 电压，单位 mV；6S 硬件未必使用此通道。 */
#define BQ76952_CMD_CELL5_VOLTAGE               (0x1Cu)   /* I2，Cell 5 电压，单位 mV；6S 硬件未必使用此通道。 */
#define BQ76952_CMD_CELL6_VOLTAGE               (0x1Eu)   /* I2，Cell 6 电压，单位 mV，little-endian。 */
#define BQ76952_CMD_CELL7_VOLTAGE               (0x20u)   /* I2，Cell 7 电压，单位 mV；6S 硬件未必使用此通道。 */
#define BQ76952_CMD_CELL8_VOLTAGE               (0x22u)   /* I2，Cell 8 电压，单位 mV；6S 硬件未必使用此通道。 */
#define BQ76952_CMD_CELL9_VOLTAGE               (0x24u)   /* I2，Cell 9 电压，单位 mV，little-endian。 */
#define BQ76952_CMD_CELL10_VOLTAGE              (0x26u)   /* I2，Cell 10 电压，单位 mV；6S 硬件未必使用此通道。 */
#define BQ76952_CMD_CELL11_VOLTAGE              (0x28u)   /* I2，Cell 11 电压，单位 mV；6S 硬件未必使用此通道。 */
#define BQ76952_CMD_CELL12_VOLTAGE              (0x2Au)   /* I2，Cell 12 电压，单位 mV，little-endian。 */
#define BQ76952_CMD_CELL13_VOLTAGE              (0x2Cu)   /* I2，Cell 13 电压，单位 mV；6S 硬件未必使用此通道。 */
#define BQ76952_CMD_CELL14_VOLTAGE              (0x2Eu)   /* I2，Cell 14 电压，单位 mV；6S 硬件未必使用此通道。 */
#define BQ76952_CMD_CELL15_VOLTAGE              (0x30u)   /* I2，Cell 15 电压，单位 mV；6S 硬件未必使用此通道。 */
#define BQ76952_CMD_CELL16_VOLTAGE              (0x32u)   /* I2，Cell 16 电压，单位 mV，little-endian。 */

#define BQ76952_CMD_STACK_VOLTAGE               (0x34u)   /* I2，总堆叠电压，单位 userV；按 TRM 校准单位解析。 */
#define BQ76952_CMD_PACK_PIN_VOLTAGE            (0x36u)   /* I2，PACK 引脚电压，单位 userV；不要直接当 mV。 */
#define BQ76952_CMD_LD_PIN_VOLTAGE              (0x38u)   /* I2，LD 引脚电压，单位 userV；用于负载检测诊断。 */
#define BQ76952_CMD_CC2_CURRENT                 (0x3Au)   /* I2，CC2 电流，单位 userA；必须结合采样电阻和校准。 */
#define BQ76952_CMD_ALARM_STATUS                (0x62u)   /* H2，ALERT 锁存状态；写 1 清除对应 bit，清除前先记录。 */
#define BQ76952_CMD_ALARM_RAW_STATUS            (0x64u)   /* H2，未锁存告警源；用于判断实时告警。 */
#define BQ76952_CMD_ALARM_ENABLE                (0x66u)   /* H2，Alarm Status 使能掩码；写错会丢 ALERT 中断。 */
#define BQ76952_CMD_INT_TEMPERATURE             (0x68u)   /* I2，内部温度，单位 0.1 K；换算摄氏度需减 273.15。 */
#define BQ76952_CMD_TS1_TEMPERATURE             (0x70u)   /* I2，TS1 温度，单位 0.1 K；本项目为靠近采样电阻的热敏电阻。 */
#define BQ76952_CMD_TS2_TEMPERATURE             (0x72u)   /* I2，TS2 温度，单位 0.1 K；本项目主要走 BMS_WAKE 唤醒路径。 */
#define BQ76952_CMD_TS3_TEMPERATURE             (0x74u)   /* I2，TS3 温度，单位 0.1 K；本项目为靠近采样电阻的热敏电阻。 */
#define BQ76952_CMD_FET_STATUS                  (0x7Fu)   /* H1，FET 与 ALERT 引脚状态；只反映状态，不等价于控制命令。 */

/*---------------------------------------------------------------------------
 * Direct Command bit mask
 *---------------------------------------------------------------------------*/
#define BQ76952_SAFETY_A_SCD_MASK               (0x80u)   /* bit7，放电短路保护。 */
#define BQ76952_SAFETY_A_OCD2_MASK              (0x40u)   /* bit6，放电过流二级保护。 */
#define BQ76952_SAFETY_A_OCD1_MASK              (0x20u)   /* bit5，放电过流一级保护。 */
#define BQ76952_SAFETY_A_OCC_MASK               (0x10u)   /* bit4，充电过流保护。 */
#define BQ76952_SAFETY_A_COV_MASK               (0x08u)   /* bit3，单体过压保护。 */
#define BQ76952_SAFETY_A_CUV_MASK               (0x04u)   /* bit2，单体欠压保护。 */

#define BQ76952_SAFETY_B_OTF_MASK               (0x80u)   /* bit7，FET 过温保护。 */
#define BQ76952_SAFETY_B_OTINT_MASK             (0x40u)   /* bit6，内部过温保护。 */
#define BQ76952_SAFETY_B_OTD_MASK               (0x20u)   /* bit5，放电过温保护。 */
#define BQ76952_SAFETY_B_OTC_MASK               (0x10u)   /* bit4，充电过温保护。 */
#define BQ76952_SAFETY_B_UTINT_MASK             (0x04u)   /* bit2，内部低温保护。 */
#define BQ76952_SAFETY_B_UTD_MASK               (0x02u)   /* bit1，放电低温保护。 */
#define BQ76952_SAFETY_B_UTC_MASK               (0x01u)   /* bit0，充电低温保护。 */

#define BQ76952_SAFETY_C_OCD3_MASK              (0x80u)   /* bit7，放电过流三级保护。 */
#define BQ76952_SAFETY_C_SCDL_MASK              (0x40u)   /* bit6，放电短路锁存。 */
#define BQ76952_SAFETY_C_OCDL_MASK              (0x20u)   /* bit5，放电过流锁存。 */
#define BQ76952_SAFETY_C_COVL_MASK              (0x10u)   /* bit4，单体过压锁存。 */
#define BQ76952_SAFETY_C_PTO_MASK               (0x04u)   /* bit2，预充超时。 */
#define BQ76952_SAFETY_C_HWDF_MASK              (0x02u)   /* bit1，主机看门狗故障。 */

#define BQ76952_ALARM_SSBC_MASK                 (0x8000u) /* bit15，Safety Status B/C 相关告警。 */
#define BQ76952_ALARM_SSA_MASK                  (0x4000u) /* bit14，Safety Status A 相关告警。 */
#define BQ76952_ALARM_PF_MASK                   (0x2000u) /* bit13，永久失效告警；处理前必须记录 PF 状态。 */
#define BQ76952_ALARM_MSK_SFALERT_MASK          (0x1000u) /* bit12，Safety alert 聚合告警。 */
#define BQ76952_ALARM_MSK_PFALERT_MASK          (0x0800u) /* bit11，PF alert 聚合告警。 */
#define BQ76952_ALARM_INITSTART_MASK            (0x0400u) /* bit10，初始化开始。 */
#define BQ76952_ALARM_INITCOMP_MASK             (0x0200u) /* bit9，初始化完成。 */
#define BQ76952_ALARM_FULLSCAN_MASK             (0x0080u) /* bit7，完整 ADC 扫描完成。 */
#define BQ76952_ALARM_XCHG_MASK                 (0x0040u) /* bit6，CHG FET 被关断/禁止相关状态。 */
#define BQ76952_ALARM_XDSG_MASK                 (0x0020u) /* bit5，DSG FET 被关断/禁止相关状态。 */
#define BQ76952_ALARM_SHUTV_MASK                (0x0010u) /* bit4，关断电压相关告警。 */
#define BQ76952_ALARM_FUSE_MASK                 (0x0008u) /* bit3，熔丝相关告警。 */
#define BQ76952_ALARM_CB_MASK                   (0x0004u) /* bit2，cell balancing 状态变化。 */
#define BQ76952_ALARM_ADSCAN_MASK               (0x0002u) /* bit1，ADC scan 状态。 */
#define BQ76952_ALARM_WAKE_MASK                 (0x0001u) /* bit0，唤醒事件。 */

#define BQ76952_BATTERY_STATUS_WD_MASK          (0x10u)   /* bit4，上次复位是否由内部 watchdog 触发。 */
#define BQ76952_BATTERY_STATUS_POR_MASK         (0x08u)   /* bit3，完整复位发生；退出 CONFIG_UPDATE 后清除。 */
#define BQ76952_BATTERY_STATUS_SLEEP_EN_MASK    (0x04u)   /* bit2，SLEEP 是否被允许。 */
#define BQ76952_BATTERY_STATUS_PCHG_MODE_MASK   (0x02u)   /* bit1，是否处于预充模式。 */
#define BQ76952_BATTERY_STATUS_CFGUPDATE_MASK   (0x01u)   /* bit0，是否处于 CONFIG_UPDATE，写配置前必须确认。 */

#define BQ76952_FET_STATUS_ALRT_PIN_MASK        (0x40u)   /* bit6，ALERT 引脚状态。 */
#define BQ76952_FET_STATUS_DDSG_PIN_MASK        (0x20u)   /* bit5，DDSG 引脚状态。 */
#define BQ76952_FET_STATUS_DCHG_PIN_MASK        (0x10u)   /* bit4，DCHG 引脚状态。 */
#define BQ76952_FET_STATUS_PDSG_FET_MASK        (0x08u)   /* bit3，PDSG FET 状态。 */
#define BQ76952_FET_STATUS_DSG_FET_MASK         (0x04u)   /* bit2，DSG FET 状态。 */
#define BQ76952_FET_STATUS_PCHG_FET_MASK        (0x02u)   /* bit1，PCHG FET 状态。 */
#define BQ76952_FET_STATUS_CHG_FET_MASK         (0x01u)   /* bit0，CHG FET 状态。 */

#define BQ76952_MFG_STATUS_OTPW_EN_MASK         (0x0080u) /* bit7，允许运行中写 OTP。 */
#define BQ76952_MFG_STATUS_PF_EN_MASK           (0x0040u) /* bit6，Permanent Fail 使能。 */
#define BQ76952_MFG_STATUS_PDSG_TEST_MASK       (0x0020u) /* bit5，FET Test 模式下 PDSG 使能。 */
#define BQ76952_MFG_STATUS_FET_EN_MASK          (0x0010u) /* bit4，1=Firmware FET Control，0=FET Test Mode。 */
#define BQ76952_MFG_STATUS_DSG_TEST_MASK        (0x0004u) /* bit2，FET Test 模式下 DSG 使能。 */
#define BQ76952_MFG_STATUS_CHG_TEST_MASK        (0x0002u) /* bit1，FET Test 模式下 CHG 使能。 */
#define BQ76952_MFG_STATUS_PCHG_TEST_MASK       (0x0001u) /* bit0，FET Test 模式下 PCHG 使能。 */

/*---------------------------------------------------------------------------
 * Command-only Subcommands
 *---------------------------------------------------------------------------*/
#define BQ76952_SUBCMD_EXIT_DEEPSLEEP           (0x000Eu) /* 退出 DEEPSLEEP；需确认唤醒源和状态。 */
#define BQ76952_SUBCMD_DEEPSLEEP                (0x000Fu) /* 进入 DEEPSLEEP；要求 4s 内连续发送两次。 */
#define BQ76952_SUBCMD_SHUTDOWN                 (0x0010u) /* 进入 SHUTDOWN 序列；会影响 BMS 在线状态，危险命令。 */
#define BQ76952_SUBCMD_RESET                    (0x0012u) /* 复位设备；会中断测量和 FET 状态管理。 */
#define BQ76952_SUBCMD_FET_ENABLE               (0x0022u) /* 切换 Manufacturing Status 的 FET_EN，不等同于强制开 FET。 */
#define BQ76952_SUBCMD_SEAL                     (0x0030u) /* 进入 sealed；调试阶段误发会增加解封复杂度。 */
#define BQ76952_SUBCMD_SET_CFGUPDATE            (0x0090u) /* 进入 CONFIG_UPDATE；写 Data Memory 前必须使用。 */
#define BQ76952_SUBCMD_EXIT_CFGUPDATE           (0x0092u) /* 退出 CONFIG_UPDATE；会清除部分电池状态标志。 */
#define BQ76952_SUBCMD_DSG_PDSG_OFF             (0x0093u) /* 关闭 DSG/PDSG；会切断放电路径。 */
#define BQ76952_SUBCMD_CHG_PCHG_OFF             (0x0094u) /* 关闭 CHG/PCHG；会切断充电路径。 */
#define BQ76952_SUBCMD_ALL_FETS_OFF             (0x0095u) /* 关闭 CHG/DSG/PCHG/PDSG；系统功率路径需先评估。 */
#define BQ76952_SUBCMD_ALL_FETS_ON              (0x0096u) /* 允许 FET 在安全条件满足时打开；不是无条件强开。 */
#define BQ76952_SUBCMD_SLEEP_ENABLE             (0x0099u) /* 允许 SLEEP；低功耗策略未确认前不要默认发送。 */
#define BQ76952_SUBCMD_SLEEP_DISABLE            (0x009Au) /* 禁止 SLEEP；会增加静态功耗。 */
#define BQ76952_SUBCMD_SWAP_COMM_MODE           (0x29BCu) /* 按 Data Memory 指定通信模式切换；误用可能断开通信。 */
#define BQ76952_SUBCMD_SWAP_TO_I2C              (0x29E7u) /* 立即切换到 I2C fast；用于通信恢复需谨慎。 */

/*---------------------------------------------------------------------------
 * Data Subcommands
 *---------------------------------------------------------------------------*/
#define BQ76952_SUBCMD_DEVICE_NUMBER            (0x0001u) /* 读取器件编号；用于确认 BQ76952 型号。 */
#define BQ76952_SUBCMD_FW_VERSION               (0x0002u) /* 读取固件版本；用于兼容性记录。 */
#define BQ76952_SUBCMD_HW_VERSION               (0x0003u) /* 读取硬件版本；用于兼容性记录。 */
#define BQ76952_SUBCMD_MANUFACTURING_STATUS     (0x0057u) /* 读取制造状态；FET_EN 等调试状态在此体现。 */
#define BQ76952_SUBCMD_DASTATUS1                (0x0071u) /* 扩展采样状态 1；返回数据格式需按 TRM 解析。 */
#define BQ76952_SUBCMD_DASTATUS2                (0x0072u) /* 扩展采样状态 2；返回数据格式需按 TRM 解析。 */
#define BQ76952_SUBCMD_DASTATUS3                (0x0073u) /* 扩展采样状态 3；返回数据格式需按 TRM 解析。 */
#define BQ76952_SUBCMD_DASTATUS4                (0x0074u) /* 扩展采样状态 4；返回数据格式需按 TRM 解析。 */
#define BQ76952_SUBCMD_DASTATUS5                (0x0075u) /* 扩展采样状态 5；返回数据格式需按 TRM 解析。 */
#define BQ76952_SUBCMD_DASTATUS6                (0x0076u) /* 扩展采样状态 6；返回数据格式需按 TRM 解析。 */
#define BQ76952_SUBCMD_DASTATUS7                (0x0077u) /* 扩展采样状态 7；返回数据格式需按 TRM 解析。 */
#define BQ76952_SUBCMD_CB_ACTIVE_CELLS          (0x0083u) /* 主机控制均衡 cell mask；bit 与 cell index 对应。 */
#define BQ76952_SUBCMD_CB_SET_LVL               (0x0084u) /* 按阈值设置均衡；策略未定前不要自动调用。 */
#define BQ76952_SUBCMD_CBSTATUS1                (0x0085u) /* 均衡状态 1；用于确认实际均衡通道。 */
#define BQ76952_SUBCMD_CBSTATUS2                (0x0086u) /* 均衡状态 2；用于确认实际均衡通道。 */
#define BQ76952_SUBCMD_CBSTATUS3                (0x0087u) /* 均衡状态 3；用于确认实际均衡通道。 */
#define BQ76952_SUBCMD_FET_CONTROL              (0x0097u) /* 主机控制单个 FET off bit；写 1 表示强制关断。 */
#define BQ76952_SUBCMD_REG12_CONTROL            (0x0098u) /* REG1/REG2 控制；会影响外设供电，需硬件确认。 */

#define BQ76952_FET_CONTROL_PCHG_OFF_MASK       (0x08u)   /* bit3，强制 PCHG 关闭。 */
#define BQ76952_FET_CONTROL_CHG_OFF_MASK        (0x04u)   /* bit2，强制 CHG 关闭。 */
#define BQ76952_FET_CONTROL_PDSG_OFF_MASK       (0x02u)   /* bit1，强制 PDSG 关闭。 */
#define BQ76952_FET_CONTROL_DSG_OFF_MASK        (0x01u)   /* bit0，强制 DSG 关闭。 */

/*---------------------------------------------------------------------------
 * Data Memory 地址
 *---------------------------------------------------------------------------*/
#define BQ76952_DM_POWER_CONFIG                 (0x9234u) /* H2，默认 0x2982，电源/SLEEP 配置；写错可能影响唤醒。 */
#define BQ76952_DM_COMM_TYPE                    (0x9239u) /* U1，默认 0x00，I2C/SPI/HDQ/CRC 配置；误写可能断开通信。 */
#define BQ76952_DM_I2C_ADDRESS                  (0x923Au) /* U1，默认 0x00，I2C 地址配置；改写前必须准备恢复方案。 */
#define BQ76952_DM_DA_CONFIGURATION             (0x9303u) /* H1，默认 0x05，DA/温度配置入口；影响采样解释。 */
#define BQ76952_DM_VCELL_MODE                   (0x9304u) /* H2，默认 0x0000，有效 cell mask；本项目 6S 使用 0x8923。 */
#define BQ76952_DM_PROTECTION_CONFIGURATION     (0x925Fu) /* H2，默认 0x0002，保护总配置；误写可能关闭关键保护。 */
#define BQ76952_DM_ENABLED_PROTECTIONS_A        (0x9261u) /* U1，默认 0x88，A 类保护使能；bit 与 Safety A 对齐。 */
#define BQ76952_DM_ENABLED_PROTECTIONS_B        (0x9262u) /* U1，默认 0x00，温度类保护使能；需确认 TS 接法。 */
#define BQ76952_DM_ENABLED_PROTECTIONS_C        (0x9263u) /* U1，默认 0x00，锁存/看门狗等保护使能。 */
#define BQ76952_DM_CHG_FET_PROTECTIONS_A        (0x9265u) /* U1，默认 0x98，A 类故障关闭 CHG 的掩码。 */
#define BQ76952_DM_CHG_FET_PROTECTIONS_B        (0x9266u) /* U1，默认 0xD5，B 类故障关闭 CHG 的掩码。 */
#define BQ76952_DM_CHG_FET_PROTECTIONS_C        (0x9267u) /* U1，默认 0x56，C 类故障关闭 CHG 的掩码。 */
#define BQ76952_DM_DSG_FET_PROTECTIONS_A        (0x9269u) /* U1，默认 0xE4，A 类故障关闭 DSG 的掩码。 */
#define BQ76952_DM_DSG_FET_PROTECTIONS_B        (0x926Au) /* U1，默认 0xE6，B 类故障关闭 DSG 的掩码。 */
#define BQ76952_DM_DSG_FET_PROTECTIONS_C        (0x926Bu) /* U1，默认 0xE2，C 类故障关闭 DSG 的掩码。 */
#define BQ76952_DM_DEFAULT_ALARM_MASK           (0x926Du) /* H2，默认 0xF800，Alarm Enable 默认值；影响 ALERT 输出。 */
#define BQ76952_DM_FET_OPTIONS                  (0x9308u) /* H1，默认 0x0D，FET 控制策略；误写可能改变功率路径。 */
#define BQ76952_DM_CHG_PUMP_CONTROL             (0x9309u) /* U1，默认 0x01，电荷泵控制；影响高边 FET 驱动。 */
#define BQ76952_DM_BALANCING_CONFIGURATION      (0x9335u) /* H1，默认 0x00，均衡配置；策略未定前不要自动开启。 */
#define BQ76952_DM_CB_MIN_CELL_TEMP             (0x9336u) /* I1，默认 -20 C，均衡温度下限，单位摄氏度。 */
#define BQ76952_DM_CB_MAX_CELL_TEMP             (0x9337u) /* I1，默认 60 C，均衡温度上限，单位摄氏度。 */
#define BQ76952_DM_CB_MAX_INTERNAL_TEMP         (0x9338u) /* I1，默认 70 C，均衡内部温度上限，单位摄氏度。 */
#define BQ76952_DM_CB_INTERVAL                  (0x9339u) /* U1，默认 20 s，均衡保持/重算间隔，单位秒。 */
#define BQ76952_DM_CB_MAX_CELLS                 (0x933Au) /* U1，默认 1，同时自动均衡最大 cell 数。 */
#define BQ76952_DM_CB_MIN_CELL_V_CHARGE         (0x933Bu) /* I2，默认 3900 mV，充电均衡电压门限。 */
#define BQ76952_DM_CB_MIN_DELTA_CHARGE          (0x933Du) /* U1，默认 40 mV，充电均衡开启压差。 */
#define BQ76952_DM_CB_STOP_DELTA_CHARGE         (0x933Eu) /* U1，默认 20 mV，充电均衡停止压差。 */
#define BQ76952_DM_MFG_STATUS_INIT              (0x9343u) /* H2，默认 0x0040，Manufacturing Status 上电初值。 */

/*---------------------------------------------------------------------------
 * Data Memory bit mask
 *---------------------------------------------------------------------------*/
#define BQ76952_VCELL_MODE_CELL1_MASK           (0x0001u) /* bit0，Cell 1 接入并纳入保护。 */
#define BQ76952_VCELL_MODE_CELL2_MASK           (0x0002u) /* bit1，Cell 2 接入并纳入保护。 */
#define BQ76952_VCELL_MODE_CELL3_MASK           (0x0004u) /* bit2，Cell 3 接入并纳入保护。 */
#define BQ76952_VCELL_MODE_CELL4_MASK           (0x0008u) /* bit3，Cell 4 接入并纳入保护。 */
#define BQ76952_VCELL_MODE_CELL5_MASK           (0x0010u) /* bit4，Cell 5 接入并纳入保护。 */
#define BQ76952_VCELL_MODE_CELL6_MASK           (0x0020u) /* bit5，Cell 6 接入并纳入保护。 */
#define BQ76952_VCELL_MODE_CELL7_MASK           (0x0040u) /* bit6，Cell 7 接入并纳入保护。 */
#define BQ76952_VCELL_MODE_CELL8_MASK           (0x0080u) /* bit7，Cell 8 接入并纳入保护。 */
#define BQ76952_VCELL_MODE_CELL9_MASK           (0x0100u) /* bit8，Cell 9 接入并纳入保护。 */
#define BQ76952_VCELL_MODE_CELL10_MASK          (0x0200u) /* bit9，Cell 10 接入并纳入保护。 */
#define BQ76952_VCELL_MODE_CELL11_MASK          (0x0400u) /* bit10，Cell 11 接入并纳入保护。 */
#define BQ76952_VCELL_MODE_CELL12_MASK          (0x0800u) /* bit11，Cell 12 接入并纳入保护。 */
#define BQ76952_VCELL_MODE_CELL13_MASK          (0x1000u) /* bit12，Cell 13 接入并纳入保护。 */
#define BQ76952_VCELL_MODE_CELL14_MASK          (0x2000u) /* bit13，Cell 14 接入并纳入保护。 */
#define BQ76952_VCELL_MODE_CELL15_MASK          (0x4000u) /* bit14，Cell 15 接入并纳入保护。 */
#define BQ76952_VCELL_MODE_CELL16_MASK          (0x8000u) /* bit15，Cell 16 接入并纳入保护。 */

#define BQ76952_ENABLED_PROTECTIONS_A_SCD_MASK  (0x80u)   /* bit7，使能放电短路保护。 */
#define BQ76952_ENABLED_PROTECTIONS_A_OCD2_MASK (0x40u)   /* bit6，使能放电过流二级保护。 */
#define BQ76952_ENABLED_PROTECTIONS_A_OCD1_MASK (0x20u)   /* bit5，使能放电过流一级保护。 */
#define BQ76952_ENABLED_PROTECTIONS_A_OCC_MASK  (0x10u)   /* bit4，使能充电过流保护。 */
#define BQ76952_ENABLED_PROTECTIONS_A_COV_MASK  (0x08u)   /* bit3，使能单体过压保护。 */
#define BQ76952_ENABLED_PROTECTIONS_A_CUV_MASK  (0x04u)   /* bit2，使能单体欠压保护。 */

#define BQ76952_ENABLED_PROTECTIONS_B_OTF_MASK  (0x80u)   /* bit7，使能 FET 过温保护。 */
#define BQ76952_ENABLED_PROTECTIONS_B_OTINT_MASK (0x40u)  /* bit6，使能内部过温保护。 */
#define BQ76952_ENABLED_PROTECTIONS_B_OTD_MASK  (0x20u)   /* bit5，使能放电过温保护。 */
#define BQ76952_ENABLED_PROTECTIONS_B_OTC_MASK  (0x10u)   /* bit4，使能充电过温保护。 */
#define BQ76952_ENABLED_PROTECTIONS_B_UTINT_MASK (0x04u)  /* bit2，使能内部低温保护。 */
#define BQ76952_ENABLED_PROTECTIONS_B_UTD_MASK  (0x02u)   /* bit1，使能放电低温保护。 */
#define BQ76952_ENABLED_PROTECTIONS_B_UTC_MASK  (0x01u)   /* bit0，使能充电低温保护。 */

#define BQ76952_ENABLED_PROTECTIONS_C_OCD3_MASK (0x80u)   /* bit7，使能放电过流三级保护。 */
#define BQ76952_ENABLED_PROTECTIONS_C_SCDL_MASK (0x40u)   /* bit6，使能放电短路锁存。 */
#define BQ76952_ENABLED_PROTECTIONS_C_OCDL_MASK (0x20u)   /* bit5，使能放电过流锁存。 */
#define BQ76952_ENABLED_PROTECTIONS_C_COVL_MASK (0x10u)   /* bit4，使能单体过压锁存。 */
#define BQ76952_ENABLED_PROTECTIONS_C_PTO_MASK  (0x04u)   /* bit2，使能预充超时保护。 */
#define BQ76952_ENABLED_PROTECTIONS_C_HWDF_MASK (0x02u)   /* bit1，使能主机看门狗故障保护。 */

#define BQ76952_FET_OPTIONS_FET_INIT_OFF_MASK   (0x20u)   /* bit5，上电后等待 host 命令再允许 FET 打开。 */
#define BQ76952_FET_OPTIONS_PDSG_EN_MASK        (0x10u)   /* bit4，允许 PDSG 预放电。 */
#define BQ76952_FET_OPTIONS_FET_CTRL_EN_MASK    (0x08u)   /* bit3，允许器件控制 FET。 */
#define BQ76952_FET_OPTIONS_HOST_FET_EN_MASK    (0x04u)   /* bit2，允许 host 命令关闭 FET。 */
#define BQ76952_FET_OPTIONS_SLEEPCHG_MASK       (0x02u)   /* bit1，SLEEP 中是否允许 CHG FET。 */
#define BQ76952_FET_OPTIONS_SFET_MASK           (0x01u)   /* bit0，串联 FET 模式，启用 body diode protection。 */

#define BQ76952_BALANCING_CB_NO_CMD_MASK        (0x10u)   /* bit4，阻止 host 控制均衡命令。 */
#define BQ76952_BALANCING_CB_NOSLEEP_MASK       (0x08u)   /* bit3，均衡时阻止 SLEEP。 */
#define BQ76952_BALANCING_CB_SLEEP_MASK         (0x04u)   /* bit2，允许 SLEEP 中均衡。 */
#define BQ76952_BALANCING_CB_RLX_MASK           (0x02u)   /* bit1，Relax 条件允许自动均衡。 */
#define BQ76952_BALANCING_CB_CHG_MASK           (0x01u)   /* bit0，充电条件允许自动均衡。 */

/*---------------------------------------------------------------------------
 * 6S 辅助 mask
 *---------------------------------------------------------------------------*/
#define BQ76952_CELL_MASK_NONE                  (0x0000u) /* 无 cell mask；用于初始化局部变量。 */
#define BQ76952_CELL_MASK_ALL_16S               (0xFFFFu) /* 16S 全 cell mask；本项目 6S 不能直接使用。 */
#define BQ76952_CELL_MASK_6S_HW_CELL1           BQ76952_VCELL_MODE_CELL1_MASK  /* physical Cell1 -> BQ Cell1 -> 0x14 -> CB bit0. */
#define BQ76952_CELL_MASK_6S_HW_CELL2           BQ76952_VCELL_MODE_CELL2_MASK  /* physical Cell2 -> BQ Cell2 -> 0x16 -> CB bit1. */
#define BQ76952_CELL_MASK_6S_HW_CELL3           BQ76952_VCELL_MODE_CELL6_MASK  /* physical Cell3 -> BQ Cell6 -> 0x1E -> CB bit5. */
#define BQ76952_CELL_MASK_6S_HW_CELL4           BQ76952_VCELL_MODE_CELL9_MASK  /* physical Cell4 -> BQ Cell9 -> 0x24 -> CB bit8. */
#define BQ76952_CELL_MASK_6S_HW_CELL5           BQ76952_VCELL_MODE_CELL12_MASK /* physical Cell5 -> BQ Cell12 -> 0x2A -> CB bit11. */
#define BQ76952_CELL_MASK_6S_HW_CELL6           BQ76952_VCELL_MODE_CELL16_MASK /* physical Cell6 -> BQ Cell16 -> 0x32 -> CB bit15. */
#define BQ76952_CELL_MASK_6S_HW                 (BQ76952_CELL_MASK_6S_HW_CELL1 | \
                                                 BQ76952_CELL_MASK_6S_HW_CELL2 | \
                                                 BQ76952_CELL_MASK_6S_HW_CELL3 | \
                                                 BQ76952_CELL_MASK_6S_HW_CELL4 | \
                                                 BQ76952_CELL_MASK_6S_HW_CELL5 | \
                                                 BQ76952_CELL_MASK_6S_HW_CELL6)
#define BQ76952_CELL_MASK_6S_HW_CONFIRMED       BQ76952_CELL_MASK_6S_HW

#endif /* INT_BQ76952_BSP_H */
