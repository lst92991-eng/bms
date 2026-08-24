#ifndef INT_BUZZER_H
#define INT_BUZZER_H

#include <stdint.h>

void Int_Buzzer_Init(void);
void Int_Buzzer_Start(uint16_t freq_hz, uint16_t duration_ms);
void Int_Buzzer_Stop(void);
void Int_Buzzer_Task(uint32_t now_ms);

#endif /* INT_BUZZER_H */
