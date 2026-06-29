#include "Int_Button.h"

static bool s_button_raw_last = false;
static bool s_button_state = false;
static bool s_button_press_event = false;
static bool s_button_release_event = false;
static uint32_t s_button_change_tick = 0u;
static uint32_t s_button_press_tick = 0u;

static bool Int_Button_ReadLevel(void)
{
    return HAL_GPIO_ReadPin(BMS_MUX_BTN_GPIO_Port, BMS_MUX_BTN_Pin) == INT_BUTTON_ACTIVE_LEVEL;
}

void Int_Button_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();

    gpio.Pin = BMS_MUX_BTN_Pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(BMS_MUX_BTN_GPIO_Port, &gpio);

    s_button_raw_last = Int_Button_ReadLevel();
    s_button_state = s_button_raw_last;
    s_button_press_event = false;
    s_button_release_event = false;
    s_button_change_tick = HAL_GetTick();
    s_button_press_tick = s_button_state ? s_button_change_tick : 0u;
}

void Int_Button_Task(uint32_t now_ms)
{
    bool raw = Int_Button_ReadLevel();

    if (raw != s_button_raw_last)
    {
        s_button_raw_last = raw;
        s_button_change_tick = now_ms;
        return;
    }

    /*
     * 旧项目教学写法：主循环定期调用 Task，稳定超过消抖时间才认作一次状态变化。
     * 这里不放 EXTI，避免按键抖动打断 BQ/SC 的 I2C bring-up。
     */
    if ((raw != s_button_state) &&
        ((uint32_t)(now_ms - s_button_change_tick) >= INT_BUTTON_DEBOUNCE_MS))
    {
        s_button_state = raw;

        if (s_button_state)
        {
            s_button_press_event = true;
            s_button_press_tick = now_ms;
        }
        else
        {
            s_button_release_event = true;
            s_button_press_tick = 0u;
        }
    }
}

Int_Button_StateTypeDef Int_Button_GetState(void)
{
    return s_button_state ? INT_BUTTON_PRESSED : INT_BUTTON_RELEASED;
}

bool Int_Button_IsPressed(void)
{
    return s_button_state;
}

bool Int_Button_IsPressedRaw(void)
{
    return Int_Button_ReadLevel();
}

bool Int_Button_WasPressed(void)
{
    bool ret = s_button_press_event;
    s_button_press_event = false;
    return ret;
}

bool Int_Button_WasReleased(void)
{
    bool ret = s_button_release_event;
    s_button_release_event = false;
    return ret;
}

uint32_t Int_Button_GetHoldMs(uint32_t now_ms)
{
    if (!s_button_state || (s_button_press_tick == 0u))
    {
        return 0u;
    }

    return (uint32_t)(now_ms - s_button_press_tick);
}
