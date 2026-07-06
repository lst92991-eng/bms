#ifndef BMS_BUZZER_SERVICE_H
#define BMS_BUZZER_SERVICE_H

#include <stdint.h>

typedef struct
{
    uint16_t freq_hz;
    uint16_t duration_ms;
    uint16_t gap_ms;
} Bms_BuzzerNoteTypeDef;

void Bms_BuzzerService_Init(void);
void Bms_BuzzerService_Task(uint32_t now_ms);
void Bms_BuzzerService_PlayPowerOn(void);
void Bms_BuzzerService_PlayMario(void);
void Bms_BuzzerService_PlayLowPower(void);

#endif /* BMS_BUZZER_SERVICE_H */
