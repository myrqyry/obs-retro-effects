#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../src/filters/dither-diffusion.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Rust has a separate symbol so this test exercises both real implementations. */
extern bool retro_dither_diffuse_rs(const uint8_t *, int, int, int, uint8_t *, int, int, int);

static uint32_t seed = 0x4d595251u;
static uint32_t random_u32(void)
{
	seed ^= seed << 13;
	seed ^= seed >> 17;
	seed ^= seed << 5;
	return seed;
}

static void compare_case(int width, int height, int padding, int levels, int method)
{
	size_t stride = (size_t)width * 4 + (size_t)padding;
	size_t length = stride * (size_t)height;
	uint8_t *src = malloc(length), *c = malloc(length), *rs = malloc(length);
	assert(src && c && rs);
	for (size_t i = 0; i < length; ++i)
		src[i] = (uint8_t)random_u32();
	for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x) {
			size_t a = (size_t)y * stride + (size_t)x * 4 + 3;
			static const uint8_t alphas[] = {0, 1, 4, 5, 80, 128, 254, 255, 255, 255};
			src[a] = alphas[random_u32() % (sizeof(alphas) / sizeof(alphas[0]))];
		}
	memset(c, 0xa5, length);
	memset(rs, 0xa5, length);
	assert(retro_dither_diffuse(src, width, height, (int)stride, c, (int)stride, levels,
				    (enum retro_dither_diffusion_method)method));
	assert(retro_dither_diffuse_rs(src, width, height, (int)stride, rs, (int)stride, levels, method));
	if (memcmp(c, rs, length) != 0) {
		for (size_t k = 0; k < length; ++k)
			if (c[k] != rs[k]) {
				fprintf(stderr,
					"DIFF width=%d height=%d padding=%d levels=%d method=%d at=%zu C=%u Rust=%u\n",
					width, height, padding, levels, method, k, c[k], rs[k]);
				abort();
			}
	}
	free(src);
	free(c);
	free(rs);
}

static void parity(void)
{
	const int sizes[][2] = {{1, 1}, {1, 9}, {9, 1}, {2, 2}, {3, 5}, {13, 11}, {64, 36}, {127, 73}};
	const int levels[] = {1, 2, 3, 4, 7, 16, 32, 255};
	unsigned checks = 0;
	for (int mode = 1; mode <= 3; ++mode)
		for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i)
			for (size_t j = 0; j < sizeof(levels) / sizeof(levels[0]); ++j)
				for (int pad = 0; pad <= 9; pad += 9) {
					compare_case(sizes[i][0], sizes[i][1], pad, levels[j], mode);
					++checks;
				}
	uint8_t source[16] = {0}, output[16] = {0};
	for (int mode = -1; mode <= 4; ++mode) {
		bool valid = mode >= 1 && mode <= 3;
		assert(retro_dither_diffuse_rs(source, 1, 1, 4, output, 4, 2, mode) == valid);
	}
	assert(!retro_dither_diffuse_rs(NULL, 1, 1, 4, output, 4, 1, 1));
	assert(!retro_dither_diffuse_rs(source, 1, 1, 4, NULL, 4, 1, 1));
	assert(!retro_dither_diffuse_rs(source, 0, 1, 4, output, 4, 1, 1));
	assert(!retro_dither_diffuse_rs(source, 1, 1, 3, output, 4, 1, 1));
	assert(!retro_dither_diffuse_rs(source, 1, 1, 4, output, 4, 0, 1));
	assert(!retro_dither_diffuse_rs(source, 1, 1, 4, output, 4, 256, 1));
	assert(!retro_dither_diffuse_rs(source, 1, 1, 4, source, 4, 1, 1));
	assert(!retro_dither_diffuse_rs(source, 1, 1, 4, source + 1, 4, 1, 1));
	assert(!retro_dither_diffuse_rs(source + 1, 1, 1, 4, source, 4, 1, 1));
	puts("PASS Rust/C parity: 384 pixel-buffer comparisons, three kernels, alpha, strides, and invalid inputs");
}

static double elapsed_ms(const struct timespec *start, const struct timespec *end)
{
	return (end->tv_sec - start->tv_sec) * 1000.0 + (end->tv_nsec - start->tv_nsec) / 1.0e6;
}

static void bench(int width, int height, int count)
{
	size_t size = (size_t)width * height * 4;
	uint8_t *src = malloc(size), *out = malloc(size);
	assert(src && out);
	for (size_t i = 0; i < size; ++i)
		src[i] = (uint8_t)random_u32();
	for (size_t i = 3; i < size; i += 4)
		src[i] = 255;
	for (int method = 1; method <= 3; ++method)
		for (int which = 0; which <= 1; ++which) {
			struct timespec before, after;
			assert(which ? retro_dither_diffuse_rs(src, width, height, width * 4, out, width * 4, 2, method)
				     : retro_dither_diffuse(src, width, height, width * 4, out, width * 4, 2,
							    (enum retro_dither_diffusion_method)method));
			clock_gettime(CLOCK_MONOTONIC, &before);
			for (int n = 0; n < count; ++n)
				assert(which ? retro_dither_diffuse_rs(src, width, height, width * 4, out, width * 4, 2,
								       method)
					     : retro_dither_diffuse(src, width, height, width * 4, out, width * 4, 2,
								    (enum retro_dither_diffusion_method)method));
			clock_gettime(CLOCK_MONOTONIC, &after);
			printf("BENCH %dx%d method=%d engine=%s avg_ms=%.3f iterations=%d checksum=%u\n", width, height,
			       method, which ? "Rust" : "C", elapsed_ms(&before, &after) / count, count, out[0]);
		}
	free(src);
	free(out);
}

int main(int argc, char **argv)
{
	parity();
	if (argc > 1 && strcmp(argv[1], "--bench") == 0) {
		bench(512, 288, 12);
		bench(1024, 576, 4);
	}
	return 0;
}
