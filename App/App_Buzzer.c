#include "App_Buzzer.h"

#include <stdbool.h>

#include "Int_Buzzer.h"
#include "main.h"

#ifndef BMS_ENGINEERING_BUILD
#define BMS_ENGINEERING_BUILD 0
#endif

#if (BMS_ENGINEERING_BUILD != 0)

typedef struct
{
    uint16_t freq_hz;
    uint16_t duration_ms;
    uint16_t gap_ms;
} App_BuzzerNoteTypeDef;

enum
{
    APP_BUZZER_LANHUA_AUTO_DELAY_MS = 5000u,
    APP_BUZZER_NOTE_A4 = 440u,
    APP_BUZZER_NOTE_B4 = 494u,
    APP_BUZZER_NOTE_C5 = 523u,
    APP_BUZZER_NOTE_D5 = 587u,
    APP_BUZZER_NOTE_E5 = 659u,
    APP_BUZZER_NOTE_FS5 = 740u,
    APP_BUZZER_NOTE_G5 = 784u
};

/* 2/4 拍、约 96 BPM；声音时长与间隔之和按 20,000 ms 精确编排。 */
#define APP_BUZZER_EIGHTH(freq) {(freq), 285u, 27u}
#define APP_BUZZER_DOTTED_EIGHTH(freq) {(freq), 441u, 27u}
#define APP_BUZZER_SIXTEENTH(freq) {(freq), 129u, 27u}
#define APP_BUZZER_QUARTER(freq) {(freq), 590u, 34u}
#define APP_BUZZER_HALF(freq) {(freq), 1190u, 58u}
#define APP_BUZZER_FINAL_HALF(freq) {(freq), 1222u, 58u}
#define APP_BUZZER_EIGHTH_REST {0u, 312u, 0u}

/* 《兰花草》前 16 小节主旋律：每行 4 小节，共 20 秒。 */
static const App_BuzzerNoteTypeDef s_lanhua_notes[] = {
    APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_A4),    APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_E5),
    APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_E5),    APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_E5),
    APP_BUZZER_QUARTER(APP_BUZZER_NOTE_E5),   APP_BUZZER_EIGHTH_REST,
    APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_D5),    APP_BUZZER_DOTTED_EIGHTH(APP_BUZZER_NOTE_C5),
    APP_BUZZER_SIXTEENTH(APP_BUZZER_NOTE_D5), APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_C5),
    APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_B4),    APP_BUZZER_HALF(APP_BUZZER_NOTE_A4),

    APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_A4),    APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_A4),
    APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_A4),    APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_A4),
    APP_BUZZER_QUARTER(APP_BUZZER_NOTE_A4),   APP_BUZZER_EIGHTH_REST,
    APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_G5),    APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_E5),
    APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_G5),    APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_G5),
    APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_FS5),   APP_BUZZER_HALF(APP_BUZZER_NOTE_E5),

    APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_E5),    APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_A4),
    APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_A4),    APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_G5),
    APP_BUZZER_QUARTER(APP_BUZZER_NOTE_E5),   APP_BUZZER_EIGHTH_REST,
    APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_D5),    APP_BUZZER_DOTTED_EIGHTH(APP_BUZZER_NOTE_C5),
    APP_BUZZER_SIXTEENTH(APP_BUZZER_NOTE_D5), APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_C5),
    APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_B4),    APP_BUZZER_QUARTER(APP_BUZZER_NOTE_A4),
    APP_BUZZER_QUARTER(APP_BUZZER_NOTE_E5),

    APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_E5),    APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_C5),
    APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_C5),    APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_B4),
    APP_BUZZER_QUARTER(APP_BUZZER_NOTE_A4),   APP_BUZZER_EIGHTH_REST,
    APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_E5),    APP_BUZZER_DOTTED_EIGHTH(APP_BUZZER_NOTE_D5),
    APP_BUZZER_SIXTEENTH(APP_BUZZER_NOTE_C5), APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_B4),
    APP_BUZZER_EIGHTH(APP_BUZZER_NOTE_G5),    APP_BUZZER_FINAL_HALF(APP_BUZZER_NOTE_A4)};

static const App_BuzzerNoteTypeDef *s_notes;
static uint16_t s_note_count;
static uint16_t s_note_index;
static uint32_t s_next_note_tick;
static uint32_t s_auto_play_tick;
static bool s_playing;
static volatile uint32_t s_lanhua_request_id;
static uint32_t s_lanhua_handled_id;
static bool s_auto_played;

static void App_Buzzer_StartLanhua(uint32_t now_ms)
{
    Int_Buzzer_Stop();
    s_notes = s_lanhua_notes;
    s_note_count = (uint16_t)(sizeof(s_lanhua_notes) / sizeof(s_lanhua_notes[0]));
    s_note_index = 0u;
    s_next_note_tick = now_ms;
    s_playing = true;
}

static void App_Buzzer_PlayNext(uint32_t now_ms)
{
    const App_BuzzerNoteTypeDef *note;

    if (!s_playing || (s_notes == NULL))
    {
        return;
    }

    if (s_note_index >= s_note_count)
    {
        s_playing = false;
        Int_Buzzer_Stop();
        return;
    }

    note = &s_notes[s_note_index++];
    if ((note->freq_hz == 0u) || (note->duration_ms == 0u))
    {
        Int_Buzzer_Stop();
    }
    else
    {
        Int_Buzzer_Start(note->freq_hz, note->duration_ms);
    }

    /* 以计划时刻累加，避免任务调度误差在 49 个音符之间持续积累。 */
    s_next_note_tick += (uint32_t)note->duration_ms + note->gap_ms;
    (void)now_ms;
}

#endif /* BMS_ENGINEERING_BUILD */

void App_Buzzer_Init(void)
{
#if (BMS_ENGINEERING_BUILD != 0)
    s_notes = NULL;
    s_note_count = 0u;
    s_note_index = 0u;
    s_next_note_tick = 0u;
    s_auto_play_tick = HAL_GetTick() + APP_BUZZER_LANHUA_AUTO_DELAY_MS;
    s_playing = false;
    s_lanhua_request_id = 0u;
    s_lanhua_handled_id = 0u;
    s_auto_played = false;
#endif
}

void App_Buzzer_Task(uint32_t now_ms)
{
#if (BMS_ENGINEERING_BUILD != 0)
    uint32_t request_id;
#endif

    Int_Buzzer_Task(now_ms);
#if (BMS_ENGINEERING_BUILD != 0)
    request_id = s_lanhua_request_id;

    if (request_id != s_lanhua_handled_id)
    {
        s_lanhua_handled_id = request_id;
        s_auto_played = true;
        App_Buzzer_StartLanhua(now_ms);
    }
    else if (!s_auto_played && ((int32_t)(now_ms - s_auto_play_tick) >= 0))
    {
        s_auto_played = true;
        App_Buzzer_StartLanhua(now_ms);
    }

    if (s_playing && ((int32_t)(now_ms - s_next_note_tick) >= 0))
    {
        App_Buzzer_PlayNext(now_ms);
    }
#endif
}

void App_Buzzer_PlayLanhua(void)
{
#if (BMS_ENGINEERING_BUILD != 0)
    /* 由蜂鸣器任务统一修改曲目状态，CLI 这里只投递无阻塞请求。 */
    s_lanhua_request_id++;
#endif
}
