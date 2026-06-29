#include "Int_Buzzer.h"

#include "tim.h"

static bool s_buzzer_on = false;
static bool s_buzzer_beeping = false;
static uint32_t s_buzzer_stop_tick = 0u;
static uint16_t s_buzzer_freq_hz = INT_BUZZER_DEFAULT_FREQ_HZ;

static void Int_Buzzer_SetPwm(uint16_t freq_hz, bool enabled)
{
    uint32_t period;

    if ((freq_hz == 0u) || !enabled)
    {
        __HAL_TIM_SET_COMPARE(&htim3, INT_BUZZER_PWM_CHANNEL, 0u);
        (void)HAL_TIM_PWM_Stop(&htim3, INT_BUZZER_PWM_CHANNEL);
        s_buzzer_on = false;
        return;
    }

    /*
     * CubeMX 将 TIM3 预分频到 1 MHz：
     * PWM 频率 = 1 MHz / (ARR + 1)，占空比固定 50% 适合无源蜂鸣器。
     */
    period = INT_BUZZER_TIMER_HZ / freq_hz;
    if (period == 0u)
    {
        period = 1u;
    }
    period--;

    __HAL_TIM_SET_AUTORELOAD(&htim3, period);
    __HAL_TIM_SET_COMPARE(&htim3, INT_BUZZER_PWM_CHANNEL, (period + 1u) / 2u);
    __HAL_TIM_SET_COUNTER(&htim3, 0u);
    (void)HAL_TIM_PWM_Start(&htim3, INT_BUZZER_PWM_CHANNEL);

    s_buzzer_freq_hz = freq_hz;
    s_buzzer_on = true;
}

void Int_Buzzer_Init(void)
{
    s_buzzer_beeping = false;
    Int_Buzzer_SetPwm(INT_BUZZER_DEFAULT_FREQ_HZ, false);
}

void Int_Buzzer_Set(bool enabled)
{
    s_buzzer_beeping = false;
    Int_Buzzer_SetPwm(s_buzzer_freq_hz, enabled);
}

void Int_Buzzer_Toggle(void)
{
    Int_Buzzer_Set(!s_buzzer_on);
}

bool Int_Buzzer_IsOn(void)
{
    return s_buzzer_on;
}

void Int_Buzzer_Start(uint16_t freq_hz, uint16_t duration_ms)
{
    if ((freq_hz == 0u) || (duration_ms == 0u))
    {
        Int_Buzzer_Stop();
        return;
    }

    s_buzzer_stop_tick = HAL_GetTick() + duration_ms;
    s_buzzer_beeping = true;
    Int_Buzzer_SetPwm(freq_hz, true);
}

void Int_Buzzer_Beep(uint16_t duration_ms)
{
    Int_Buzzer_Start(INT_BUZZER_DEFAULT_FREQ_HZ, duration_ms);
}

void Int_Buzzer_Stop(void)
{
    s_buzzer_beeping = false;
    Int_Buzzer_SetPwm(s_buzzer_freq_hz, false);
}

void Int_Buzzer_Task(uint32_t now_ms)
{
    if (!s_buzzer_beeping)
    {
        return;
    }

    if ((int32_t)(now_ms - s_buzzer_stop_tick) >= 0)
    {
        Int_Buzzer_Stop();
    }
}

void Int_Buzzer_ToneBlocking(uint16_t freq_hz, uint16_t duration_ms)
{
    if ((freq_hz == 0u) || (duration_ms == 0u))
    {
        Int_Buzzer_Stop();
        return;
    }

    Int_Buzzer_SetPwm(freq_hz, true);
    HAL_Delay(duration_ms);
    Int_Buzzer_Stop();
}
