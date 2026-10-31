#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/filters/blue-noise-mask.generated.h"

int main(void)
{
	unsigned int histogram[256] = {0};
	assert(RETRO_BLUE_NOISE_SIZE == 64);
	assert(sizeof(retro_blue_noise_r8) == 4096);
	for (size_t p = 0; p < sizeof(retro_blue_noise_r8); ++p)
		histogram[retro_blue_noise_r8[p]]++;
	for (unsigned v = 0; v < 256; ++v)
		assert(histogram[v] == 16);
	/* The mask is a stored deterministic threshold field. There must not
	 * be a runtime RNG or re-sampling, and both endpoints must be present. */
	printf("VOID_CLUSTER_R8_HISTOGRAM_PASS=256x16\n");
	return 0;
}
