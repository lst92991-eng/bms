#include "Bms_App.h"

#include "Bms_Model.h"

void Bms_App_Init(void)
{
    Bms_Model_Init(Bms_Model_GetMutableContext());
}

void Bms_App_Task(uint32_t interval_ms)
{
    (void)interval_ms;
}
