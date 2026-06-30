#include "App_Buzzer.h"

#include "Int_Buzzer.h"
#include "main.h"

#define APP_BUZZER_MARIO_DELAY_MS  5000u

/*
 * 无源蜂鸣器只能输出方波，这里用音符表模拟短提示音。
 * 若后续要换真正的音频文件，需要先离线转成这样的 freq/duration 表。
 */
static const App_BuzzerNoteTypeDef s_power_on_notes[] =
{
    {659u, 100u, 35u},    /* E5 */
    {587u, 100u, 35u},    /* D5 */
    {370u, 180u, 40u},    /* F#4 */
    {415u, 180u, 40u},    /* G#4 */
    {554u, 100u, 35u},    /* C#5 */
    {494u, 100u, 35u},    /* B4 */
    {294u, 180u, 40u},    /* D4 */
    {330u, 180u, 40u},    /* E4 */
    {494u, 100u, 35u},    /* B4 */
    {440u, 100u, 35u},    /* A4 */
    {277u, 180u, 40u},    /* C#4 */
    {330u, 180u, 40u},    /* E4 */
    {440u, 260u, 60u},    /* A4 */
};

static const App_BuzzerNoteTypeDef s_mario_notes[] =
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

static const App_BuzzerNoteTypeDef s_low_power_notes[] =
{
    {784u, 140u, 35u},    /* G5 */
    {659u, 140u, 35u},    /* E5 */
    {523u, 160u, 45u},    /* C5 */
    {392u, 180u, 60u},    /* G4 */
    {262u, 260u, 80u},    /* C4 */
};

static const App_BuzzerNoteTypeDef *s_notes = 0;
static uint16_t s_note_count = 0u;
static uint16_t s_note_index = 0u;
static uint32_t s_next_note_tick = 0u;
static uint32_t s_mario_tick = 0u;
static bool s_playing = false;
static bool s_mario_played = false;

static void App_Buzzer_StartTable(const App_BuzzerNoteTypeDef *notes,
                                  uint16_t count)
{
    s_notes = notes;
    s_note_count = count;
    s_note_index = 0u;
    s_next_note_tick = 0u;
    s_playing = (notes != 0) && (count != 0u);
    Int_Buzzer_Stop();
}

void App_Buzzer_Init(void)
{
    s_mario_tick = HAL_GetTick() + APP_BUZZER_MARIO_DELAY_MS;
    s_mario_played = false;
    App_Buzzer_PlayPowerOn();
}

static void App_Buzzer_PlayNext(uint32_t now_ms)
{
    const App_BuzzerNoteTypeDef *note;

    if (!s_playing || (s_notes == 0))
    {
        return;
    }

    if (s_note_index >= s_note_count)
    {
        s_playing = false;
        Int_Buzzer_Stop();
        return;
    }

    note = &s_notes[s_note_index];
    s_note_index++;

    if ((note->freq_hz == 0u) || (note->duration_ms == 0u))
    {
        Int_Buzzer_Stop();
    }
    else
    {
        Int_Buzzer_Start(note->freq_hz, note->duration_ms);
    }

    s_next_note_tick = now_ms + note->duration_ms + note->gap_ms;
}

void App_Buzzer_Task(uint32_t now_ms)
{
    Int_Buzzer_Task(now_ms);

    if (!s_mario_played &&
        !s_playing &&
        ((int32_t)(now_ms - s_mario_tick) >= 0))
    {
        App_Buzzer_PlayMario();
    }

    if (!s_playing)
    {
        return;
    }

    if ((s_next_note_tick == 0u) ||
        ((int32_t)(now_ms - s_next_note_tick) >= 0))
    {
        App_Buzzer_PlayNext(now_ms);
    }
}

void App_Buzzer_PlayPowerOn(void)
{
    App_Buzzer_StartTable(s_power_on_notes,
                          (uint16_t)(sizeof(s_power_on_notes) / sizeof(s_power_on_notes[0])));
}

void App_Buzzer_PlayMario(void)
{
    s_mario_played = true;
    App_Buzzer_StartTable(s_mario_notes,
                          (uint16_t)(sizeof(s_mario_notes) / sizeof(s_mario_notes[0])));
}

void App_Buzzer_PlayLowPower(void)
{
    s_mario_played = true;
    App_Buzzer_StartTable(s_low_power_notes,
                          (uint16_t)(sizeof(s_low_power_notes) / sizeof(s_low_power_notes[0])));
}
