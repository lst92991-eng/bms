#ifndef __APP_BATMAN_H_
#define __APP_BATMAN_H_
#include "Int_BQ769.h"
#include "Com_Bq769.h"
#include "math.h"

#define BQ769_CELL_REG_NUM 10
#define UPS_CELL_NUM 6

/**
 * @brief 电池管理的初始化
 *
 1   bq769 重置 唤醒
2   从寄存器中取出 gain值 和 offset值
3   参数设置   ：  启用各种功能  充放电引脚  读数功能
4   阈值设置   ：  电池（欠压  过压) 、  放电过程 电路的 过流 和短路

 */
void App_Batman_Init(void);

/**
 * @brief 获取各个电池电压
 *
 *
 */
void App_Batman_loadCellsVoltage(void);
/**
 * @brief 获取整个电池电压
 *
 *
 */
void App_Batman_loadBatVoltage(void);

/**
 * @brief 充放电电路的电流值
 */
void App_Batman_loadCurrent(void);

/**
 * @brief 获取热敏温度
 */
void App_Batman_loadTemperature(void);

/**
 * @brief 获取电池包电量百分比
 */
void App_Batman_loadBatSocPercent(void);

/**
 * @brief 电池均衡
 */
void App_Batman_CellBalance(void);

/**
 * @brief 设定充电状态
 */
void App_Batman_SetChargeState(uint8_t charge_state);

/**
 * @brief 设定放电状态
 */
void App_Batman_SetDischargeState(uint8_t discharge_state);

/**
 * @brief 库伦计算
 */
void App_Batman_CalcCoulomb(uint16_t delta_ms);

/**
 * @brief 电量计算
 */
void App_Batman_CalcSoc(uint16_t interval_ms);

#endif // __APP_BATMAN_H_
