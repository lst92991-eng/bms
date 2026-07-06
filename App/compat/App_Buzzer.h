#ifndef APP_BUZZER_H
#define APP_BUZZER_H

#include "Bms_BuzzerService.h"

typedef Bms_BuzzerNoteTypeDef App_BuzzerNoteTypeDef;

void App_Buzzer_Init(void);
void App_Buzzer_Task(uint32_t now_ms);
void App_Buzzer_PlayPowerOn(void);
void App_Buzzer_PlayMario(void);
void App_Buzzer_PlayLowPower(void);

#endif /* APP_BUZZER_H */
