#ifndef INT_SC8815_BSP_H
#define INT_SC8815_BSP_H

#include <stdint.h>

/*
 * SC8815 BSP 头文件首版。
 * 本文件只保存寄存器、位定义、硬件常量和项目安全约束，不实现通信、GPIO 或业务策略。
 */

/* ==================== I2C 地址 ==================== */

/* SC8815 固定 7-bit I2C 地址。 */
#define SC8815_I2C_ADDR_7BIT                         (0x74u)
/* 8-bit 写地址：仅用于对照手册，驱动层应优先使用 7-bit 地址。 */
#define SC8815_I2C_ADDR_WRITE_8BIT                   (0xE8u)
/* 8-bit 读地址：仅用于对照手册，驱动层应优先使用 7-bit 地址。 */
#define SC8815_I2C_ADDR_READ_8BIT                    (0xE9u)
#define SC8815_I2C_STANDARD_MODE_HZ                  (100000u)
#define SC8815_I2C_FAST_MODE_HZ                      (400000u)
#define SC8815_SW_I2C_DELAY_CYCLES                   (32u)     /* 软件 I2C 半周期粗延时，后续上逻辑分析仪后再按波形校准。 */

/* ==================== 项目硬件常量 ==================== */

/* 本项目 SC8815 只允许用于 6S 三元锂充电方向，禁止 OTG/反向输出/反向供电。 */
#define SC8815_PROJECT_CELL_COUNT_SERIES             (6u)
#define SC8815_PROJECT_CELL_FULL_MV                  (4200u)
#define SC8815_PROJECT_PACK_FULL_MV                  (25200u)
#define SC8815_PROJECT_TARGET_CHARGE_MV              (24730u)

/*
 * 网表按 MCU I2C3 命名为 PA7=SCL、PA6=SDA，但当前硬件已确认硬件 I2C3
 * 线序不可直接使用。软件 IIC 默认先按网表线序访问，ACK 失败时 INT 层会
 * 自动尝试交换线序，便于 bring-up 阶段确认真实接线。
 */
#define SC8815_PROJECT_IIC_LINE_SWAPPED              (0u)
/* 软 I2C 网表连接：PA7=MCU_SCL_NET，PA6=MCU_SDA_NET；这里只记录硬件事实，不提供 GPIO 操作。 */
#define SC8815_PROJECT_SCL_PORT_LETTER               ('A')
#define SC8815_PROJECT_SCL_PIN_NUMBER                (7u)
#define SC8815_PROJECT_SDA_PORT_LETTER               ('A')
#define SC8815_PROJECT_SDA_PIN_NUMBER                (6u)
/* INT=PA5，ISR 只允许置标志，后续任务再读状态。 */
#define SC8815_PROJECT_INT_PORT_LETTER               ('A')
#define SC8815_PROJECT_INT_PIN_NUMBER                (5u)
/* #CE=PB1，低有效；初始化安全态必须先保持高电平 disable。 */
#define SC8815_PROJECT_CE_N_PORT_LETTER              ('B')
#define SC8815_PROJECT_CE_N_PIN_NUMBER               (1u)
#define SC8815_PROJECT_CE_N_ENABLE_LEVEL             (0u)
#define SC8815_PROJECT_CE_N_DISABLE_LEVEL            (1u)
/* PSTOP=PB0，高电平 standby；配置关键寄存器前应保持高电平。 */
#define SC8815_PROJECT_PSTOP_PORT_LETTER             ('B')
#define SC8815_PROJECT_PSTOP_PIN_NUMBER              (0u)
#define SC8815_PROJECT_PSTOP_WORK_LEVEL              (0u)
#define SC8815_PROJECT_PSTOP_STANDBY_LEVEL           (1u)

/* 输入/电池侧采样电阻，单位 mOhm。 */
#define SC8815_PROJECT_RSNS_IBUS_MOHM                (10u)
#define SC8815_PROJECT_RSNS_IBAT_MOHM                (10u)
/* VBATS 外部分压：R17=100k，R18=5.1k，目标约 24.73V。 */
#define SC8815_PROJECT_VBATS_RUP_OHM                 (100000u)
#define SC8815_PROJECT_VBATS_RDOWN_OHM               (5100u)
#define SC8815_PROJECT_VBATS_REF_MV                  (1200u)
#define SC8815_PROJECT_VBATS_TARGET_FROM_DIVIDER_MV  (24729u)

/* 项目要求的电流采样比例。 */
#define SC8815_PROJECT_IBUS_RATIO_X                  (3u)
#define SC8815_PROJECT_IBAT_RATIO_X                  (6u)
#define SC8815_PROJECT_VBUS_MON_RATIO_X10            (125u)
#define SC8815_PROJECT_VBAT_MON_RATIO_X10            (125u)

/* Bring-up 与软件约束电流，单位 mA。限流值禁止为 0，项目最低不低于 300mA。 */
#define SC8815_PROJECT_MIN_LIMIT_CURRENT_MA          (300u)
#define SC8815_PROJECT_BRINGUP_IBUS_LIMIT_MA         (3000u)
#define SC8815_PROJECT_BRINGUP_IBAT_LIMIT_MA         (1000u)
#define SC8815_PROJECT_DEFAULT_IBUS_LIMIT_MA         (1500u)
#define SC8815_PROJECT_DEFAULT_IBAT_LIMIT_MA         (3000u)
#define SC8815_PROJECT_MAX_IBUS_LIMIT_MA             (3000u)
#define SC8815_PROJECT_MAX_IBAT_LIMIT_MA             (5000u)

/* 6S 不能用内部 CSEL/VCELL_SET 生成目标电压；必须 VBAT_SEL=1，使用 VBATS 外部分压。 */
#define SC8815_PROJECT_MUST_USE_EXTERNAL_VBATS       (1u)
#define SC8815_PROJECT_FORBID_INTERNAL_CSEL_VCELL    (1u)

/* ==================== 寄存器地址 0x00-0x1B ==================== */

/* 电池目标电压、外部分压选择、IR 补偿。 */
#define SC8815_REG_VBAT_SET                          (0x00u)
/* OTG 内部 VBUS 参考高 8 bit；本项目禁止作为可用功能配置。 */
#define SC8815_REG_VBUSREF_I_SET                     (0x01u)
/* OTG 内部 VBUS 参考低 2 bit；本项目禁止作为可用功能配置。 */
#define SC8815_REG_VBUSREF_I_SET2                    (0x02u)
/* OTG 外部 VBUS 参考高 8 bit；本项目禁止反向输出相关模式。 */
#define SC8815_REG_VBUSREF_E_SET                     (0x03u)
/* OTG 外部 VBUS 参考低 2 bit；本项目禁止反向输出相关模式。 */
#define SC8815_REG_VBUSREF_E_SET2                    (0x04u)
/* 输入侧电流限流设置。 */
#define SC8815_REG_IBUS_LIM_SET                      (0x05u)
/* 电池侧充电电流限流设置。 */
#define SC8815_REG_IBAT_LIM_SET                      (0x06u)
/* 充电 VINREG 参考设置。 */
#define SC8815_REG_VINREG_SET                        (0x07u)
/* 电流/电压采样比例设置。 */
#define SC8815_REG_RATIO                             (0x08u)
/* OTG、VINREG、频率、死区设置。 */
#define SC8815_REG_CTRL0_SET                         (0x09u)
/* 充电电流选择、涓流、终止、FB、OVP 设置。 */
#define SC8815_REG_CTRL1_SET                         (0x0Au)
/* FACTORY、dither、slew 设置。 */
#define SC8815_REG_CTRL2_SET                         (0x0Bu)
/* PGATE、GPO、ADC、环路、EOC、PFM 设置。 */
#define SC8815_REG_CTRL3_SET                         (0x0Cu)
/* VBUS ADC 高 8 bit。 */
#define SC8815_REG_VBUS_FB_VALUE                     (0x0Du)
/* VBUS ADC 低 2 bit。 */
#define SC8815_REG_VBUS_FB_VALUE2                    (0x0Eu)
/* VBAT ADC 高 8 bit。 */
#define SC8815_REG_VBAT_FB_VALUE                     (0x0Fu)
/* VBAT ADC 低 2 bit。 */
#define SC8815_REG_VBAT_FB_VALUE2                    (0x10u)
/* IBUS ADC 高 8 bit。 */
#define SC8815_REG_IBUS_VALUE                        (0x11u)
/* IBUS ADC 低 2 bit。 */
#define SC8815_REG_IBUS_VALUE2                       (0x12u)
/* IBAT ADC 高 8 bit。 */
#define SC8815_REG_IBAT_VALUE                        (0x13u)
/* IBAT ADC 低 2 bit。 */
#define SC8815_REG_IBAT_VALUE2                       (0x14u)
/* ADIN ADC 高 8 bit。 */
#define SC8815_REG_ADIN_VALUE                        (0x15u)
/* ADIN ADC 低 2 bit。 */
#define SC8815_REG_ADIN_VALUE2                       (0x16u)
/* 状态：AC_OK、INDET、VBUS_SHORT、OTP、EOC。 */
#define SC8815_REG_STATUS                            (0x17u)
/* 保留寄存器，读改写时不得覆盖。 */
#define SC8815_REG_RESERVED_18                       (0x18u)
/* 中断 mask。 */
#define SC8815_REG_MASK                              (0x19u)
/* 保留寄存器，读改写时不得覆盖。 */
#define SC8815_REG_RESERVED_1A                       (0x1Au)
/* 保留寄存器，读改写时不得覆盖。 */
#define SC8815_REG_RESERVED_1B                       (0x1Bu)

/* ==================== POR 默认值 ==================== */

#define SC8815_DEFAULT_VBAT_SET                      (0x01u)
#define SC8815_DEFAULT_VBUSREF_I_SET                 (0x31u)
/* POR 默认值为 11xx xxxx：只确认 bit7:6 为 1，低 6 bit 未抽取为固定值。 */
#define SC8815_DEFAULT_VBUSREF_I_SET2_KNOWN_BITS     (0xC0u)
#define SC8815_DEFAULT_VBUSREF_E_SET                 (0x7Cu)
/* POR 默认值为 11xx xxxx：只确认 bit7:6 为 1，低 6 bit 未抽取为固定值。 */
#define SC8815_DEFAULT_VBUSREF_E_SET2_KNOWN_BITS     (0xC0u)
#define SC8815_DEFAULT_IBUS_LIM_SET                  (0xFFu)
#define SC8815_DEFAULT_IBAT_LIM_SET                  (0xFFu)
#define SC8815_DEFAULT_VINREG_SET                    (0x2Cu)
#define SC8815_DEFAULT_RATIO                         (0x38u)
#define SC8815_DEFAULT_CTRL0_SET                     (0x04u)
#define SC8815_DEFAULT_CTRL1_SET                     (0x01u)
#define SC8815_DEFAULT_CTRL2_SET                     (0x01u)
#define SC8815_DEFAULT_CTRL3_SET                     (0x02u)
#define SC8815_DEFAULT_VBUS_FB_VALUE                 (0x00u)
#define SC8815_DEFAULT_VBUS_FB_VALUE2                (0x00u)
#define SC8815_DEFAULT_VBAT_FB_VALUE                 (0x00u)
#define SC8815_DEFAULT_VBAT_FB_VALUE2                (0x00u)
#define SC8815_DEFAULT_IBUS_VALUE                    (0x00u)
#define SC8815_DEFAULT_IBUS_VALUE2                   (0x00u)
#define SC8815_DEFAULT_IBAT_VALUE                    (0x00u)
#define SC8815_DEFAULT_IBAT_VALUE2                   (0x00u)
#define SC8815_DEFAULT_ADIN_VALUE                    (0x00u)
#define SC8815_DEFAULT_ADIN_VALUE2                   (0x00u)
#define SC8815_DEFAULT_STATUS                        (0x00u)
#define SC8815_DEFAULT_RESERVED_18                   (0x00u)
#define SC8815_DEFAULT_MASK                          (0x80u)
#define SC8815_DEFAULT_RESERVED_1A                   (0x00u)
/* POR 默认值为 xxx0 0000：只确认 bit4:0 为 0，bit7:5 未抽取为固定值。 */
#define SC8815_DEFAULT_RESERVED_1B_KNOWN_BITS        (0x00u)

/* ==================== bit mask / shift / reserved mask ==================== */

/* 0x00 VBAT_SET：IR 补偿电阻，设置时需要 PSTOP 高电平。 */
#define SC8815_VBAT_SET_IRCOMP_MASK                  (0xC0u)
#define SC8815_VBAT_SET_IRCOMP_SHIFT                 (6u)
#define SC8815_VBAT_SET_IRCOMP_0_MOHM                (0u)
#define SC8815_VBAT_SET_IRCOMP_20_MOHM               (1u)
#define SC8815_VBAT_SET_IRCOMP_40_MOHM               (2u)
#define SC8815_VBAT_SET_IRCOMP_80_MOHM               (3u)
/* 0x00 VBAT_SET：0=内部电池电压设置，1=外部 VBATS 分压；本项目必须为 1。 */
#define SC8815_VBAT_SET_VBAT_SEL_MASK                (0x20u)
#define SC8815_VBAT_SET_VBAT_SEL_SHIFT               (5u)
#define SC8815_VBAT_SET_VBAT_SEL_INTERNAL            (0u)
#define SC8815_VBAT_SET_VBAT_SEL_EXTERNAL            (1u)
/* 0x00 VBAT_SET：内部 CSEL 只支持 1S-4S，本项目 6S 禁止依赖该字段。 */
#define SC8815_VBAT_SET_CSEL_MASK                    (0x18u)
#define SC8815_VBAT_SET_CSEL_SHIFT                   (3u)
#define SC8815_VBAT_SET_CSEL_1S                      (0u)
#define SC8815_VBAT_SET_CSEL_2S                      (1u)
#define SC8815_VBAT_SET_CSEL_3S                      (2u)
#define SC8815_VBAT_SET_CSEL_4S                      (3u)
/* 0x00 VBAT_SET：内部每节目标电压 4.1V-4.5V，本项目 6S 禁止依赖该字段。 */
#define SC8815_VBAT_SET_VCELL_SET_MASK               (0x07u)
#define SC8815_VBAT_SET_VCELL_SET_SHIFT              (0u)
#define SC8815_VBAT_SET_VCELL_4100_MV                (0u)
#define SC8815_VBAT_SET_VCELL_4200_MV                (1u)
#define SC8815_VBAT_SET_VCELL_4250_MV                (2u)
#define SC8815_VBAT_SET_VCELL_4300_MV                (3u)
#define SC8815_VBAT_SET_VCELL_4350_MV                (4u)
#define SC8815_VBAT_SET_VCELL_4400_MV                (5u)
#define SC8815_VBAT_SET_VCELL_4450_MV                (6u)
#define SC8815_VBAT_SET_VCELL_4500_MV                (7u)
#define SC8815_VBAT_SET_RESERVED_MASK                (0x00u)

/* 0x01 VBUSREF_I_SET：OTG 内部 VBUS 参考高 8 bit，本项目禁止配置为可用反向输出。 */
#define SC8815_VBUSREF_I_SET_VALUE_MASK              (0xFFu)
#define SC8815_VBUSREF_I_SET_VALUE_SHIFT             (0u)
#define SC8815_VBUSREF_I_SET_RESERVED_MASK           (0x00u)

/* 0x02 VBUSREF_I_SET2：OTG 内部 VBUS 参考低 2 bit 位于 bit7:6；bit5:0 保留。 */
#define SC8815_VBUSREF_I_SET2_LOW2_MASK              (0xC0u)
#define SC8815_VBUSREF_I_SET2_LOW2_SHIFT             (6u)
#define SC8815_VBUSREF_I_SET2_RESERVED_MASK          (0x3Fu)

/* 0x03 VBUSREF_E_SET：OTG 外部 VBUS 参考高 8 bit，本项目禁止配置为可用反向输出。 */
#define SC8815_VBUSREF_E_SET_VALUE_MASK              (0xFFu)
#define SC8815_VBUSREF_E_SET_VALUE_SHIFT             (0u)
#define SC8815_VBUSREF_E_SET_RESERVED_MASK           (0x00u)

/* 0x04 VBUSREF_E_SET2：OTG 外部 VBUS 参考低 2 bit 位于 bit7:6；bit5:0 保留。 */
#define SC8815_VBUSREF_E_SET2_LOW2_MASK              (0xC0u)
#define SC8815_VBUSREF_E_SET2_LOW2_SHIFT             (6u)
#define SC8815_VBUSREF_E_SET2_RESERVED_MASK          (0x3Fu)

/* VBUSREF 编码事实：10-bit reference = (4 * HIGH8 + LOW2 + 1) * 2mV。 */
#define SC8815_VBUSREF_HIGH8_MIN                     (0u)
#define SC8815_VBUSREF_HIGH8_MAX                     (255u)
#define SC8815_VBUSREF_LOW2_MIN                      (0u)
#define SC8815_VBUSREF_LOW2_MAX                      (3u)
#define SC8815_VBUSREF_LOW2_DEFAULT                  (3u)
#define SC8815_VBUSREF_LSB_MV                        (2u)
#define SC8815_VBUSREF_VALUE_OFFSET                  (1u)
#define SC8815_VBUSREF_I_DEFAULT_MV                  (400u)
#define SC8815_VBUSREF_E_DEFAULT_MV                  (1000u)
#define SC8815_VBUSREF_I_DEFAULT_VBUS_MV             (5000u)
#define SC8815_VBUSREF_E_RECOMMEND_MIN_MV            (700u)
#define SC8815_VBUSREF_E_RECOMMEND_MAX_MV            (2048u)
#define SC8815_VBUS_INTERNAL_RECOMMEND_MIN_MV        (3000u)
#define SC8815_VBUS_INTERNAL_RECOMMEND_MAX_MV        (36000u)

/* 0x05 IBUS_LIM_SET：输入侧限流 DAC 值；项目禁止配置为 0A。 */
#define SC8815_IBUS_LIM_SET_VALUE_MASK               (0xFFu)
#define SC8815_IBUS_LIM_SET_VALUE_SHIFT              (0u)
#define SC8815_IBUS_LIM_SET_RESERVED_MASK            (0x00u)

/* 0x06 IBAT_LIM_SET：电池侧充电限流 DAC 值；项目禁止配置为 0A。 */
#define SC8815_IBAT_LIM_SET_VALUE_MASK               (0xFFu)
#define SC8815_IBAT_LIM_SET_VALUE_SHIFT              (0u)
#define SC8815_IBAT_LIM_SET_RESERVED_MASK            (0x00u)

/* 0x07 VINREG_SET：充电输入电压调节参考。 */
#define SC8815_VINREG_SET_VALUE_MASK                 (0xFFu)
#define SC8815_VINREG_SET_VALUE_SHIFT                (0u)
#define SC8815_VINREG_SET_RESERVED_MASK              (0x00u)

/* 0x08 RATIO：bit7:5 为保留/内部使用，读改写必须保留，bit5 默认 1。 */
#define SC8815_RATIO_RESERVED_MASK                   (0xE0u)
/* 0x08 RATIO：IBAT_RATIO，0=6x，1=12x；本项目必须 0，即 6x。 */
#define SC8815_RATIO_IBAT_RATIO_MASK                 (0x10u)
#define SC8815_RATIO_IBAT_RATIO_SHIFT                (4u)
#define SC8815_RATIO_IBAT_RATIO_6X                   (0u)
#define SC8815_RATIO_IBAT_RATIO_12X                  (1u)
/* 0x08 RATIO：IBUS_RATIO，10=3x，01=6x，00/11 不允许；本项目必须 10。 */
#define SC8815_RATIO_IBUS_RATIO_MASK                 (0x0Cu)
#define SC8815_RATIO_IBUS_RATIO_SHIFT                (2u)
#define SC8815_RATIO_IBUS_RATIO_NOT_ALLOWED_0        (0u)
#define SC8815_RATIO_IBUS_RATIO_6X                   (1u)
#define SC8815_RATIO_IBUS_RATIO_3X                   (2u)
#define SC8815_RATIO_IBUS_RATIO_NOT_ALLOWED_3        (3u)
/* 0x08 RATIO：VBAT 监测比例，0=12.5x，1=5x；本项目草案保留 12.5x。 */
#define SC8815_RATIO_VBAT_MON_RATIO_MASK             (0x02u)
#define SC8815_RATIO_VBAT_MON_RATIO_SHIFT            (1u)
#define SC8815_RATIO_VBAT_MON_RATIO_12P5X            (0u)
#define SC8815_RATIO_VBAT_MON_RATIO_5X               (1u)
/* 0x08 RATIO：VBUS 监测比例，0=12.5x，1=5x；本项目草案保留 12.5x。 */
#define SC8815_RATIO_VBUS_RATIO_MASK                 (0x01u)
#define SC8815_RATIO_VBUS_RATIO_SHIFT                (0u)
#define SC8815_RATIO_VBUS_RATIO_12P5X                (0u)
#define SC8815_RATIO_VBUS_RATIO_5X                   (1u)

/* 0x09 CTRL0_SET：EN_OTG，0=充电，1=放电/OTG；本项目禁止置 1。 */
#define SC8815_CTRL0_SET_EN_OTG_MASK                 (0x80u)
#define SC8815_CTRL0_SET_EN_OTG_SHIFT                (7u)
#define SC8815_CTRL0_SET_EN_OTG_CHARGE               (0u)
#define SC8815_CTRL0_SET_EN_OTG_DISCHARGE            (1u)
/* 0x09 CTRL0_SET：bit6:5 保留/内部使用，读改写必须保留。 */
#define SC8815_CTRL0_SET_RESERVED_MASK               (0x60u)
/* 0x09 CTRL0_SET：VINREG 比例，0=100x，1=40x。 */
#define SC8815_CTRL0_SET_VINREG_RATIO_MASK           (0x10u)
#define SC8815_CTRL0_SET_VINREG_RATIO_SHIFT          (4u)
#define SC8815_CTRL0_SET_VINREG_RATIO_100X           (0u)
#define SC8815_CTRL0_SET_VINREG_RATIO_40X            (1u)
/* 0x09 CTRL0_SET：开关频率选择。 */
#define SC8815_CTRL0_SET_FREQ_SET_MASK               (0x0Cu)
#define SC8815_CTRL0_SET_FREQ_SET_SHIFT              (2u)
#define SC8815_CTRL0_SET_FREQ_150KHZ                 (0u)
#define SC8815_CTRL0_SET_FREQ_300KHZ_A               (1u)
#define SC8815_CTRL0_SET_FREQ_300KHZ_B               (2u)
#define SC8815_CTRL0_SET_FREQ_450KHZ                 (3u)
/* 0x09 CTRL0_SET：死区时间选择。 */
#define SC8815_CTRL0_SET_DT_SET_MASK                 (0x03u)
#define SC8815_CTRL0_SET_DT_SET_SHIFT                (0u)
#define SC8815_CTRL0_SET_DT_20NS                     (0u)
#define SC8815_CTRL0_SET_DT_40NS                     (1u)
#define SC8815_CTRL0_SET_DT_60NS                     (2u)
#define SC8815_CTRL0_SET_DT_80NS                     (3u)

/* 0x0A CTRL1_SET：ICHAR_SEL，选择以 IBUS 或 IBAT 作为充电电流基准。 */
#define SC8815_CTRL1_SET_ICHAR_SEL_MASK              (0x80u)
#define SC8815_CTRL1_SET_ICHAR_SEL_SHIFT             (7u)
#define SC8815_CTRL1_SET_ICHAR_SEL_IBUS              (0u)
#define SC8815_CTRL1_SET_ICHAR_SEL_IBAT              (1u)
/* 0x0A CTRL1_SET：DIS_TRICKLE，禁止涓流；本项目危险配置，禁止置 1。 */
#define SC8815_CTRL1_SET_DIS_TRICKLE_MASK            (0x40u)
#define SC8815_CTRL1_SET_DIS_TRICKLE_SHIFT           (6u)
#define SC8815_CTRL1_SET_TRICKLE_ENABLE              (0u)
#define SC8815_CTRL1_SET_TRICKLE_DISABLE             (1u)
/* 0x0A CTRL1_SET：DIS_TERM，禁止自动终止；本项目危险配置，禁止置 1。 */
#define SC8815_CTRL1_SET_DIS_TERM_MASK               (0x20u)
#define SC8815_CTRL1_SET_DIS_TERM_SHIFT              (5u)
#define SC8815_CTRL1_SET_AUTO_TERM_ENABLE            (0u)
#define SC8815_CTRL1_SET_AUTO_TERM_DISABLE           (1u)
/* 0x0A CTRL1_SET：FB_SEL 仅放电/反向输出模式使用；本项目禁止使用相关模式。 */
#define SC8815_CTRL1_SET_FB_SEL_MASK                 (0x10u)
#define SC8815_CTRL1_SET_FB_SEL_SHIFT                (4u)
#define SC8815_CTRL1_SET_FB_SEL_INTERNAL_VBUS        (0u)
#define SC8815_CTRL1_SET_FB_SEL_EXTERNAL_FB          (1u)
/* 0x0A CTRL1_SET：涓流阈值选择。 */
#define SC8815_CTRL1_SET_TRICKLE_SET_MASK            (0x08u)
#define SC8815_CTRL1_SET_TRICKLE_SET_SHIFT           (3u)
#define SC8815_CTRL1_SET_TRICKLE_THRESHOLD_70PCT     (0u)
#define SC8815_CTRL1_SET_TRICKLE_THRESHOLD_60PCT     (1u)
/* 0x0A CTRL1_SET：DIS_OVP，禁用放电模式 OVP；本项目危险配置，禁止置 1。 */
#define SC8815_CTRL1_SET_DIS_OVP_MASK                (0x04u)
#define SC8815_CTRL1_SET_DIS_OVP_SHIFT               (2u)
#define SC8815_CTRL1_SET_OVP_ENABLE                  (0u)
#define SC8815_CTRL1_SET_OVP_DISABLE                 (1u)
/* 0x0A CTRL1_SET：bit1:0 保留/内部使用，其中 bit0 默认应保持 1。 */
#define SC8815_CTRL1_SET_RESERVED_MASK               (0x03u)

/* 0x0B CTRL2_SET：bit7:4 保留/内部使用，读改写必须保留。 */
#define SC8815_CTRL2_SET_RESERVED_MASK               (0xF0u)
/* 0x0B CTRL2_SET：FACTORY 位，手册要求 MCU 上电后写 1。 */
#define SC8815_CTRL2_SET_FACTORY_MASK                (0x08u)
#define SC8815_CTRL2_SET_FACTORY_SHIFT               (3u)
#define SC8815_CTRL2_SET_FACTORY_AFTER_POWER_UP      (1u)
/* 0x0B CTRL2_SET：EN_DITHER，启用频率抖动；启用后 PGATE 不能作为 PMOS gate 控制。 */
#define SC8815_CTRL2_SET_EN_DITHER_MASK              (0x04u)
#define SC8815_CTRL2_SET_EN_DITHER_SHIFT             (2u)
#define SC8815_CTRL2_SET_DITHER_DISABLE              (0u)
#define SC8815_CTRL2_SET_DITHER_ENABLE               (1u)
/* 0x0B CTRL2_SET：SLEW_SET，VBUS 动态变化斜率，仅放电/反向输出相关。 */
#define SC8815_CTRL2_SET_SLEW_SET_MASK               (0x03u)
#define SC8815_CTRL2_SET_SLEW_SET_SHIFT              (0u)
#define SC8815_CTRL2_SET_SLEW_1MV_PER_US             (0u)
#define SC8815_CTRL2_SET_SLEW_2MV_PER_US             (1u)
#define SC8815_CTRL2_SET_SLEW_4MV_PER_US             (2u)
#define SC8815_CTRL2_SET_SLEW_8MV_PER_US             (3u)

/* 0x0C CTRL3_SET：EN_PGATE，PGATE 控制。 */
#define SC8815_CTRL3_SET_EN_PGATE_MASK               (0x80u)
#define SC8815_CTRL3_SET_EN_PGATE_SHIFT              (7u)
#define SC8815_CTRL3_SET_PGATE_PMOS_OFF              (0u)
#define SC8815_CTRL3_SET_PGATE_PMOS_ON               (1u)
/* 0x0C CTRL3_SET：GPO_CTRL，GPO 控制。 */
#define SC8815_CTRL3_SET_GPO_CTRL_MASK               (0x40u)
#define SC8815_CTRL3_SET_GPO_CTRL_SHIFT              (6u)
#define SC8815_CTRL3_SET_GPO_HIGH_Z                  (0u)
#define SC8815_CTRL3_SET_GPO_LOW                     (1u)
/* 0x0C CTRL3_SET：AD_START，启动 ADC。 */
#define SC8815_CTRL3_SET_AD_START_MASK               (0x20u)
#define SC8815_CTRL3_SET_AD_START_SHIFT              (5u)
#define SC8815_CTRL3_SET_ADC_STOP                    (0u)
#define SC8815_CTRL3_SET_ADC_START                   (1u)
/* 0x0C CTRL3_SET：ILIM_BW_SEL，电流限制环路带宽。 */
#define SC8815_CTRL3_SET_ILIM_BW_SEL_MASK            (0x10u)
#define SC8815_CTRL3_SET_ILIM_BW_SEL_SHIFT           (4u)
#define SC8815_CTRL3_SET_ILIM_BW_5KHZ                (0u)
#define SC8815_CTRL3_SET_ILIM_BW_1P25KHZ             (1u)
/* 0x0C CTRL3_SET：LOOP_SET，环路响应设置。 */
#define SC8815_CTRL3_SET_LOOP_SET_MASK               (0x08u)
#define SC8815_CTRL3_SET_LOOP_SET_SHIFT              (3u)
#define SC8815_CTRL3_SET_LOOP_NORMAL                 (0u)
#define SC8815_CTRL3_SET_LOOP_IMPROVED               (1u)
/* 0x0C CTRL3_SET：DIS_ShortFoldBack，禁用 VBUS 短路折返；本项目危险配置，禁止置 1。 */
#define SC8815_CTRL3_SET_DIS_SHORT_FOLDBACK_MASK     (0x04u)
#define SC8815_CTRL3_SET_DIS_SHORT_FOLDBACK_SHIFT    (2u)
#define SC8815_CTRL3_SET_SHORT_FOLDBACK_ENABLE       (0u)
#define SC8815_CTRL3_SET_SHORT_FOLDBACK_DISABLE      (1u)
/* 0x0C CTRL3_SET：EOC_SET，EOC 电流阈值选择。 */
#define SC8815_CTRL3_SET_EOC_SET_MASK                (0x02u)
#define SC8815_CTRL3_SET_EOC_SET_SHIFT               (1u)
#define SC8815_CTRL3_SET_EOC_1_OF_25                 (0u)
#define SC8815_CTRL3_SET_EOC_1_OF_10                 (1u)
/* 0x0C CTRL3_SET：EN_PFM，放电模式 PFM；本项目危险配置，禁止置 1。 */
#define SC8815_CTRL3_SET_EN_PFM_MASK                 (0x01u)
#define SC8815_CTRL3_SET_EN_PFM_SHIFT                (0u)
#define SC8815_CTRL3_SET_PFM_DISABLE                 (0u)
#define SC8815_CTRL3_SET_PFM_ENABLE                  (1u)
#define SC8815_CTRL3_SET_RESERVED_MASK               (0x00u)

/* ADC 高 8 bit 寄存器：与对应低 2 bit 组成 10 bit value。 */
#define SC8815_ADC_HIGH8_MASK                        (0xFFu)
#define SC8815_ADC_HIGH8_SHIFT                       (0u)
/* ADC 低 2 bit 寄存器：有效位位于 bit7:6，bit5:0 保留。 */
#define SC8815_ADC_LOW2_MASK                         (0xC0u)
#define SC8815_ADC_LOW2_SHIFT                        (6u)
#define SC8815_ADC_LOW2_RESERVED_MASK                (0x3Fu)

/* 0x17 STATUS：AC_OK 状态。 */
#define SC8815_STATUS_AC_OK_MASK                     (0x40u)
#define SC8815_STATUS_AC_OK_SHIFT                    (6u)
/* 0x17 STATUS：INDET 输入检测状态。 */
#define SC8815_STATUS_INDET_MASK                     (0x20u)
#define SC8815_STATUS_INDET_SHIFT                    (5u)
/* 0x17 STATUS：VBUS_SHORT 短路状态。 */
#define SC8815_STATUS_VBUS_SHORT_MASK                (0x08u)
#define SC8815_STATUS_VBUS_SHORT_SHIFT               (3u)
/* 0x17 STATUS：OTP 过温状态。 */
#define SC8815_STATUS_OTP_MASK                       (0x04u)
#define SC8815_STATUS_OTP_SHIFT                      (2u)
/* 0x17 STATUS：EOC 充电结束状态。 */
#define SC8815_STATUS_EOC_MASK                       (0x02u)
#define SC8815_STATUS_EOC_SHIFT                      (1u)
/* 0x17 STATUS：bit7、bit4、bit0 保留。 */
#define SC8815_STATUS_RESERVED_MASK                  (0x91u)

/* 0x18 保留寄存器：整字节保留。 */
#define SC8815_RESERVED_18_RESERVED_MASK             (0xFFu)

/* 0x19 MASK：bit7、bit4、bit0 为保留/内部位，读改写必须保留。 */
#define SC8815_MASK_RESERVED_MASK                    (0x91u)
/* 0x19 MASK：bit0 虽为内部位，但手册注明 MCU 上电后应写 1，后续 guard 必须单独处理。 */
#define SC8815_MASK_POWER_UP_INTERNAL_SET_MASK       (0x01u)
/* 0x19 MASK：AC_OK interrupt mask，1 表示禁用中断。 */
#define SC8815_MASK_AC_OK_MASK                       (0x40u)
#define SC8815_MASK_AC_OK_SHIFT                      (6u)
/* 0x19 MASK：INDET interrupt mask，1 表示禁用中断。 */
#define SC8815_MASK_INDET_MASK                       (0x20u)
#define SC8815_MASK_INDET_SHIFT                      (5u)
/* 0x19 MASK：VBUS_SHORT interrupt mask，1 表示禁用中断。 */
#define SC8815_MASK_VBUS_SHORT_MASK                  (0x08u)
#define SC8815_MASK_VBUS_SHORT_SHIFT                 (3u)
/* 0x19 MASK：OTP interrupt mask，1 表示禁用中断。 */
#define SC8815_MASK_OTP_MASK                         (0x04u)
#define SC8815_MASK_OTP_SHIFT                        (2u)
/* 0x19 MASK：EOC interrupt mask，1 表示禁用中断。 */
#define SC8815_MASK_EOC_MASK                         (0x02u)
#define SC8815_MASK_EOC_SHIFT                        (1u)

/* 0x1A 保留寄存器：整字节保留。 */
#define SC8815_RESERVED_1A_RESERVED_MASK             (0xFFu)
/* 0x1B 保留寄存器：bit7:5 未固定，bit4:0 默认为 0，读改写必须保留。 */
#define SC8815_RESERVED_1B_RESERVED_MASK             (0xFFu)
#define SC8815_RESERVED_1B_KNOWN_ZERO_MASK           (0x1Fu)

/* ==================== ADC / 电流 / 电压公式常量 ==================== */

/* 10 bit ADC 组合：value10 = 4 * HIGH + LOW + 1。 */
#define SC8815_ADC_HIGH_MULTIPLIER                   (4u)
#define SC8815_ADC_VALUE_OFFSET                      (1u)
#define SC8815_ADC_LSB_MV                            (2u)
#define SC8815_ADC_VALUE_BITS                        (10u)
#define SC8815_ADC_VALUE_MAX                         (1023u)

/* 电压公式：VBUS = value10 * VBUS_RATIO * 2mV；VBAT 同理使用 VBAT_MON_RATIO。 */
#define SC8815_ADC_VBUS_RATIO_DEFAULT_X10            (125u)
#define SC8815_ADC_VBAT_RATIO_DEFAULT_X10            (125u)
#define SC8815_ADC_ADIN_RATIO_X10                    (10u)
#define SC8815_ADC_ADIN_MAX_MV                       (2048u)

/* 电流公式：I(A)=value10 * 2 / 1200 * RATIO * 10mOhm / RSENSE。 */
#define SC8815_ADC_CURRENT_NUMERATOR                 (2u)
#define SC8815_ADC_CURRENT_DENOMINATOR               (1200u)
#define SC8815_ADC_CURRENT_REF_RSENSE_MOHM           (10u)
#define SC8815_ADC_IBUS_RATIO_PROJECT_X              (3u)
#define SC8815_ADC_IBAT_RATIO_PROJECT_X              (6u)

/* 限流 DAC 公式：I(A)=(SET+1)/256*RATIO*10mOhm/RSENSE；手册禁止低于 0.3A。 */
#define SC8815_CURRENT_LIMIT_CODE_MIN                (0u)
#define SC8815_CURRENT_LIMIT_CODE_MAX                (255u)
#define SC8815_CURRENT_LIMIT_CODE_OFFSET             (1u)
#define SC8815_CURRENT_LIMIT_CODE_DENOMINATOR        (256u)
#define SC8815_CURRENT_LIMIT_REF_RSENSE_MOHM         (10u)

/* VINREG 公式：VINREG=(VINREG_SET+1)*VINREG_RATIO(mV)。 */
#define SC8815_VINREG_CODE_MIN                       (0u)
#define SC8815_VINREG_CODE_MAX                       (255u)
#define SC8815_VINREG_CODE_OFFSET                    (1u)
#define SC8815_VINREG_RATIO_100X_MV                  (100u)
#define SC8815_VINREG_RATIO_40X_MV                   (40u)
#define SC8815_VINREG_RATIO_100X_MAX_MV              (25600u)
#define SC8815_VINREG_RATIO_40X_MAX_MV               (10240u)
#define SC8815_VINREG_RATIO_100X_DEFAULT_MV          (4500u)
#define SC8815_VINREG_RATIO_40X_DEFAULT_MV           (1800u)

/* 外部分压目标：VBAT = 1.2V * (1 + RUP / RDOWN)。 */
#define SC8815_VBATS_EXTERNAL_REF_MV                 (1200u)
#define SC8815_VBATS_EXTERNAL_RUP_OHM                (100000u)
#define SC8815_VBATS_EXTERNAL_RDOWN_OHM              (5100u)

/* ==================== 项目推荐草案配置 ==================== */

/* 0x00：VBAT_SEL=1，6S 目标电压由 VBATS 100k/5.1k 外部分压决定。 */
#define SC8815_PROJECT_VBAT_SET_VALUE                (SC8815_VBAT_SET_VBAT_SEL_EXTERNAL << \
                                                      SC8815_VBAT_SET_VBAT_SEL_SHIFT)

/* 0x08：保留 bit5=1，IBAT_RATIO=6x，IBUS_RATIO=3x，VBUS/VBAT 监测比例保持 12.5x。 */
#define SC8815_PROJECT_RATIO_VALUE                   ((SC8815_DEFAULT_RATIO & \
                                                      SC8815_RATIO_RESERVED_MASK) | \
                                                     (SC8815_RATIO_IBAT_RATIO_6X << \
                                                      SC8815_RATIO_IBAT_RATIO_SHIFT) | \
                                                     (SC8815_RATIO_IBUS_RATIO_3X << \
                                                      SC8815_RATIO_IBUS_RATIO_SHIFT) | \
                                                     (SC8815_RATIO_VBAT_MON_RATIO_12P5X << \
                                                      SC8815_RATIO_VBAT_MON_RATIO_SHIFT) | \
                                                     (SC8815_RATIO_VBUS_RATIO_12P5X << \
                                                      SC8815_RATIO_VBUS_RATIO_SHIFT))

/* 0x09：EN_OTG=0，保持充电方向；其它位以读改写方式结合默认值处理。 */
#define SC8815_PROJECT_CTRL0_EN_OTG_VALUE            (SC8815_CTRL0_SET_EN_OTG_CHARGE << \
                                                      SC8815_CTRL0_SET_EN_OTG_SHIFT)
#define SC8815_PROJECT_CTRL0_SAFE_CLEAR_MASK         (SC8815_CTRL0_SET_EN_OTG_MASK)

/* 0x0A：充电电流以 IBAT 为基准；保留 bit0=1；禁止危险位。 */
#define SC8815_PROJECT_CTRL1_SAFE_SET_MASK           (SC8815_CTRL1_SET_ICHAR_SEL_MASK)
#define SC8815_PROJECT_CTRL1_SAFE_CLEAR_MASK         (SC8815_CTRL1_SET_DIS_TRICKLE_MASK | \
                                                      SC8815_CTRL1_SET_DIS_TERM_MASK | \
                                                      SC8815_CTRL1_SET_FB_SEL_MASK | \
                                                      SC8815_CTRL1_SET_DIS_OVP_MASK)

/* 0x0B：手册要求 MCU 上电后 FACTORY 写 1；其它位读改写保留。 */
#define SC8815_PROJECT_CTRL2_SAFE_SET_MASK           (SC8815_CTRL2_SET_FACTORY_MASK)

/* 0x0C：禁止 DIS_ShortFoldBack 和 EN_PFM 置 1。 */
#define SC8815_PROJECT_CTRL3_SAFE_CLEAR_MASK         (SC8815_CTRL3_SET_DIS_SHORT_FOLDBACK_MASK | \
                                                      SC8815_CTRL3_SET_EN_PFM_MASK)

/* 0x19：保留内部 bit7/4，且按手册要求上电后置 bit0。 */
#define SC8815_PROJECT_MASK_SAFE_SET_MASK            (SC8815_MASK_POWER_UP_INTERNAL_SET_MASK)

/* ==================== 项目禁止/危险 bit 汇总 ==================== */

/* EN_OTG=1 会进入放电/OTG，电流从 VBAT 流向 VBUS，本项目禁止。 */
#define SC8815_PROJECT_FORBID_EN_OTG_MASK            (SC8815_CTRL0_SET_EN_OTG_MASK)
/* FB_SEL 属于反向输出相关模式，本项目禁止使用内部/外部 FB 反向输出配置。 */
#define SC8815_PROJECT_FORBID_FB_SEL_MASK            (SC8815_CTRL1_SET_FB_SEL_MASK)
/* DIS_OVP 会关闭放电模式 OVP，属于危险配置，本项目禁止。 */
#define SC8815_PROJECT_FORBID_DIS_OVP_MASK           (SC8815_CTRL1_SET_DIS_OVP_MASK)
/* DIS_ShortFoldBack 会禁用 VBUS 短路折返，属于危险配置，本项目禁止。 */
#define SC8815_PROJECT_FORBID_DIS_SHORT_FOLDBACK_MASK \
                                                        (SC8815_CTRL3_SET_DIS_SHORT_FOLDBACK_MASK)
/* EN_PFM 属于放电模式 PFM，关联 OTG/反向输出，本项目禁止。 */
#define SC8815_PROJECT_FORBID_EN_PFM_MASK            (SC8815_CTRL3_SET_EN_PFM_MASK)
/* DIS_TRICKLE 会禁用涓流保护策略，属于危险配置，本项目禁止。 */
#define SC8815_PROJECT_FORBID_DIS_TRICKLE_MASK       (SC8815_CTRL1_SET_DIS_TRICKLE_MASK)
/* DIS_TERM 会禁用自动终止，属于危险配置，本项目禁止。 */
#define SC8815_PROJECT_FORBID_DIS_TERM_MASK          (SC8815_CTRL1_SET_DIS_TERM_MASK)

#define SC8815_PROJECT_FORBID_CTRL0_SET_MASK         (SC8815_PROJECT_FORBID_EN_OTG_MASK)
#define SC8815_PROJECT_FORBID_CTRL1_SET_MASK         (SC8815_PROJECT_FORBID_FB_SEL_MASK | \
                                                      SC8815_PROJECT_FORBID_DIS_OVP_MASK | \
                                                      SC8815_PROJECT_FORBID_DIS_TRICKLE_MASK | \
                                                      SC8815_PROJECT_FORBID_DIS_TERM_MASK)
#define SC8815_PROJECT_FORBID_CTRL3_SET_MASK         (SC8815_PROJECT_FORBID_DIS_SHORT_FOLDBACK_MASK | \
                                                      SC8815_PROJECT_FORBID_EN_PFM_MASK)

#endif /* INT_SC8815_BSP_H */
