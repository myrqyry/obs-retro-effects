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
static void verify_reusable_scratch(void)
{
    enum { w = 37, h = 19, stride = w * 4 + 7 };
    uint8_t source[stride * h], old_output[stride * h], scratch_output[stride * h];
    float work[w * 9];
    for (size_t i = 0; i < sizeof(source); ++i)
        source[i] = (uint8_t)((i * 29 + (i >> 3) * 13) & 255);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            static const uint8_t alphas[] = {0, 4, 5, 80, 255};
            source[y * stride + x * 4 + 3] = alphas[(x + y * 7) % 5];
        }
    for (int method = 1; method <= 3; ++method)
        for (int levels = 1; levels <= 32; levels *= 2) {
            memset(old_output, 0xa5, sizeof(old_output));
            memset(scratch_output, 0xa5, sizeof(scratch_output));
            assert(retro_dither_diffuse(source, w, h, stride, old_output, stride, levels,
                                        (enum retro_dither_diffusion_method)method));
            assert(retro_dither_diffuse_with_scratch(source, w, h, stride, scratch_output, stride, levels,
                                                     (enum retro_dither_diffusion_method)method,
                                                     work, w * 9));
            assert(memcmp(old_output, scratch_output, sizeof(old_output)) == 0);
        }
    assert(!retro_dither_diffuse_with_scratch(source, w, h, stride, scratch_output, stride, 1,
                                              RETRO_DITHER_FLOYD, work, w * 9 - 1));
    puts("ERROR_DIFFUSION_SCRATCH_PARITY_PASS");
}

int main(void)
{
	verify_two_pixels();
	verify_methods();
	verify_reusable_scratch();
	return 0;
}
