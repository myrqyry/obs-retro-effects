#include "../src/filters/retro-palette.h"
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); return 1; } } while (0)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const size_t BYTES = (size_t)RETRO_PALETTE_LUT_WIDTH * RETRO_PALETTE_LUT_HEIGHT * 4;

static const uint8_t *lookup(const uint8_t *texture, int r, int g, int b)
{
    int rx = (r * 31 + 127) / 255;
    int gy = (g * 31 + 127) / 255;
    int bz = (b * 31 + 127) / 255;
    return texture + ((size_t)gy * RETRO_PALETTE_LUT_WIDTH + (size_t)bz * 32 + rx) * 4;
}

static int is_color(const uint8_t *p, unsigned int hex)
{
    return p[0] == (hex >> 16 & 255) && p[1] == (hex >> 8 & 255) &&
           p[2] == (hex & 255);
}

int main(void)
{
    const uint32_t *colors = NULL;
    size_t count = 0;
    CHECK(retro_palette_colors(RETRO_PALETTE_DMG, &colors, &count) && count == 4);
    CHECK(colors[0] == 0x0f380f && colors[3] == 0x9bbc0f);
    CHECK(retro_palette_colors(RETRO_PALETTE_CGA, &colors, &count) && count == 4);
    CHECK(colors[0] == 0x000000 && colors[1] == 0x55ffff);
    CHECK(retro_palette_colors(RETRO_PALETTE_PICO8, &colors, &count) && count == 16);
    CHECK(colors[8] == 0xff004d);
    CHECK(!retro_palette_colors(RETRO_PALETTE_NONE, &colors, &count));
    CHECK(!retro_palette_colors(99, &colors, &count));
    uint8_t *near = malloc(BYTES), *second = malloc(BYTES);
    CHECK(near && second);
    CHECK(!retro_palette_build_lut(99, near, second));
    CHECK(!retro_palette_build_lut(RETRO_PALETTE_DMG, NULL, second));

    CHECK(retro_palette_build_lut(RETRO_PALETTE_CGA, near, second));
    // Exact palette endpoints stay unaltered, and must never dither away.
    const uint8_t *black = lookup(near, 0, 0, 0);
    CHECK(is_color(black, 0x000000) && black[3] == 0);
    const uint8_t *white = lookup(near, 255, 255, 255);
    CHECK(is_color(white, 0xffffff) && white[3] == 0);
    // LUT stores two *actual* palette colors: never an RGB intermediate.
    const uint8_t *middle = lookup(near, 128, 140, 155);
    const uint8_t *alternate = lookup(second, 128, 140, 155);
    CHECK(middle[3] <= 255);
    CHECK(is_color(middle, 0x000000) || is_color(middle, 0x55ffff) ||
           is_color(middle, 0xff55ff) || is_color(middle, 0xffffff));
    CHECK(is_color(alternate, 0x000000) || is_color(alternate, 0x55ffff) ||
           is_color(alternate, 0xff55ff) || is_color(alternate, 0xffffff));
    CHECK(memcmp(middle, alternate, 3) != 0);

    CHECK(retro_palette_build_lut(RETRO_PALETTE_DMG, near, second));
    CHECK(is_color(lookup(near, 0, 0, 0), 0x0f380f));
    const uint8_t *dmg = lookup(near, 150, 190, 25);
    CHECK(is_color(dmg, 0x8bac0f) || is_color(dmg, 0x9bbc0f) ||
           is_color(dmg, 0x306230) || is_color(dmg, 0x0f380f));
    CHECK(retro_palette_build_lut(RETRO_PALETTE_PICO8, near, second));
    const uint8_t *red = lookup(near, 255, 0, 80);
    CHECK(is_color(red, 0xff004d));
    free(near);
    free(second);
    puts("RETRO_PALETTE_LUT_OK");
    return 0;
}
