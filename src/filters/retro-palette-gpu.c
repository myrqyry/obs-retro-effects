#include "retro-palette-gpu.h"
#include "retro-palette.h"
#include <stdlib.h>

void retro_palette_gpu_destroy(struct retro_palette_gpu *gpu)
{
    if (!gpu) return;
    if (gpu->primary) gs_texture_destroy(gpu->primary);
    if (gpu->alternate) gs_texture_destroy(gpu->alternate);
    gpu->primary = NULL;
    gpu->alternate = NULL;
    gpu->palette_id = RETRO_PALETTE_NONE;
}

bool retro_palette_gpu_ensure(struct retro_palette_gpu *gpu, int palette_id)
{
    if (!gpu || palette_id == RETRO_PALETTE_NONE) return false;
    if (gpu->palette_id == palette_id && gpu->primary && gpu->alternate)
        return true;

    const size_t bytes = (size_t)RETRO_PALETTE_LUT_WIDTH * RETRO_PALETTE_LUT_HEIGHT * 4;
    uint8_t *pixels = malloc(bytes * 2);
    if (!pixels) return false;
    if (!retro_palette_build_lut(palette_id, pixels, pixels + bytes)) {
        free(pixels);
        return false;
    }

    const uint8_t *primary_pixels[] = {pixels};
    const uint8_t *alternate_pixels[] = {pixels + bytes};
    gs_texture_t *primary = gs_texture_create(RETRO_PALETTE_LUT_WIDTH, RETRO_PALETTE_LUT_HEIGHT,
                                              GS_RGBA, 1, primary_pixels, 0);
    gs_texture_t *alternate = gs_texture_create(RETRO_PALETTE_LUT_WIDTH, RETRO_PALETTE_LUT_HEIGHT,
                                                GS_RGBA, 1, alternate_pixels, 0);
    free(pixels);
    if (!primary || !alternate) {
        if (primary) gs_texture_destroy(primary);
        if (alternate) gs_texture_destroy(alternate);
        return false;
    }

    retro_palette_gpu_destroy(gpu);
    gpu->primary = primary;
    gpu->alternate = alternate;
    gpu->palette_id = palette_id;
    return true;
}
