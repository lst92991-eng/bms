#ifndef APP_BUZZER_H
#define APP_BUZZER_H

#include <stdint.h>

typedef struct
{
    uint16_t freq_hz;
    uint16_t duration_ms;
    uint16_t gap_ms;
} App_BuzzerNoteTypeDef;

void App_Buzzer_Init(void);
void App_Buzzer_PlayTable(const App_BuzzerNoteTypeDef *notes, uint16_t count);
void App_Buzzer_PlayPowerOn(void);

#endif /* APP_BUZZER_H */
