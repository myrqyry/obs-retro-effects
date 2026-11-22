#pragma once
#include <graphics/graphics.h>
#include <stdbool.h>

struct retro_palette_gpu {
    gs_texture_t *primary;
    gs_texture_t *alternate;
    int palette_id;
};

/* Call inside OBS graphics context (video_render or explicit enter/leave). */
bool retro_palette_gpu_ensure(struct retro_palette_gpu *gpu, int palette_id);
void retro_palette_gpu_destroy(struct retro_palette_gpu *gpu);
