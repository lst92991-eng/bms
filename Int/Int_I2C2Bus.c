#include "Int_I2C2Bus.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

static StaticSemaphore_t s_i2c2_mutex_buffer;
static SemaphoreHandle_t s_i2c2_mutex = NULL;
static uint8_t s_pre_scheduler_lock_depth = 0u;

bool Int_I2C2Bus_Init(void)
{
    if (s_i2c2_mutex == NULL)
    {
        s_i2c2_mutex = xSemaphoreCreateRecursiveMutexStatic(&s_i2c2_mutex_buffer);
    }
    return s_i2c2_mutex != NULL;
}

bool Int_I2C2Bus_Lock(uint32_t timeout_ms)
{
    TickType_t timeout_ticks;

    if ((s_i2c2_mutex == NULL) || (__get_IPSR() != 0u))
    {
        return false;
    }
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
    {
        if (s_pre_scheduler_lock_depth == UINT8_MAX)
        {
            return false;
        }
        s_pre_scheduler_lock_depth++;
        return true;
    }

    timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    if ((timeout_ms != 0u) && (timeout_ticks == 0u))
    {
        timeout_ticks = 1u;
    }
    return xSemaphoreTakeRecursive(s_i2c2_mutex, timeout_ticks) == pdTRUE;
}

void Int_I2C2Bus_Unlock(void)
{
    if ((s_i2c2_mutex == NULL) || (__get_IPSR() != 0u))
    {
        return;
    }
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
    {
        if (s_pre_scheduler_lock_depth > 0u)
        {
            s_pre_scheduler_lock_depth--;
        }
        return;
    }
    (void)xSemaphoreGiveRecursive(s_i2c2_mutex);
}
