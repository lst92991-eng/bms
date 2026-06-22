#ifndef __APP_UPSMAN_H_
#define __APP_UPSMAN_H_

/**
 * @brief  当发现外部24v电源断掉则发出警告 1 蜂鸣器 2 can网关
 */
void App_UpsMan_24vPowerCheck(void);

/**
 * @brief 当电池电压过低关闭芯片的电池供电
 */
void App_UpsMan_McuPowerOffOnUV(void);

#endif // __APP_UPSMAN_H_
