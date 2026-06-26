#include "App_OLED.h"

#include "Int_OLED.h"

static bool s_oled_ready = false;
static bool s_iic_status_valid = false;
static bool s_last_iic_ok = false;
static bool s_power_config_valid = false;
static uint16_t s_last_power_config = 0u;

static char App_OLED_HexNibble(uint8_t value)
{
    value &= 0x0Fu;
    if (value < 10u)
    {
        return (char)('0' + value);
    }

    return (char)('A' + (value - 10u));
}

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

void App_OLED_Init(void)
{
    Inf_OLED_Init();
    s_oled_ready = true;
    s_iic_status_valid = false;

    App_OLED_ShowIicStatus(false);
}

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
