#include "Int_OLED.h"
#include "Int_font.h"

#include <string.h>

#define OLED_WIDTH       128u
#define OLED_HEIGHT      64u
#define OLED_PAGE_NUM    8u
#define OLED_GRAM_WIDTH  144u

uint8_t OLED_GRAM[OLED_GRAM_WIDTH][OLED_PAGE_NUM];

static void Inf_OLED_DrawPointSafe(int x, int y, uint8_t t)
{
    if ((x < 0) || (x >= (int)OLED_GRAM_WIDTH) ||
        (y < 0) || (y >= (int)OLED_HEIGHT))
    {
        return;
    }

    Inf_OLED_DrawPoint((uint8_t)x, (uint8_t)y, t);
}

void Inf_OLED_WR_Byte(uint8_t dat, uint8_t mode)
{
    uint8_t tx_data[2];

    tx_data[0] = (mode == OLED_CMD) ? 0x00u : 0x40u;
    tx_data[1] = dat;
    (void)HAL_I2C_Master_Transmit(&hi2c2, OLED_I2C_ADDRESS, tx_data, 2u, 100u);
}

void Inf_OLED_ColorTurn(uint8_t i)
{
    if (i == 0u)
    {
        Inf_OLED_WR_Byte(0xA6u, OLED_CMD);
    }
    else
    {
        Inf_OLED_WR_Byte(0xA7u, OLED_CMD);
    }
}

void Inf_OLED_DisplayTurn(uint8_t i)
{
    if (i == 0u)
    {
        Inf_OLED_WR_Byte(0xC8u, OLED_CMD);
        Inf_OLED_WR_Byte(0xA1u, OLED_CMD);
    }
    else
    {
        Inf_OLED_WR_Byte(0xC0u, OLED_CMD);
        Inf_OLED_WR_Byte(0xA0u, OLED_CMD);
    }
}

void Inf_OLED_DisPlay_On(void)
{
    Inf_OLED_WR_Byte(0x8Du, OLED_CMD);
    Inf_OLED_WR_Byte(0x14u, OLED_CMD);
    Inf_OLED_WR_Byte(0xAFu, OLED_CMD);
}

void Inf_OLED_DisPlay_Off(void)
{
    Inf_OLED_WR_Byte(0x8Du, OLED_CMD);
    Inf_OLED_WR_Byte(0x10u, OLED_CMD);
    Inf_OLED_WR_Byte(0xAEu, OLED_CMD);
}

void Inf_OLED_Refresh(void)
{
    for (uint8_t page = 0u; page < OLED_PAGE_NUM; page++)
    {
        Inf_OLED_WR_Byte((uint8_t)(0xB0u + page), OLED_CMD);
        Inf_OLED_WR_Byte(0x00u, OLED_CMD);
        Inf_OLED_WR_Byte(0x10u, OLED_CMD);

        for (uint8_t col = 0u; col < OLED_WIDTH; col++)
        {
            Inf_OLED_WR_Byte(OLED_GRAM[col][page], OLED_DATA);
        }
    }
}

void Inf_OLED_Clear(void)
{
    memset(OLED_GRAM, 0, sizeof(OLED_GRAM));
    Inf_OLED_Refresh();
}

void Inf_OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t t)
{
    uint8_t page;
    uint8_t bit;

    if ((x >= OLED_GRAM_WIDTH) || (y >= OLED_HEIGHT))
    {
        return;
    }

    page = (uint8_t)(y / 8u);
    bit = (uint8_t)(1u << (y % 8u));

    if (t)
    {
        OLED_GRAM[x][page] |= bit;
    }
    else
    {
        OLED_GRAM[x][page] &= (uint8_t)(~bit);
    }
}

void Inf_OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t mode)
{
    int x = x1;
    int y = y1;
    int dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
    int dy = (y2 > y1) ? (y2 - y1) : (y1 - y2);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (1)
    {
        if ((x >= 0) && (x < (int)OLED_WIDTH) &&
            (y >= 0) && (y < (int)OLED_HEIGHT))
        {
            Inf_OLED_DrawPoint((uint8_t)x, (uint8_t)y, mode);
        }

        if ((x == x2) && (y == y2))
        {
            break;
        }

        if ((err * 2) > -dy)
        {
            err -= dy;
            x += sx;
        }

        if ((err * 2) < dx)
        {
            err += dx;
            y += sy;
        }
    }
}

void Inf_OLED_DrawCircle(uint8_t x, uint8_t y, uint8_t r)
{
    int a = 0;
    int b = r;
    int d = 3 - (2 * r);

    while (a <= b)
    {
        Inf_OLED_DrawPointSafe((int)x + a, (int)y + b, 1u);
        Inf_OLED_DrawPointSafe((int)x + b, (int)y + a, 1u);
        Inf_OLED_DrawPointSafe((int)x + b, (int)y - a, 1u);
        Inf_OLED_DrawPointSafe((int)x + a, (int)y - b, 1u);
        Inf_OLED_DrawPointSafe((int)x - a, (int)y - b, 1u);
        Inf_OLED_DrawPointSafe((int)x - b, (int)y - a, 1u);
        Inf_OLED_DrawPointSafe((int)x - b, (int)y + a, 1u);
        Inf_OLED_DrawPointSafe((int)x - a, (int)y + b, 1u);

        if (d < 0)
        {
            d += (4 * a) + 6;
        }
        else
        {
            d += 4 * (a - b) + 10;
            b--;
        }
        a++;
    }
}

void Inf_OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr, uint8_t size1, uint8_t mode)
{
    uint8_t temp;
    uint8_t size2;
    uint8_t chr_index;
    uint8_t x0 = x;
    uint8_t y0 = y;

    if ((chr < ' ') || (chr > '~'))
    {
        return;
    }

    if (size1 == 8u)
    {
        size2 = 6u;
    }
    else if ((size1 == 12u) || (size1 == 16u) || (size1 == 24u))
    {
        size2 = (uint8_t)((size1 / 8u + ((size1 % 8u) ? 1u : 0u)) * (size1 / 2u));
    }
    else
    {
        return;
    }

    chr_index = (uint8_t)(chr - ' ');

    for (uint8_t i = 0u; i < size2; i++)
    {
        if (size1 == 8u)
        {
            temp = asc2_0806[chr_index][i];
        }
        else if (size1 == 12u)
        {
            temp = asc2_1206[chr_index][i];
        }
        else if (size1 == 16u)
        {
            temp = asc2_1608[chr_index][i];
        }
        else
        {
            temp = asc2_2412[chr_index][i];
        }

        for (uint8_t m = 0u; m < 8u; m++)
        {
            Inf_OLED_DrawPoint(x, y, (temp & 0x01u) ? mode : (uint8_t)!mode);
            temp >>= 1u;
            y++;
        }

        x++;
        if ((size1 != 8u) && ((uint8_t)(x - x0) == (size1 / 2u)))
        {
            x = x0;
            y0 = (uint8_t)(y0 + 8u);
        }
        y = y0;
    }
}

void Inf_OLED_ShowString(uint8_t x, uint8_t y, uint8_t *chr, uint8_t size1, uint8_t mode)
{
    uint8_t step;

    if (chr == 0)
    {
        return;
    }

    if ((size1 != 8u) && (size1 != 12u) && (size1 != 16u) && (size1 != 24u))
    {
        return;
    }

    step = (size1 == 8u) ? 6u : (uint8_t)(size1 / 2u);

    /*
     * 教学版显示函数只负责一行英文字符串。
     * 如果字符串超出 128 列，直接截断，防止写穿 OLED_GRAM。
     */
    while ((*chr != '\0') && (x < OLED_WIDTH))
    {
        Inf_OLED_ShowChar(x, y, *chr, size1, mode);
        x = (uint8_t)(x + step);
        chr++;
    }
}

uint32_t Inf_OLED_Pow(uint8_t m, uint8_t n)
{
    uint32_t result = 1u;

    while (n-- > 0u)
    {
        result *= m;
    }

    return result;
}

void Inf_OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size1, uint8_t mode)
{
    uint8_t offset = (size1 == 8u) ? 2u : 0u;

    for (uint8_t t = 0u; t < len; t++)
    {
        uint8_t temp = (uint8_t)((num / Inf_OLED_Pow(10u, (uint8_t)(len - t - 1u))) % 10u);
        Inf_OLED_ShowChar((uint8_t)(x + (size1 / 2u + offset) * t),
                          y,
                          (uint8_t)(temp + '0'),
                          size1,
                          mode);
    }
}

void Inf_OLED_ShowChinese(uint8_t x, uint8_t y, uint8_t num, uint8_t size1, uint8_t mode)
{
    uint8_t temp;
    uint8_t x0 = x;
    uint8_t y0 = y;
    uint16_t font_size;

    if ((size1 != 16u) || (num >= (sizeof(Hzk1) / sizeof(Hzk1[0]))))
    {
        return;
    }

    font_size = (uint16_t)((size1 / 8u + ((size1 % 8u) ? 1u : 0u)) * size1);

    for (uint16_t i = 0u; i < font_size; i++)
    {
        temp = Hzk1[num][i];
        for (uint8_t m = 0u; m < 8u; m++)
        {
            Inf_OLED_DrawPoint(x, y, (temp & 0x01u) ? mode : (uint8_t)!mode);
            temp >>= 1u;
            y++;
        }

        x++;
        if ((uint8_t)(x - x0) == size1)
        {
            x = x0;
            y0 = (uint8_t)(y0 + 8u);
        }
        y = y0;
    }
}

void Inf_OLED_ScrollDisplay(uint8_t num, uint8_t space, uint8_t mode)
{
    uint8_t t = 0u;
    uint8_t m = 0u;

    /*
     * 这是旧教学例程里的阻塞滚动演示函数。
     * 业务页面不要调用它，否则会长期占用 I2C2 和主循环。
     */
    while (1)
    {
        if (m == 0u)
        {
            Inf_OLED_ShowChinese(128u, 24u, t, 16u, mode);
            t++;
        }

        if (t == num)
        {
            for (uint8_t r = 0u; r < (uint8_t)(16u * space); r++)
            {
                for (uint8_t i = 1u; i < OLED_GRAM_WIDTH; i++)
                {
                    for (uint8_t n = 0u; n < OLED_PAGE_NUM; n++)
                    {
                        OLED_GRAM[i - 1u][n] = OLED_GRAM[i][n];
                    }
                }
                Inf_OLED_Refresh();
                HAL_Delay(20u);
            }
            t = 0u;
        }

        m++;
        if (m == 16u)
        {
            m = 0u;
        }

        for (uint8_t i = 1u; i < OLED_GRAM_WIDTH; i++)
        {
            for (uint8_t n = 0u; n < OLED_PAGE_NUM; n++)
            {
                OLED_GRAM[i - 1u][n] = OLED_GRAM[i][n];
            }
        }

        Inf_OLED_Refresh();
        HAL_Delay(20u);
    }
}

void Inf_OLED_ShowPicture(uint8_t x, uint8_t y, uint8_t sizex, uint8_t sizey, uint8_t BMP[], uint8_t mode)
{
    uint16_t j = 0u;
    uint8_t x0 = x;
    uint8_t y0 = y;
    uint8_t page_num;

    if (BMP == 0)
    {
        return;
    }

    page_num = (uint8_t)(sizey / 8u + ((sizey % 8u) ? 1u : 0u));

    for (uint8_t n = 0u; n < page_num; n++)
    {
        for (uint8_t i = 0u; i < sizex; i++)
        {
            uint8_t temp = BMP[j++];
            for (uint8_t m = 0u; m < 8u; m++)
            {
                Inf_OLED_DrawPoint(x, y, (temp & 0x01u) ? mode : (uint8_t)!mode);
                temp >>= 1u;
                y++;
            }

            x++;
            if ((uint8_t)(x - x0) == sizex)
            {
                x = x0;
                y0 = (uint8_t)(y0 + 8u);
            }
            y = y0;
        }
    }
}

void Inf_OLED_Init(void)
{
    HAL_Delay(800u);

    Inf_OLED_WR_Byte(0xAEu, OLED_CMD);
    Inf_OLED_WR_Byte(0x00u, OLED_CMD);
    Inf_OLED_WR_Byte(0x10u, OLED_CMD);
    Inf_OLED_WR_Byte(0x40u, OLED_CMD);
    Inf_OLED_WR_Byte(0x81u, OLED_CMD);
    Inf_OLED_WR_Byte(0xCFu, OLED_CMD);
    Inf_OLED_WR_Byte(0xA1u, OLED_CMD);
    Inf_OLED_WR_Byte(0xC8u, OLED_CMD);
    Inf_OLED_WR_Byte(0xA6u, OLED_CMD);
    Inf_OLED_WR_Byte(0xA8u, OLED_CMD);
    Inf_OLED_WR_Byte(0x3Fu, OLED_CMD);
    Inf_OLED_WR_Byte(0xD3u, OLED_CMD);
    Inf_OLED_WR_Byte(0x00u, OLED_CMD);
    Inf_OLED_WR_Byte(0xD5u, OLED_CMD);
    Inf_OLED_WR_Byte(0x80u, OLED_CMD);
    Inf_OLED_WR_Byte(0xD9u, OLED_CMD);
    Inf_OLED_WR_Byte(0xF1u, OLED_CMD);
    Inf_OLED_WR_Byte(0xDAu, OLED_CMD);
    Inf_OLED_WR_Byte(0x12u, OLED_CMD);
    Inf_OLED_WR_Byte(0xDBu, OLED_CMD);
    Inf_OLED_WR_Byte(0x40u, OLED_CMD);
    Inf_OLED_WR_Byte(0x20u, OLED_CMD);
    Inf_OLED_WR_Byte(0x02u, OLED_CMD);
    Inf_OLED_WR_Byte(0x8Du, OLED_CMD);
    Inf_OLED_WR_Byte(0x14u, OLED_CMD);
    Inf_OLED_WR_Byte(0xA4u, OLED_CMD);
    Inf_OLED_WR_Byte(0xA6u, OLED_CMD);

    Inf_OLED_Clear();
    Inf_OLED_WR_Byte(0xAFu, OLED_CMD);
}
