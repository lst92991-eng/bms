#ifndef __INT_BQ769_BSP_H__
#define __INT_BQ769_BSP_H__

#include <stdint.h>

#define TLB_OV_PROTECT 4.00		  // 单体过压保护电压
#define TLB_UV_PROTECT 2.80		  // 单体欠压保护电压(负载payload)
#define TLB_SHUTDOWN_VOLTAGE 2.70 // 自动关机电压(MCU供电)

#define TLB_BALANCE_VOLTAGE 3.0 // 均衡起始电压
#define TLB_BALANCE_DIFF_VOLTAGE 0.05
#define TLB_BALANCE_MIN_TEMPR 0
#define TLB_BALANCE_MAX_TEMPR 40

/*----------------------------------
  系统状态与控制寄存器
----------------------------------*/
#define BQ_SYS_STAT 0x00  // 系统状态寄存器（故障标志）
#define BQ_CELLBAL1 0x01  // 电池平衡控制1（Cell 1-5）
#define BQ_CELLBAL2 0x02  // 电池平衡控制2（Cell 6-10）
#define BQ_SYS_CTRL1 0x04 // 系统控制1（ADC使能/关断）
#define BQ_SYS_CTRL2 0x05 // 系统控制2（负载开关控制）

/*----------------------------------
  保护配置寄存器
----------------------------------*/
#define BQ_PROTECT1 0x06 // 保护使能寄存器（OV/UV/OCD/SCD）
#define BQ_PROTECT2 0x07 // 二级保护阈值
#define BQ_PROTECT3 0x08 // 保护延迟时间配置
#define BQ_OV_TRIP 0x09	 // 过压阈值（16mV/bit）
#define BQ_UV_TRIP 0x0A	 // 欠压阈值（16mV/bit）
#define BQ_CC_CFG 0x0B	 // 库仑计数器配置

/*----------------------------------
  电池电压检测寄存器（10节）
----------------------------------*/
// Cell 1-5
#define BQ_VC1_HI 0x0C // [11:4] bits
#define BQ_VC1_LO 0x0D // [3:0] bits (左对齐)
#define BQ_VC2_HI 0x0E
#define BQ_VC2_LO 0x0F
#define BQ_VC3_HI 0x10
#define BQ_VC3_LO 0x11
#define BQ_VC4_HI 0x12
#define BQ_VC4_LO 0x13
#define BQ_VC5_HI 0x14
#define BQ_VC5_LO 0x15

// Cell 6-10（BQ76930特有）
#define BQ_VC6_HI 0x16
#define BQ_VC6_LO 0x17
#define BQ_VC7_HI 0x18
#define BQ_VC7_LO 0x19
#define BQ_VC8_HI 0x1A
#define BQ_VC8_LO 0x1B
#define BQ_VC9_HI 0x1C
#define BQ_VC9_LO 0x1D
#define BQ_VC10_HI 0x1E
#define BQ_VC10_LO 0x1F

#define BQ_BAT_HI 0x2A // 电池组总电压高字节（14-bit ADC值，2.44mV/bit）?:ml-citation{ref="3,4" data="citationList"}
#define BQ_BAT_LO 0x2B // 电池组总电压低字节（低6位有效）?:ml-citation{ref="3" data="citationList"}

/*----------------------------------
  温度检测寄存器
----------------------------------*/
#define BQ_TS1_HI 0x2C // 温度传感器1高字节
#define BQ_TS1_LO 0x2D // 温度传感器1低字节
#define BQ_TS2_HI 0x2E // 温度传感器2高字节（仅BQ76930/40）
#define BQ_TS2_LO 0x2F // 温度传感器2低字节（仅BQ76930/40）
#define BQ_CC_HI 0x32  // 库仑计数器高字节
#define BQ_CC_LO 0x33  // 库仑计数器低字节

#define BQ_ADCGAIN1 0x50
#define BQ_ADCOFFSET 0x51
#define BQ_ADCGAIN2 0x59

typedef struct _Register_Group
{
	union
	{
		struct
		{
			unsigned char OCD : 1;
			unsigned char SCD : 1;
			unsigned char OV : 1;
			unsigned char UV : 1;
			unsigned char OVRD_ALERT : 1;
			unsigned char DEVICE_XREADY : 1;
			unsigned char WAKE : 1;
			unsigned char CC_READY : 1;
		} StatusBit;
		unsigned char StatusByte;
	} SysStatus;

	union
	{
		struct
		{
			unsigned char CB1 : 1;
			unsigned char CB2 : 1;
			unsigned char CB3 : 1;
			unsigned char CB4 : 1;
			unsigned char CB5 : 1;
			unsigned char RSVD : 3;
		} CellBal1Bit;
		unsigned char CellBal1Byte;
	} CellBal1;

	union
	{
		struct
		{
			unsigned char CB6 : 1;
			unsigned char CB7 : 1;
			unsigned char CB8 : 1;
			unsigned char CB9 : 1;
			unsigned char CB10 : 1;
			unsigned char RSVD : 3;
		} CellBal2Bit;
		unsigned char CellBal2Byte;
	} CellBal2;

	union
	{
		struct
		{
			unsigned char RSVD : 3;
			unsigned char CB15 : 1;
			unsigned char CB14 : 1;
			unsigned char CB13 : 1;
			unsigned char CB12 : 1;
			unsigned char CB11 : 1;
		} CellBal3Bit;
		unsigned char CellBal3Byte;
	} CellBal3;

	union
	{
		struct
		{
			unsigned char SHUT_B : 1;
			unsigned char SHUT_A : 1;
			unsigned char RSVD1 : 1;
			unsigned char TEMP_SEL : 1;
			unsigned char ADC_EN : 1;
			unsigned char RSVD2 : 2;
			unsigned char LOAD_PRESENT : 1;
		} SysCtrl1Bit;
		unsigned char SysCtrl1Byte;
	} SysCtrl1;

	union
	{
		struct
		{
			unsigned char CHG_ON : 1;
			unsigned char DSG_ON : 1;
			unsigned char WAKE_T : 2;
			unsigned char WAKE_EN : 1;
			unsigned char CC_ONESHOT : 1;
			unsigned char CC_EN : 1;
			unsigned char DELAY_DIS : 1;
		} SysCtrl2Bit;
		unsigned char SysCtrl2Byte;
	} SysCtrl2;

	union
	{
		struct
		{
			unsigned char SCD_THRESH : 3;
			unsigned char SCD_DELAY : 2;
			unsigned char RSVD : 2;
			unsigned char RSNS : 1;
		} Protect1Bit;
		unsigned char Protect1Byte;
	} Protect1;

	union
	{
		struct
		{
			unsigned char OCD_THRESH : 4;
			unsigned char OCD_DELAY : 3;
			unsigned char RSVD : 1;
		} Protect2Bit;
		unsigned char Protect2Byte;
	} Protect2;

	union
	{
		struct
		{
			unsigned char RSVD : 4;
			unsigned char OV_DELAY : 2;
			unsigned char UV_DELAY : 2;
		} Protect3Bit;
		unsigned char Protect3Byte;
	} Protect3;

	unsigned char OVTrip;
	unsigned char UVTrip;
	unsigned char CCCfg; // must be 0x19

	union
	{
		struct
		{
			unsigned char VC1_HI;
			unsigned char VC1_LO;
		} VCell1Byte;
		unsigned short VCell1Word;
	} VCell1;

	union
	{
		struct
		{
			unsigned char VC2_HI;
			unsigned char VC2_LO;
		} VCell2Byte;
		unsigned short VCell2Word;
	} VCell2;

	union
	{
		struct
		{
			unsigned char VC3_HI;
			unsigned char VC3_LO;
		} VCell3Byte;
		unsigned short VCell3Word;
	} VCell3;

	union
	{
		struct
		{
			unsigned char VC4_HI;
			unsigned char VC4_LO;
		} VCell4Byte;
		unsigned short VCell4Word;
	} VCell4;

	union
	{
		struct
		{
			unsigned char VC5_HI;
			unsigned char VC5_LO;
		} VCell5Byte;
		unsigned short VCell5Word;
	} VCell5;

	union
	{
		struct
		{
			unsigned char VC6_HI;
			unsigned char VC6_LO;
		} VCell6Byte;
		unsigned short VCell6Word;
	} VCell6;

	union
	{
		struct
		{
			unsigned char VC7_HI;
			unsigned char VC7_LO;
		} VCell7Byte;
		unsigned short VCell7Word;
	} VCell7;

	union
	{
		struct
		{
			unsigned char VC8_HI;
			unsigned char VC8_LO;
		} VCell8Byte;
		unsigned short VCell8Word;
	} VCell8;

	union
	{
		struct
		{
			unsigned char VC9_HI;
			unsigned char VC9_LO;
		} VCell9Byte;
		unsigned short VCell9Word;
	} VCell9;

	union
	{
		struct
		{
			unsigned char VC10_HI;
			unsigned char VC10_LO;
		} VCell10Byte;
		unsigned short VCell10Word;
	} VCell10;

	union
	{
		struct
		{
			unsigned char VC11_HI;
			unsigned char VC11_LO;
		} VCell11Byte;
		unsigned short VCell11Word;
	} VCell11;

	union
	{
		struct
		{
			unsigned char VC12_HI;
			unsigned char VC12_LO;
		} VCell12Byte;
		unsigned short VCell12Word;
	} VCell12;

	union
	{
		struct
		{
			unsigned char VC13_HI;
			unsigned char VC13_LO;
		} VCell13Byte;
		unsigned short VCell13Word;
	} VCell13;

	union
	{
		struct
		{
			unsigned char VC14_HI;
			unsigned char VC14_LO;
		} VCell14Byte;
		unsigned short VCell14Word;
	} VCell14;

	union
	{
		struct
		{
			unsigned char VC15_HI;
			unsigned char VC15_LO;
		} VCell15Byte;
		unsigned short VCell15Word;
	} VCell15;

	union
	{
		struct
		{
			unsigned char BAT_HI;
			unsigned char BAT_LO;
		} VBatByte;
		unsigned short VBatWord;
	} VBat;

	union
	{
		struct
		{
			unsigned char TS1_HI;
			unsigned char TS1_LO;
		} TS1Byte;
		unsigned short TS1Word;
	} TS1;

	union
	{
		struct
		{
			unsigned char TS2_HI;
			unsigned char TS2_LO;
		} TS2Byte;
		unsigned short TS2Word;
	} TS2;

	union
	{
		struct
		{
			unsigned char TS3_HI;
			unsigned char TS3_LO;
		} TS3Byte;
		unsigned short TS3Word;
	} TS3;

	union
	{
		struct
		{
			unsigned char CC_HI;
			unsigned char CC_LO;
		} CCByte;
		unsigned short CCWord;
	} CC;

	union
	{
		struct
		{
			unsigned char RSVD1 : 2;
			unsigned char ADCGAIN_4_3 : 2;
			unsigned char RSVD2 : 4;
		} ADCGain1Bit;
		unsigned char ADCGain1Byte;
	} ADCGain1;

	unsigned char ADCOffset;

	union
	{
		struct
		{
			unsigned char RSVD : 5;
			unsigned char ADCGAIN_2_0 : 3;
		} ADCGain2Bit;
		unsigned char ADCGain2Byte;
	} ADCGain2;

} RegisterGroup;

typedef enum
{
	BMS_SCD_DELAY_50us = 0x00,
	BMS_SCD_DELAY_100us = 0x01,
	BMS_SCD_DELAY_200us = 0x02,
	BMS_SCD_DELAY_400us = 0x03,
} BMS_SCDDelayTypedef;

typedef enum
{
	BMS_OCD_DEALY_10ms = 0x00,
	BMS_OCD_DELAY_20ms = 0x01,
	BMS_OCD_DELAY_40ms = 0x02,
	BMS_OCD_DELAY_80ms = 0x03,
	BMS_OCD_DELAY_160ms = 0x04,
	BMS_OCD_DELAY_320ms = 0x05,
	BMS_OCD_DELAY_640ms = 0x06,
	BMS_OCD_DELAY_1280ms = 0x07,
} BMS_OCDDelayTypedef;

typedef enum
{
	BMS_OV_DELAY_1s = 0x00,
	BMS_OV_DELAY_2s = 0x01,
	BMS_OV_DELAY_4s = 0x02,
	BMS_OV_DELAY_8s = 0x03,
} BMS_OVDelayTypedef;

typedef enum
{
	BMS_UV_DELAY_1s = 0x00,
	BMS_UV_DELAY_4s = 0x01,
	BMS_UV_DELAY_8s = 0x02,
	BMS_UV_DELAY_16s = 0x03,
} BMS_UVDelayTypedef;

typedef enum
{
	BQ_CELL_NONE = 0x0000,
	BQ_CELL_INDEX1 = 0x0001,
	BQ_CELL_INDEX2 = 0x0002,
	BQ_CELL_INDEX3 = 0x0004,
	BQ_CELL_INDEX4 = 0x0008,
	BQ_CELL_INDEX5 = 0x0010,
	BQ_CELL_INDEX6 = 0x0020,
	BQ_CELL_INDEX7 = 0x0040,
	BQ_CELL_INDEX8 = 0x0080,
	BQ_CELL_INDEX9 = 0x0100,
	BQ_CELL_INDEX10 = 0x0200,
	BQ_CELL_INDEX11 = 0x0400,
	BQ_CELL_INDEX12 = 0x0800,
	BQ_CELL_INDEX13 = 0x1000,
	BQ_CELL_INDEX14 = 0x2000,
	BQ_CELL_INDEX15 = 0x4000,
	BQ_CELL_ALL = 0x3FFF,
} BQ_CellIndexTypedef;

#endif
