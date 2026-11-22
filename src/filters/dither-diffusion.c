#include "dither-diffusion.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct diffusion_weight {
    int dx, dy;
    float weight;
};

/* Match the existing noita-chat-inspired weight tables exactly. */
static const struct diffusion_weight floyd[] = {
    {1, 0, 7.f / 16.f}, {-1, 1, 3.f / 16.f},
    {0, 1, 5.f / 16.f}, {1, 1, 1.f / 16.f},
};
static const struct diffusion_weight atkinson[] = {
    {1, 0, 1.f / 8.f}, {2, 0, 1.f / 8.f}, {-1, 1, 1.f / 8.f},
    {0, 1, 1.f / 8.f}, {1, 1, 1.f / 8.f}, {0, 2, 1.f / 8.f},
};
static const struct diffusion_weight sierra_lite[] = {
    {1, 0, 2.f / 4.f}, {-1, 1, 1.f / 4.f}, {0, 1, 1.f / 4.f},
};

static float clamp_byte(float v)
{
    return v < 0.f ? 0.f : v > 255.f ? 255.f : v;
}

static void fill_row(float *dst, const uint8_t *source, int width, int source_stride, int y)
{
    const uint8_t *row = source + (size_t)y * source_stride;
    for (int x = 0; x < width; ++x)
        for (int c = 0; c < 3; ++c)
            dst[(size_t)x * 3 + c] = row[4 * x + c];
}

bool retro_dither_diffuse_with_scratch(const uint8_t *source, int width, int height, int source_stride,
                                      uint8_t *output, int output_stride, int levels,
                                      enum retro_dither_diffusion_method method,
                                      float *scratch, size_t scratch_floats)
{
    if (!source || !output || !scratch || source == output || width < 1 || height < 1 || width > 4096 ||
        height > 4096 || (size_t)width * height > 16777216u || source_stride < width * 4 ||
        output_stride < width * 4 || levels < 1 || levels > 255 ||
        scratch_floats < (size_t)width * 9)
        return false;

    const struct diffusion_weight *weights = NULL;
    size_t weight_count = 0;
    switch (method) {
    case RETRO_DITHER_FLOYD:
        weights = floyd;
        weight_count = sizeof(floyd) / sizeof(floyd[0]);
        break;
    case RETRO_DITHER_ATKINSON:
        weights = atkinson;
        weight_count = sizeof(atkinson) / sizeof(atkinson[0]);
        break;
    case RETRO_DITHER_SIERRA_LITE:
        weights = sierra_lite;
        weight_count = sizeof(sierra_lite) / sizeof(sierra_lite[0]);
        break;
    default:
        return false;
    }

    // Rotate three row pointers: no modulo or full-frame indexing in the hot loop.
    float *current = scratch;
    float *next = scratch + (size_t)width * 3;
    float *after = scratch + (size_t)width * 6;
    fill_row(current, source, width, source_stride, 0);
    if (height > 1) fill_row(next, source, width, source_stride, 1);
    if (height > 2) fill_row(after, source, width, source_stride, 2);

    const float step = 255.f / (float)levels;
    for (int y = 0; y < height; ++y) {
        const uint8_t *row = source + (size_t)y * source_stride;
        uint8_t *dest = output + (size_t)y * output_stride;
        for (int x = 0; x < width; ++x) {
            size_t idx = (size_t)x * 3;
            const uint8_t *px = row + 4 * x;
            uint8_t *out = dest + 4 * x;
            memcpy(out, px, 4);
            if (px[3] < 5)
                continue;
            for (int c = 0; c < 3; ++c) {
                float prior = clamp_byte(current[idx + c]);
                float quantized = clamp_byte(roundf(prior / step) * step);
                out[c] = (uint8_t)lroundf(quantized);
                float error = prior - quantized;
                for (size_t j = 0; j < weight_count; ++j) {
                    int nx = x + weights[j].dx, ny = y + weights[j].dy;
                    if (nx < 0 || nx >= width || ny < 0 || ny >= height)
                        continue;
                    const uint8_t *neighbor = source + (size_t)ny * source_stride + nx * 4;
                    if (neighbor[3] != 255)
                        continue;
                    float *neighbor_row = weights[j].dy == 0 ? current :
                                          weights[j].dy == 1 ? next : after;
                    neighbor_row[(size_t)nx * 3 + c] += error * weights[j].weight;
                }
            }
        }
        // The old row is dead. Recycle its storage as future row y+3.
        float *released = current;
        current = next;
        next = after;
        after = released;
        if (y + 3 < height)
            fill_row(after, source, width, source_stride, y + 3);
    }
    return true;
}

bool retro_dither_diffuse(const uint8_t *source, int width, int height, int source_stride, uint8_t *output,
                          int output_stride, int levels, enum retro_dither_diffusion_method method)
{
    if (width < 1 || width > 4096)
        return false;
    const size_t count = (size_t)width * 9;
    float *scratch = malloc(count * sizeof(*scratch));
    if (!scratch)
        return false;
    bool ok = retro_dither_diffuse_with_scratch(source, width, height, source_stride, output,
                                               output_stride, levels, method, scratch, count);
    free(scratch);
    return ok;
}
