#include "App_UpsMan.h"
#include "gpio.h"
#include "Int_Can.h"
#include "App_Batman.h"

extern uint16_t cell_mv[UPS_CELL_NUM];

/**
 * @brief  当发现外部24v电源断掉则发出警告 1 蜂鸣器 2 can网关
 */
void App_UpsMan_24vPowerCheck(void)
{
    uint8_t power_24v_status = HAL_GPIO_ReadPin(POWER_24V_GPIO_Port, POWER_24V_Pin);
    if (power_24v_status == GPIO_PIN_RESET)
    {
        vTaskDelay(500);
        if (power_24v_status == GPIO_PIN_RESET) // 二次校验
        {
            printf(" Warning: Primary power source lost. Switching to backup power. \r\n");

            // 蜂鸣器
            HAL_GPIO_WritePin(BUZZER_SIG_GPIO_Port, BUZZER_SIG_Pin, GPIO_PIN_SET);
            vTaskDelay(500);
            HAL_GPIO_WritePin(BUZZER_SIG_GPIO_Port, BUZZER_SIG_Pin, GPIO_PIN_RESET);

            // can 发送
            uint8_t warn_data[1] = {1};
            Int_Can_Send(can_id_mcu_poweroff, warn_data, 1);
        }
    }
}

/**
 * @brief 当电池电压过低关闭芯片的电池供电
 */
void App_UpsMan_McuPowerOffOnUV(void)
{
    for (size_t i = 0; i < UPS_CELL_NUM; i++)
    {
        if (cell_mv[i] < TLB_SHUTDOWN_VOLTAGE)
        {
            printf(" Warning: System will shut down due to low battery voltage.cell_mv[%d]:%d \r\n", i, cell_mv[i]);
            HAL_GPIO_WritePin(STM32_POWER_EN_GPIO_Port, STM32_POWER_EN_Pin, GPIO_PIN_RESET);
            return;
        }
    }
}
