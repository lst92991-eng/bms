#ifndef APP_BUZZER_H
#define APP_BUZZER_H

#include <stdint.h>

#define APP_BUZZER_LANHUA_DURATION_MS 20000u

void App_Buzzer_Init(void);
void App_Buzzer_Task(uint32_t now_ms);
void App_Buzzer_PlayLanhua(void);

#endif /* APP_BUZZER_H */
