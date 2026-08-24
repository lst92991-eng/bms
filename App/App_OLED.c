#include "App_OLED.h"

#include "FreeRTOS.h"

#include "Int_OLED.h"

/**
 * @file App_OLED.c
 * @brief APP 层 OLED 简单状态页。
 *
 * 当前 OLED 显示 BQ I2C 状态、实时 SOC 和 SOH。显示布局和数值格式集中
 * 在本文件，避免把 UI 细节混进 BatMan。
 */

static bool s_oled_ready = false;
static bool s_iic_status_valid = false;
static bool s_last_iic_ok = false;
static bool s_battery_status_valid = false;
static uint8_t s_last_soc_percent = 0u;
static uint8_t s_last_soh_percent = 0u;
static bool s_last_soh_valid = false;
static bool s_render_pending = false;
static uint32_t s_reinit_elapsed_ms = 0u;

enum
{
    APP_OLED_REINIT_PERIOD_MS = 5000u
};

typedef struct
{
    bool iic_ok;
    bool battery_valid;
    uint8_t soc_percent;
    bool soh_valid;
    uint8_t soh_percent;
} App_OLED_PageTypeDef;

static uint32_t App_OLED_Lock(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    return primask;
}

static void App_OLED_Unlock(uint32_t primask)
{
    if (primask == 0u)
    {
        __enable_irq();
    }
}

/**
 * @brief 将浮点百分比限制并四舍五入到 0~100。
 */
static uint8_t App_OLED_RoundPercent(float value)
{
    if (value <= 0.0f)
    {
        return 0u;
    }
    if (value >= 100.0f)
    {
        return 100u;
    }
    return (uint8_t)(value + 0.5f);
}

/**
 * @brief 生成三位百分比显示行，例如 SOC:045。
 */
static void App_OLED_MakePercentLine(const char *name, uint8_t percent, char *line)
{
    if (percent > 100u)
    {
        percent = 100u;
    }

    line[0] = name[0];
    line[1] = name[1];
    line[2] = name[2];
    line[3] = ':';
    line[4] = (char)('0' + (percent / 100u));
    line[5] = (char)('0' + ((percent / 10u) % 10u));
    line[6] = (char)('0' + (percent % 10u));
    line[7] = '\0';
}

/**
 * @brief 重绘完整 OLED 状态页。
 *
 * 页面很小，整屏刷新更容易保证错误、复位和状态变化后的显示一致性。
 */
static bool App_OLED_Render(const App_OLED_PageTypeDef *page)
{
    char soc_line[8];
    char soh_line[8];

    Inf_OLED_Clear();
    Inf_OLED_ShowText16(0u, 0u, "BMS24V");
    Inf_OLED_ShowText16(0u, 16u, page->iic_ok ? "BQ IIC OK" : "BQ IIC FAIL");

    if (page->iic_ok && page->battery_valid)
    {
        App_OLED_MakePercentLine("SOC", page->soc_percent, soc_line);
        Inf_OLED_ShowText16(0u, 32u, soc_line);
        if (page->soh_valid)
        {
            App_OLED_MakePercentLine("SOH", page->soh_percent, soh_line);
            Inf_OLED_ShowText16(0u, 48u, soh_line);
        }
        else
        {
            Inf_OLED_ShowText16(0u, 48u, "SOH:---");
        }
    }
    else
    {
        Inf_OLED_ShowText16(0u, 32u, "SOC:---");
        Inf_OLED_ShowText16(0u, 48u, "SOH:---");
    }

    return Inf_OLED_Refresh();
}

/**
 * @brief 初始化 OLED APP 页面。
 */
void App_OLED_Init(void)
{
    uint32_t primask;

    /* BQ 通信确认前默认显示 FAIL；初始化失败由维护任务低频重试。 */
    s_oled_ready = Inf_OLED_Init();
    primask = App_OLED_Lock();
    s_iic_status_valid = false;
    s_battery_status_valid = false;
    s_last_iic_ok = false;
    s_last_soc_percent = 0u;
    s_last_soh_percent = 0u;
    s_last_soh_valid = false;
    s_render_pending = s_oled_ready;
    s_reinit_elapsed_ms = 0u;
    App_OLED_Unlock(primask);
}

void App_OLED_Task(uint32_t interval_ms)
{
    App_OLED_PageTypeDef page;
    uint32_t primask;
    bool pending;

    if (!s_oled_ready)
    {
        if (s_reinit_elapsed_ms < APP_OLED_REINIT_PERIOD_MS)
        {
            uint32_t elapsed = s_reinit_elapsed_ms + interval_ms;

            s_reinit_elapsed_ms =
                (elapsed > APP_OLED_REINIT_PERIOD_MS) ? APP_OLED_REINIT_PERIOD_MS : elapsed;
        }
        if (s_reinit_elapsed_ms < APP_OLED_REINIT_PERIOD_MS)
        {
            return;
        }

        s_reinit_elapsed_ms = 0u;
        s_oled_ready = Inf_OLED_Init();
        if (!s_oled_ready)
        {
            return;
        }
        s_render_pending = true;
    }

    primask = App_OLED_Lock();
    pending = s_render_pending;
    page.iic_ok = s_iic_status_valid && s_last_iic_ok;
    page.battery_valid = s_battery_status_valid;
    page.soc_percent = s_last_soc_percent;
    page.soh_valid = s_last_soh_valid;
    page.soh_percent = s_last_soh_percent;
    s_render_pending = false;
    App_OLED_Unlock(primask);

    if (pending && !App_OLED_Render(&page))
    {
        /* 失败后保留 dirty，下一维护周期重试；本周期首错即停。 */
        primask = App_OLED_Lock();
        s_render_pending = true;
        App_OLED_Unlock(primask);
    }
}

/**
 * @brief 显示 BQ I2C 是否正常。
 *
 * 连续相同状态不重复刷新，减少 OLED I2C 访问。
 */
void App_OLED_ShowIicStatus(bool ok)
{
    uint32_t primask = App_OLED_Lock();

    if (s_iic_status_valid && (s_last_iic_ok == ok))
    {
        App_OLED_Unlock(primask);
        return;
    }

    s_iic_status_valid = true;
    s_last_iic_ok = ok;
    s_render_pending = true;
    App_OLED_Unlock(primask);
}

/**
 * @brief 保留旧初始化接口；Power Config 不再占用运行状态页。
 */
void App_OLED_ShowBqIicPowerConfig(bool ok, uint16_t power_config)
{
    (void)power_config;
    App_OLED_ShowIicStatus(ok);
}

/**
 * @brief 刷新运行态 SOC/SOH；数值不变时不重复访问 OLED。
 */
void App_OLED_ShowBatteryStatus(bool ok, float soc_percent, bool soh_valid, uint8_t soh_percent)
{
    uint8_t soc_value;
    uint32_t primask;

    soc_value = App_OLED_RoundPercent(soc_percent);
    if (soh_percent > 100u)
    {
        soh_percent = 100u;
    }

    primask = App_OLED_Lock();
    if (s_iic_status_valid && (s_last_iic_ok == ok) && (s_battery_status_valid == ok) &&
        (!ok || ((s_last_soc_percent == soc_value) && (s_last_soh_valid == soh_valid) &&
                 (s_last_soh_percent == soh_percent))))
    {
        App_OLED_Unlock(primask);
        return;
    }

    s_iic_status_valid = true;
    s_last_iic_ok = ok;
    s_battery_status_valid = ok;
    if (ok)
    {
        s_last_soc_percent = soc_value;
        s_last_soh_valid = soh_valid;
        s_last_soh_percent = soh_percent;
    }

    s_render_pending = true;
    App_OLED_Unlock(primask);
}
