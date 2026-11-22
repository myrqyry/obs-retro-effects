#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 32x32x32 RGB lookup packed as 1024x32 RGBA8 textures. */
#define RETRO_PALETTE_LUT_WIDTH 1024
#define RETRO_PALETTE_LUT_HEIGHT 32

enum retro_palette_id {
    RETRO_PALETTE_NONE = 0,
    RETRO_PALETTE_DMG = 1,
    RETRO_PALETTE_CGA = 2,
    RETRO_PALETTE_PICO8 = 3,
};

bool retro_palette_colors(int palette, const uint32_t **rgb, size_t *count);
/* nearest RGB + probability of choosing second color in its alpha channel.
 * second texture contains second-nearest palette color. Caller owns buffers. */
bool retro_palette_build_lut(int palette, uint8_t *nearest, uint8_t *second);
