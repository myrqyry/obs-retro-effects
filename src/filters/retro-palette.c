#include "retro-palette.h"

static const uint32_t dmg[] = {
    0x0f380f, 0x306230, 0x8bac0f, 0x9bbc0f
};
/* CGA palette 1, high intensity. */
static const uint32_t cga[] = {
    0x000000, 0x55ffff, 0xff55ff, 0xffffff
};
static const uint32_t pico8[] = {
    0x000000, 0x1d2b53, 0x7e2553, 0x008751,
    0xab5236, 0x5f574f, 0xc2c3c7, 0xfff1e8,
    0xff004d, 0xffa300, 0xffec27, 0x00e436,
    0x29adff, 0x83769c, 0xff77a8, 0xffccaa
};

bool retro_palette_colors(int palette, const uint32_t **rgb, size_t *count)
{
    if (!rgb || !count)
        return false;
    switch (palette) {
    case RETRO_PALETTE_DMG:
        *rgb = dmg;
        *count = sizeof(dmg) / sizeof(dmg[0]);
        return true;
    case RETRO_PALETTE_CGA:
        *rgb = cga;
        *count = sizeof(cga) / sizeof(cga[0]);
        return true;
    case RETRO_PALETTE_PICO8:
        *rgb = pico8;
        *count = sizeof(pico8) / sizeof(pico8[0]);
        return true;
    default:
        *rgb = NULL;
        *count = 0;
        return false;
    }
}

static void decode_rgb(uint32_t color, int *r, int *g, int *b)
{
    *r = (color >> 16) & 255;
    *g = (color >> 8) & 255;
    *b = color & 255;
}

bool retro_palette_build_lut(int palette, uint8_t *nearest, uint8_t *second)
{
    const uint32_t *colors;
    size_t count;
    if (!nearest || !second || !retro_palette_colors(palette, &colors, &count) || count < 2)
        return false;

    int decoded[16][3];
    if (count > 16)
        return false;
    for (size_t i = 0; i < count; ++i)
        decode_rgb(colors[i], &decoded[i][0], &decoded[i][1], &decoded[i][2]);

    for (int g = 0; g < 32; ++g)
        for (int b = 0; b < 32; ++b)
            for (int r = 0; r < 32; ++r) {
                int pixel[3] = {r * 255 / 31, g * 255 / 31, b * 255 / 31};
                int first = -1, alternate = -1;
                int d_first = 0x7fffffff, d_alt = 0x7fffffff;
                for (size_t i = 0; i < count; ++i) {
                    int dr = pixel[0] - decoded[i][0];
                    int dg = pixel[1] - decoded[i][1];
                    int db = pixel[2] - decoded[i][2];
                    int d = dr * dr + dg * dg + db * db;
                    if (d < d_first) {
                        alternate = first;
                        d_alt = d_first;
                        first = (int)i;
                        d_first = d;
                    } else if (d < d_alt) {
                        alternate = (int)i;
                        d_alt = d;
                    }
                }
                int delta[3] = {
                    decoded[alternate][0] - decoded[first][0],
                    decoded[alternate][1] - decoded[first][1],
                    decoded[alternate][2] - decoded[first][2],
                };
                int denominator = delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2];
                int numerator = (pixel[0] - decoded[first][0]) * delta[0] +
                                (pixel[1] - decoded[first][1]) * delta[1] +
                                (pixel[2] - decoded[first][2]) * delta[2];
                float mix = denominator > 0 ? (float)numerator / (float)denominator : 0.f;
                if (mix < 0.f)
                    mix = 0.f;
                if (mix > 1.f)
                    mix = 1.f;

                size_t index = ((size_t)g * RETRO_PALETTE_LUT_WIDTH + (size_t)b * 32 + r) * 4;
                nearest[index]     = (uint8_t)decoded[first][0];
                nearest[index + 1] = (uint8_t)decoded[first][1];
                nearest[index + 2] = (uint8_t)decoded[first][2];
                nearest[index + 3] = (uint8_t)(mix * 255.f + 0.5f);
                second[index]     = (uint8_t)decoded[alternate][0];
                second[index + 1] = (uint8_t)decoded[alternate][1];
                second[index + 2] = (uint8_t)decoded[alternate][2];
                second[index + 3] = 255;
            }
    return true;
}
