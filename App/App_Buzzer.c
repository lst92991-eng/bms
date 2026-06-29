#include "App_Buzzer.h"

#include "Int_Buzzer.h"
#include "main.h"

/*
 * 方案 A 的“音频文件”不是直接播放 WAV/MP3，而是先转换成音符表：
 * freq_hz 为蜂鸣器方波频率，duration_ms 为发声时间，gap_ms 为两个音之间的停顿。
 * freq_hz 填 0 表示休止符，只延时不发声。
 */
static const App_BuzzerNoteTypeDef s_power_on_notes[] =
{
    {1047u,  90u, 20u},   /* C6 */
    {1319u,  90u, 20u},   /* E6 */
    {1568u, 120u, 30u},   /* G6 */
    {2093u, 180u,  0u},   /* C7 */
};

void App_Buzzer_Init(void)
{
    Int_Buzzer_Stop();
}

void App_Buzzer_PlayTable(const App_BuzzerNoteTypeDef *notes, uint16_t count)
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
