#include "dither-diffusion.h"

/* Replaces only the CPU diffusion kernel when ENABLE_RUST_DITHER is selected.
 * The OBS graphics path remains unchanged, and the original C kernel is the
 * default on every platform. */
extern bool retro_dither_diffuse_rs(const uint8_t *, int, int, int, uint8_t *, int, int, int);

bool retro_dither_diffuse(const uint8_t *source, int width, int height, int source_stride, uint8_t *output,
			  int output_stride, int levels, enum retro_dither_diffusion_method method)
{
	return retro_dither_diffuse_rs(source, width, height, source_stride, output, output_stride, levels,
				       (int)method);
}
