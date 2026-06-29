#include "App_OLED.h"

#include "Int_OLED.h"

/**
 * @file App_OLED.c
 * @brief APP 层 OLED 简单状态页。
 *
 * 当前 OLED 只承载 bring-up 必需信息：BQ I2C 是否正常，以及成功读到的
 * BQ Power Config。显示布局集中在本文件，避免把 UI 细节混进 BatMan。
 */

static bool s_oled_ready = false;
static bool s_iic_status_valid = false;
static bool s_last_iic_ok = false;
static bool s_power_config_valid = false;
static uint16_t s_last_power_config = 0u;

/**
 * @brief 将 4-bit 值转换为十六进制字符。
 *
 * 避免为了显示 4 位十六进制值引入 sprintf。
 */
static char App_OLED_HexNibble(uint8_t value)
{
    value &= 0x0Fu;
    if (value < 10u)
    {
        return (char)('0' + value);
    }

    return (char)('A' + (value - 10u));
}

/**
 * @brief 生成 OLED 上的 Power Config 显示行。
 */
static void App_OLED_MakePowerConfigLine(uint16_t power_config, char *line)
{
    line[0] = 'P';
    line[1] = 'W';
    line[2] = 'R';
    line[3] = ':';
    line[4] = App_OLED_HexNibble((uint8_t)(power_config >> 12u));
    line[5] = App_OLED_HexNibble((uint8_t)(power_config >> 8u));
    line[6] = App_OLED_HexNibble((uint8_t)(power_config >> 4u));
    line[7] = App_OLED_HexNibble((uint8_t)power_config);
    line[8] = '\0';
}

/**
 * @brief 重绘完整 OLED 状态页。
 *
 * 页面很小，整屏刷新更容易保证错误、复位和状态变化后的显示一致性。
 */
static void App_OLED_Render(bool ok, bool power_valid, uint16_t power_config)
{
    char power_line[9];

    Inf_OLED_Clear();
    Inf_OLED_ShowString(0u, 0u, (uint8_t *)"BMS24V", 16u, 1u);
    Inf_OLED_ShowString(0u, 16u, (uint8_t *)"BQ IIC", 16u, 1u);
    Inf_OLED_ShowString(0u, 32u, ok ? (uint8_t *)"OK" : (uint8_t *)"FAIL", 16u, 1u);

    if (ok && power_valid)
    {
        App_OLED_MakePowerConfigLine(power_config, power_line);
        Inf_OLED_ShowString(0u, 48u, (uint8_t *)power_line, 16u, 1u);
    }
    else
    {
        Inf_OLED_ShowString(0u, 48u, (uint8_t *)"PWR:----", 16u, 1u);
    }

    Inf_OLED_Refresh();
}

/**
 * @brief 初始化 OLED APP 页面。
 */
void App_OLED_Init(void)
{
    /* BQ 通信确认前默认显示 FAIL，方便上板排查。 */
    Inf_OLED_Init();
    s_oled_ready = true;
    s_iic_status_valid = false;

    App_OLED_ShowIicStatus(false);
}

/**
 * @brief 显示 BQ I2C 是否正常。
 *
 * 连续相同状态不重复刷新，减少 OLED I2C 访问。
 */
void App_OLED_ShowIicStatus(bool ok)
{
    if (!s_oled_ready)
    {
        return;
    }

    if (s_iic_status_valid && (s_last_iic_ok == ok))
    {
        return;
    }

    s_iic_status_valid = true;
    s_last_iic_ok = ok;

    App_OLED_Render(ok, ok && s_power_config_valid, s_last_power_config);
}

/**
 * @brief 显示 BQ I2C 状态和 Power Config。
 *
 * Power Config 只有在 `ok=true` 时才有意义；失败时屏幕显示占位符，
 * 内部保留上一次成功值以便后续恢复显示。
 */
void App_OLED_ShowBqIicPowerConfig(bool ok, uint16_t power_config)
{
    if (!s_oled_ready)
    {
        return;
    }

    if (s_iic_status_valid &&
        (s_last_iic_ok == ok) &&
        (s_power_config_valid == ok) &&
        (!ok || (s_last_power_config == power_config)))
    {
        return;
    }

    s_iic_status_valid = true;
    s_last_iic_ok = ok;
    s_power_config_valid = ok;
    if (ok)
    {
        s_last_power_config = power_config;
    }

    App_OLED_Render(ok, ok, power_config);
}
