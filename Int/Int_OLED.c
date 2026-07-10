#include "Int_OLED.h"

#include <stdbool.h>
#include <string.h>

#include "Int_font.h"
#include "i2c.h"

enum
{
    OLED_WIDTH = 128u,
    OLED_HEIGHT = 64u,
    OLED_PAGE_COUNT = 8u,
    OLED_I2C_ADDRESS = 0x78u,
    OLED_COMMAND = 0u,
    OLED_DATA = 1u,
    OLED_I2C_TIMEOUT_MS = 100u,
    OLED_FONT_WIDTH = 8u,
    OLED_FONT_BYTES = 16u
};

static uint8_t s_oled_gram[OLED_WIDTH][OLED_PAGE_COUNT];

static void Int_OLED_WriteByte(uint8_t data, uint8_t mode)
{
    uint8_t tx_data[2];

    tx_data[0] = (mode == OLED_COMMAND) ? 0x00u : 0x40u;
    tx_data[1] = data;
    (void)HAL_I2C_Master_Transmit(&hi2c2,
                                  OLED_I2C_ADDRESS,
                                  tx_data,
                                  sizeof(tx_data),
                                  OLED_I2C_TIMEOUT_MS);
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
        s_oled_gram[x][page] |= bit;
    }
    else
    {
        s_oled_gram[x][page] &= (uint8_t)~bit;
    }
}

static void Int_OLED_ShowChar16(uint8_t x, uint8_t y, char ch)
{
    const Int_OLED_FontGlyphTypeDef *glyph = &g_oled_font_8x16[0];
    uint8_t x_origin = x;
    uint8_t y_origin = y;

    for (uint8_t i = 0u;
         i < (uint8_t)(sizeof(g_oled_font_8x16) / sizeof(g_oled_font_8x16[0]));
         i++)
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

void Inf_OLED_Refresh(void)
{
    for (uint8_t page = 0u; page < OLED_PAGE_COUNT; page++)
    {
        Int_OLED_WriteByte((uint8_t)(0xB0u + page), OLED_COMMAND);
        Int_OLED_WriteByte(0x00u, OLED_COMMAND);
        Int_OLED_WriteByte(0x10u, OLED_COMMAND);

        for (uint8_t col = 0u; col < OLED_WIDTH; col++)
        {
            Int_OLED_WriteByte(s_oled_gram[col][page], OLED_DATA);
        }
    }
}

void Inf_OLED_Clear(void)
{
    memset(s_oled_gram, 0, sizeof(s_oled_gram));
    Inf_OLED_Refresh();
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

void Inf_OLED_Init(void)
{
    volatile uint32_t i;

    for (i = 0u; i < 5120000u; i++)
    {
    }

    /* SSD1315/SSD1306 初始化序列保持与已验证版本完全一致。 */
    static const uint8_t init_commands[] =
    {
        0xAEu, 0x00u, 0x10u, 0x40u, 0x81u, 0xCFu, 0xA1u, 0xC8u,
        0xA6u, 0xA8u, 0x3Fu, 0xD3u, 0x00u, 0xD5u, 0x80u, 0xD9u,
        0xF1u, 0xDAu, 0x12u, 0xDBu, 0x40u, 0x20u, 0x02u, 0x8Du,
        0x14u, 0xA4u, 0xA6u
    };

    for (i = 0u; i < sizeof(init_commands); i++)
    {
        Int_OLED_WriteByte(init_commands[i], OLED_COMMAND);
    }

    Inf_OLED_Clear();
    Int_OLED_WriteByte(0xAFu, OLED_COMMAND);
}
