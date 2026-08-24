#include "Int_Watchdog.h"

#include "stm32g0xx.h"

enum
{
    INT_WATCHDOG_LSI_NOMINAL_HZ = 32000u,
    INT_WATCHDOG_PRESCALER_DIV = 256u,
    INT_WATCHDOG_PRESCALER_BITS = 6u,
    INT_WATCHDOG_RELOAD_MAX = 0x0FFFu,
    INT_WATCHDOG_UPDATE_WAIT_LIMIT = 100000u
};

static bool s_watchdog_started = false;
static uint32_t s_watchdog_timeout_ms = 0u;

bool Int_Watchdog_Start(uint32_t timeout_ms)
{
    uint64_t numerator;
    uint32_t reload_ticks;
    uint32_t wait_count;

    if (s_watchdog_started)
    {
        return true;
    }
    if (timeout_ms == 0u)
    {
        return false;
    }

    numerator = ((uint64_t)timeout_ms * INT_WATCHDOG_LSI_NOMINAL_HZ) +
                ((uint64_t)INT_WATCHDOG_PRESCALER_DIV * 1000u) - 1u;
    reload_ticks = (uint32_t)(numerator / ((uint64_t)INT_WATCHDOG_PRESCALER_DIV * 1000u));
    if ((reload_ticks == 0u) || (reload_ticks > (INT_WATCHDOG_RELOAD_MAX + 1u)))
    {
        return false;
    }

    /* 0x5555 解锁 PR/RLR；PR=6 对应 256 分频。 */
    IWDG->KR = 0x5555u;
    IWDG->PR = INT_WATCHDOG_PRESCALER_BITS;
    IWDG->RLR = reload_ticks - 1u;

    wait_count = INT_WATCHDOG_UPDATE_WAIT_LIMIT;
    while (((IWDG->SR & (IWDG_SR_PVU | IWDG_SR_RVU)) != 0u) && (wait_count > 0u))
    {
        wait_count--;
    }
    if (wait_count == 0u)
    {
        return false;
    }

    IWDG->KR = 0xCCCCu;
    IWDG->KR = 0xAAAAu;
    s_watchdog_timeout_ms = timeout_ms;
    s_watchdog_started = true;
    return true;
}

void Int_Watchdog_Refresh(void)
{
    if (s_watchdog_started)
    {
        IWDG->KR = 0xAAAAu;
    }
}

bool Int_Watchdog_IsStarted(void)
{
    return s_watchdog_started;
}

uint32_t Int_Watchdog_GetConfiguredTimeoutMs(void)
{
    return s_watchdog_timeout_ms;
}
