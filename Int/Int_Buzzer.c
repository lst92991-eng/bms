#include "Int_Buzzer.h"

#include <stdbool.h>

#include "tim.h"

enum
{
    INT_BUZZER_DEFAULT_FREQ_HZ = 2048u,
    INT_BUZZER_TIMER_HZ = 1000000u,
    INT_BUZZER_MIN_FREQ_HZ = 16u,
    INT_BUZZER_MAX_FREQ_HZ = 20000u
};

#define INT_BUZZER_PWM_CHANNEL TIM_CHANNEL_2

static bool s_buzzer_beeping;
static uint32_t s_buzzer_stop_tick;

static void Int_Buzzer_SetPwm(uint16_t freq_hz, bool enabled)
{
    uint32_t period;

    if (!enabled || (freq_hz < INT_BUZZER_MIN_FREQ_HZ) || (freq_hz > INT_BUZZER_MAX_FREQ_HZ))
    {
        __HAL_TIM_SET_COMPARE(&htim3, INT_BUZZER_PWM_CHANNEL, 0u);
        (void)HAL_TIM_PWM_Stop(&htim3, INT_BUZZER_PWM_CHANNEL);
        return;
    }

    /*
     * TIM3 已由 CubeMX 预分频到 1 MHz。动态更新 ARR 改变音高，
     * CCR 保持 50% 占空比，适合 PB5/Q10 驱动的无源蜂鸣器。
     */
    period = INT_BUZZER_TIMER_HZ / freq_hz;
    __HAL_TIM_SET_AUTORELOAD(&htim3, period - 1u);
    __HAL_TIM_SET_COMPARE(&htim3, INT_BUZZER_PWM_CHANNEL, period / 2u);
    __HAL_TIM_SET_COUNTER(&htim3, 0u);
    (void)HAL_TIM_PWM_Start(&htim3, INT_BUZZER_PWM_CHANNEL);
}

void Int_Buzzer_Init(void)
{
    s_buzzer_beeping = false;
    s_buzzer_stop_tick = 0u;
    Int_Buzzer_SetPwm(INT_BUZZER_DEFAULT_FREQ_HZ, false);
}

void Int_Buzzer_Start(uint16_t freq_hz, uint16_t duration_ms)
{
    if ((duration_ms == 0u) || (freq_hz < INT_BUZZER_MIN_FREQ_HZ) ||
        (freq_hz > INT_BUZZER_MAX_FREQ_HZ))
    {
        Int_Buzzer_Stop();
        return;
    }

    s_buzzer_stop_tick = HAL_GetTick() + duration_ms;
    s_buzzer_beeping = true;
    /* 临时关闭蜂鸣器输出，需要恢复时取消下行注释。 */
    /* Int_Buzzer_SetPwm(freq_hz, true); */
}

void Int_Buzzer_Stop(void)
{
    s_buzzer_beeping = false;
    Int_Buzzer_SetPwm(INT_BUZZER_DEFAULT_FREQ_HZ, false);
}

void Int_Buzzer_Task(uint32_t now_ms)
{
    if (s_buzzer_beeping && ((int32_t)(now_ms - s_buzzer_stop_tick) >= 0))
    {
        Int_Buzzer_Stop();
    }
}
