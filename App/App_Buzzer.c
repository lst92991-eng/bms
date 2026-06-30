#include "App_Buzzer.h"

#include "Int_Buzzer.h"
#include "main.h"

/*
 * 无源蜂鸣器只能输出方波，这里用音符表模拟一小段复古平台跳跃游戏风格旋律。
 * 若后续要换真正的音频文件，需要先离线转成这样的 freq/duration 表。
 */
static const App_BuzzerNoteTypeDef s_power_on_notes[] =
{
    {659u,  90u, 35u},    /* E5 */
    {659u,  90u, 90u},    /* E5 */
    {659u,  90u, 90u},    /* E5 */
    {523u,  90u, 35u},    /* C5 */
    {659u,  90u, 90u},    /* E5 */
    {784u, 140u, 130u},   /* G5 */
    {392u, 140u, 130u},   /* G4 */
    {523u, 120u, 35u},    /* C5 */
    {392u, 120u, 35u},    /* G4 */
    {330u, 120u, 35u},    /* E4 */
    {440u, 110u, 35u},    /* A4 */
    {494u, 100u, 35u},    /* B4 */
    {466u,  80u, 35u},    /* A#4 */
    {440u, 120u, 80u},    /* A4 */
};

void App_Buzzer_Init(void)
{
    Int_Buzzer_Stop();
}

static void App_Buzzer_PlayTable(const App_BuzzerNoteTypeDef *notes, uint16_t count)
{
    if (notes == 0)
    {
        return;
    }

    for (uint16_t i = 0u; i < count; i++)
    {
        if (notes[i].duration_ms == 0u)
        {
            continue;
        }

        if (notes[i].freq_hz == 0u)
        {
            HAL_Delay(notes[i].duration_ms);
        }
        else
        {
            Int_Buzzer_ToneBlocking(notes[i].freq_hz, notes[i].duration_ms);
        }

        if (notes[i].gap_ms != 0u)
        {
            HAL_Delay(notes[i].gap_ms);
        }
    }

    Int_Buzzer_Stop();
}

void App_Buzzer_PlayPowerOn(void)
{
    App_Buzzer_PlayTable(s_power_on_notes,
                         (uint16_t)(sizeof(s_power_on_notes) / sizeof(s_power_on_notes[0])));
}
