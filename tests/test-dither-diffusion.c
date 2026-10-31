#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/filters/dither-diffusion.h"

static void verify_two_pixels(void)
{
	uint8_t rgba[8] = {120, 120, 120, 255, 120, 120, 120, 255};
	uint8_t result[8] = {0};
	assert(retro_dither_diffuse(rgba, 2, 1, 8, result, 8, 1, RETRO_DITHER_FLOYD));
	assert(result[0] == 0 && result[4] == 255);
	assert(result[3] == 255 && result[7] == 255);
}
static void verify_methods(void)
{
	enum { w = 13, h = 11 };
	uint8_t source[w * h * 4], result[w * h * 4];
	for (int y = 0; y < h; ++y)
		for (int x = 0; x < w; ++x) {
			size_t i = (size_t)(y * w + x) * 4;
			source[i] = source[i + 1] = source[i + 2] = (uint8_t)((x * 19 + y * 13) % 256);
			source[i + 3] = 255;
		}
	unsigned long fingerprints[3] = {0};
	for (int method = RETRO_DITHER_FLOYD; method <= RETRO_DITHER_SIERRA_LITE; ++method) {
		assert(retro_dither_diffuse(source, w, h, w * 4, result, w * 4, 1,
					    (enum retro_dither_diffusion_method)method));
		unsigned long fingerprint = 0;
		for (size_t i = 0; i < sizeof(result); i += 4) {
			assert(result[i] == 0 || result[i] == 255);
			assert(result[i] == result[i + 1] && result[i + 1] == result[i + 2]);
			assert(result[i + 3] == 255);
			fingerprint = fingerprint * 31u + (unsigned long)(result[i] > 0);
		}
		fingerprints[method - 1] = fingerprint;
	}
	assert(fingerprints[0] != fingerprints[1] && fingerprints[1] != fingerprints[2] &&
	       fingerprints[0] != fingerprints[2]);
	// Non-opaque pixels must not receive propagated errors.
	source[0] = 120;
	source[1] = 120;
	source[2] = 120;
	source[3] = 0;
	assert(retro_dither_diffuse(source, w, h, w * 4, result, w * 4, 1, RETRO_DITHER_ATKINSON));
	assert(result[3] == 0 && result[0] == 120);
	assert(!retro_dither_diffuse(source, 0, h, w * 4, result, w * 4, 1, RETRO_DITHER_FLOYD));
	printf("ERROR_DIFFUSION_METHODS_PASS Floyd Atkinson SierraLite\n");
}
int main(void)
{
	verify_two_pixels();
	verify_methods();
	return 0;
}
