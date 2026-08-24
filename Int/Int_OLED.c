#include "Int_OLED.h"

#include <stdbool.h>
#include <string.h>

#include "Int_I2C2Bus.h"
#include "Int_font.h"
#include "i2c.h"

enum
{
    OLED_WIDTH = 128u,
    OLED_HEIGHT = 64u,
    OLED_PAGE_COUNT = 8u,
    OLED_I2C_ADDRESS = 0x78u,
    OLED_I2C_TIMEOUT_MS = 20u,
    OLED_I2C_BUS_LOCK_TIMEOUT_MS = 20u,
    OLED_POWER_ON_DELAY_MS = 100u,
    OLED_FONT_WIDTH = 8u,
    OLED_FONT_BYTES = 16u,
    OLED_PAGE_PACKET_SIZE = OLED_WIDTH + 7u
};

static uint8_t s_oled_gram[OLED_PAGE_COUNT][OLED_WIDTH];

static bool Int_OLED_WriteCommands(const uint8_t *commands, uint16_t length)
{
    uint8_t tx_data[32];
    bool success;

    if ((commands == NULL) || (length == 0u) || (length > (uint16_t)(sizeof(tx_data) - 1u)))
    {
        return false;
    }

    /* Co=0、D/C#=0：同一事务中的后续字节全部解释为命令。 */
    tx_data[0] = 0x00u;
    memcpy(&tx_data[1], commands, length);
    if (!Int_I2C2Bus_Lock(OLED_I2C_BUS_LOCK_TIMEOUT_MS))
    {
        return false;
    }
    success =
        HAL_I2C_Master_Transmit(
            &hi2c2, OLED_I2C_ADDRESS, tx_data, (uint16_t)(length + 1u), OLED_I2C_TIMEOUT_MS) ==
        HAL_OK;
    Int_I2C2Bus_Unlock();
    return success;
}

static void Int_OLED_DrawPoint(uint8_t x, uint8_t y, bool on)
{
    uint8_t page;
    uint8_t bit;

    if ((x >= OLED_WIDTH) || (y >= OLED_HEIGHT))
    {
        return;
    }

    page = (uint8_t)(y / 8u);
    bit = (uint8_t)(1u << (y % 8u));

    if (on)
    {
        s_oled_gram[page][x] |= bit;
    }
    else
    {
        s_oled_gram[page][x] &= (uint8_t)~bit;
    }
}

static void Int_OLED_ShowChar16(uint8_t x, uint8_t y, char ch)
{
    const Int_OLED_FontGlyphTypeDef *glyph = &g_oled_font_8x16[0];
    uint8_t x_origin = x;
    uint8_t y_origin = y;

    for (uint8_t i = 0u; i < (uint8_t)(sizeof(g_oled_font_8x16) / sizeof(g_oled_font_8x16[0])); i++)
    {
        if (g_oled_font_8x16[i].character == ch)
        {
            glyph = &g_oled_font_8x16[i];
            break;
        }
    }

    for (uint8_t i = 0u; i < OLED_FONT_BYTES; i++)
    {
        uint8_t column = glyph->bitmap[i];

        for (uint8_t bit = 0u; bit < 8u; bit++)
        {
            Int_OLED_DrawPoint(x, y, (column & 0x01u) != 0u);
            column >>= 1u;
            y++;
        }

        x++;
        if ((uint8_t)(x - x_origin) == OLED_FONT_WIDTH)
        {
            x = x_origin;
            y_origin = (uint8_t)(y_origin + 8u);
        }
        y = y_origin;
    }
}

bool Inf_OLED_Refresh(void)
{
    uint8_t tx_data[OLED_PAGE_PACKET_SIZE];

    if (!Int_I2C2Bus_Lock(OLED_I2C_BUS_LOCK_TIMEOUT_MS))
    {
        return false;
    }
    for (uint8_t page = 0u; page < OLED_PAGE_COUNT; page++)
    {
        /*
         * 每页严格一次事务：三个 Co=1 命令控制字之后以 Co=0、D/C#=1
         * 连续发送 128 字节像素。任何页失败都立即终止，最坏阻塞有明确上界。
         */
        tx_data[0] = 0x80u;
        tx_data[1] = (uint8_t)(0xB0u + page);
        tx_data[2] = 0x80u;
        tx_data[3] = 0x00u;
        tx_data[4] = 0x80u;
        tx_data[5] = 0x10u;
        tx_data[6] = 0x40u;
        memcpy(&tx_data[7], s_oled_gram[page], OLED_WIDTH);

        if (HAL_I2C_Master_Transmit(
                &hi2c2, OLED_I2C_ADDRESS, tx_data, sizeof(tx_data), OLED_I2C_TIMEOUT_MS) != HAL_OK)
        {
            Int_I2C2Bus_Unlock();
            return false;
        }
    }
    Int_I2C2Bus_Unlock();
    return true;
}

void Inf_OLED_Clear(void)
{
    /* 只清显存；物理刷新由低优先级 OLED 维护任务显式触发。 */
    memset(s_oled_gram, 0, sizeof(s_oled_gram));
}

void Inf_OLED_ShowText16(uint8_t x, uint8_t y, const char *text)
{
    if (text == NULL)
    {
        return;
    }

    while ((*text != '\0') && (x <= (OLED_WIDTH - OLED_FONT_WIDTH)))
    {
        Int_OLED_ShowChar16(x, y, *text);
        x = (uint8_t)(x + OLED_FONT_WIDTH);
        text++;
    }
}

bool Inf_OLED_Init(void)
{
    /* SSD1315/SSD1306 初始化序列保持与已验证版本完全一致。 */
    static const uint8_t init_commands[] = {0xAEu, 0x00u, 0x10u, 0x40u, 0x81u, 0xCFu, 0xA1u,
                                            0xC8u, 0xA6u, 0xA8u, 0x3Fu, 0xD3u, 0x00u, 0xD5u,
                                            0x80u, 0xD9u, 0xF1u, 0xDAu, 0x12u, 0xDBu, 0x40u,
                                            0x20u, 0x02u, 0x8Du, 0x14u, 0xA4u, 0xA6u};
    static const uint8_t display_on = 0xAFu;
    bool success = false;

    HAL_Delay(OLED_POWER_ON_DELAY_MS);
    if (!Int_I2C2Bus_Lock(OLED_I2C_BUS_LOCK_TIMEOUT_MS))
    {
        return false;
    }
    if (!Int_OLED_WriteCommands(init_commands, sizeof(init_commands)))
    {
        goto done;
    }

    Inf_OLED_Clear();
    if (!Inf_OLED_Refresh())
    {
        goto done;
    }
    success = Int_OLED_WriteCommands(&display_on, 1u);

done:
    Int_I2C2Bus_Unlock();
    return success;
}
