#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

enum retro_dither_diffusion_method {
	RETRO_DITHER_FLOYD = 1,
	RETRO_DITHER_ATKINSON = 2,
	RETRO_DITHER_SIERRA_LITE = 3,
};

/* Real left-to-right error diffusion, preserving transparent pixels.
 * Input must contain gamma/contrast corrected RGBA pixels at the intended
 * logical render resolution. levels=1 means a two-color-per-channel palette.
 * Output may not alias input.  Returns false on invalid input or allocation.
 */
bool retro_dither_diffuse(const uint8_t *source, int width, int height,
			  int source_stride, uint8_t *output, int output_stride,
			  int levels, enum retro_dither_diffusion_method method);

/* Reusable 3-row workspace. Caller owns at least width * 9 floats.
 * Preserves the exact output of retro_dither_diffuse; no allocations. */
bool retro_dither_diffuse_with_scratch(const uint8_t *source, int width, int height, int source_stride,
                                      uint8_t *output, int output_stride, int levels,
                                      enum retro_dither_diffusion_method method,
                                      float *scratch, size_t scratch_floats);
