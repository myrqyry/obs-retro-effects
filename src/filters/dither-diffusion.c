#include "dither-diffusion.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct diffusion_weight {
	int dx, dy;
	float weight;
};

/* Weights copied from noita-chat/utils/colorScience.ts getDiffusionWeights.
 * Applied to OBS's uniform per-channel palette instead of noitachat's
 * nearest-color user palette. They are the actual error propagation kernels. */
static const struct diffusion_weight floyd[] = {
	{1, 0, 7.f / 16.f},
	{-1, 1, 3.f / 16.f},
	{0, 1, 5.f / 16.f},
	{1, 1, 1.f / 16.f},
};
static const struct diffusion_weight atkinson[] = {
	{1, 0, 1.f / 8.f}, {2, 0, 1.f / 8.f}, {-1, 1, 1.f / 8.f},
	{0, 1, 1.f / 8.f}, {1, 1, 1.f / 8.f}, {0, 2, 1.f / 8.f},
};
static const struct diffusion_weight sierra_lite[] = {
	{1, 0, 2.f / 4.f},
	{-1, 1, 1.f / 4.f},
	{0, 1, 1.f / 4.f},
};
static float clamp_byte(float v)
{
	return v < 0.f ? 0.f : v > 255.f ? 255.f : v;
}
bool retro_dither_diffuse(const uint8_t *source, int width, int height, int source_stride, uint8_t *output,
			  int output_stride, int levels, enum retro_dither_diffusion_method method)
{
	if (!source || !output || source == output || width < 1 || height < 1 || width > 4096 || height > 4096 ||
	    (size_t)width * height > 16777216u || source_stride < width * 4 || output_stride < width * 4 ||
	    levels < 1 || levels > 255)
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

	const size_t total = (size_t)width * height;
	if (total > SIZE_MAX / (3 * sizeof(float)))
		return false;
	float *work = malloc(total * 3 * sizeof(float));
	if (!work)
		return false;
	for (int y = 0; y < height; ++y) {
		const uint8_t *row = source + (size_t)y * source_stride;
		for (int x = 0; x < width; ++x) {
			size_t at = ((size_t)y * width + x) * 3;
			for (int c = 0; c < 3; ++c)
				work[at + c] = row[4 * x + c];
		}
	}
	const float step = 255.f / (float)levels;
	for (int y = 0; y < height; ++y) {
		const uint8_t *row = source + (size_t)y * source_stride;
		uint8_t *dest = output + (size_t)y * output_stride;
		for (int x = 0; x < width; ++x) {
			size_t idx = ((size_t)y * width + x) * 3;
			const uint8_t *px = row + 4 * x;
			uint8_t *out = dest + 4 * x;
			memcpy(out, px, 4);
			if (px[3] < 5)
				continue;
			for (int c = 0; c < 3; ++c) {
				float prior = clamp_byte(work[idx + c]);
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
					work[((size_t)ny * width + nx) * 3 + c] += error * weights[j].weight;
				}
			}
		}
	}
	free(work);
	return true;
}
