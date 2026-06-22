#include "App_Display.h"

extern uint32_t bat_mv;
extern float current_a;

extern int8_t tempr;

extern float soc_percent;

/**
 * @brief 初始化
 */
void App_Display_Init(void)
{
    Inf_OLED_Init();
}

static void show_title(void)
{ // 尚硅谷UPS项目
    for (size_t i = 0; i < 5; i++)
    {
        if (i <= 2)
        {
            Inf_OLED_ShowChinese(8 + i * 16, 8, i, 16, 1);
        }
        else
        {
            Inf_OLED_ShowChinese(8 + 24 + i * 16, 8, i, 16, 1);
        }
    }
    Inf_OLED_ShowString(8 + 16 * 3, 8, (uint8_t *)"BMS", 16, 1);
}

static void show_battery_data(void)
{
    // 电压
    char v_str[10] = {0};
    sprintf(v_str, "V:%5.2f", bat_mv / 1000.0);
    Inf_OLED_ShowString(8, 8 + 16, (uint8_t *)v_str, 16, 1);
    // 电流
    char a_str[10] = {0};
    sprintf(a_str, "A:%5.2f", current_a);
    Inf_OLED_ShowString(70, 8 + 16, (uint8_t *)a_str, 16, 1);

    // 温度
    char t_str[10] = {0};
    sprintf(t_str, "T:%d", tempr);
    Inf_OLED_ShowString(8, 8 + 16 * 2, (uint8_t *)t_str, 16, 1);
    // 电量
    char s_str[10] = {0};
    sprintf(s_str, "S:%d%%", (int)(soc_percent + 0.5f));
    Inf_OLED_ShowString(70, 8 + 16 * 2, (uint8_t *)s_str, 16, 1);
}

/**
 * @brief 显示
 */
void App_Display_Show(void)
{
    show_title();

    show_battery_data();
    taskENTER_CRITICAL();
    Inf_OLED_Refresh();
    taskEXIT_CRITICAL();
}
