#ifndef INT_BUZZER_H
#define INT_BUZZER_H

#include <stdbool.h>
#include <stdint.h>

#include "main.h"

/* PB5 使用 TIM3_CH2 输出 PWM，板上 Q10 驱动无源蜂鸣器。 */
#define INT_BUZZER_DEFAULT_FREQ_HZ 2048u
#define INT_BUZZER_TIMER_HZ        1000000u
#define INT_BUZZER_PWM_CHANNEL     TIM_CHANNEL_2

void Int_Buzzer_Init(void);
void Int_Buzzer_Set(bool enabled);
void Int_Buzzer_Toggle(void);
bool Int_Buzzer_IsOn(void);
void Int_Buzzer_Start(uint16_t freq_hz, uint16_t duration_ms);
void Int_Buzzer_Beep(uint16_t duration_ms);
void Int_Buzzer_Stop(void);
void Int_Buzzer_Task(uint32_t now_ms);
void Int_Buzzer_ToneBlocking(uint16_t freq_hz, uint16_t duration_ms);

#endif /* INT_BUZZER_H */
