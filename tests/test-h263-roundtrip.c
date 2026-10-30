#include "../src/codec/video-roundtrip.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fill_rgba(uint8_t *pixels, int width, int height, int frame)
{
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			int off = (y * width + x) * 4;
			pixels[off] = (uint8_t)((x * 9 + y * 2 + frame * 4) & 255);
			pixels[off + 1] = (uint8_t)(((x / 8 + frame / 3) % 4) * 60);
			pixels[off + 2] = (uint8_t)(((y / 8 + frame / 5) % 4) * 60);
			pixels[off + 3] = 255;
		}
	}
}

int main(void)
{
	assert(retro_video_supported_size(RETRO_VIDEO_H263, 128, 96));
	assert(retro_video_supported_size(RETRO_VIDEO_H263, 176, 144));
	assert(!retro_video_supported_size(RETRO_VIDEO_H263, 320, 240));
	assert(retro_video_create(&(struct retro_video_options){.codec = RETRO_VIDEO_H263,
								.width = 320,
								.height = 240,
								.fps = 10,
								.bitrate = 64000,
								.gop_size = 10}) == NULL);
	const int w = 128, h = 96;
	size_t count = (size_t)w * h * 4;
	uint8_t *in = malloc(count), *out = malloc(count);
	assert(in && out);
	struct retro_video_options low =
		{.codec = RETRO_VIDEO_H263, .width = w, .height = h, .fps = 10, .bitrate = 24000, .gop_size = 10};
	struct retro_video_options high =
		{.codec = RETRO_VIDEO_H263, .width = w, .height = h, .fps = 10, .bitrate = 160000, .gop_size = 10};
	struct retro_video *a = retro_video_create(&low);
	struct retro_video *b = retro_video_create(&high);
	assert(a && b);
	int low_frames = 0, high_frames = 0, errors = 0;
	unsigned long long last_frame_difference = 0;
	for (int f = 0; f < 45; ++f) {
		fill_rgba(in, w, h, f);
		memset(out, 0, count);
		int got = retro_video_process(a, in, w * 4, out, w * 4);
		assert(got >= 0);
		if (got == 1) {
			low_frames++;
			for (size_t i = 0; i < count; i += 4) {
				last_frame_difference += abs((int)in[i] - (int)out[i]);
				if (out[i + 3] != 255)
					errors++;
			}
		}
		got = retro_video_process(b, in, w * 4, out, w * 4);
		assert(got >= 0);
		if (got == 1)
			high_frames++;
	}
	assert(low_frames >= 40 && high_frames >= 40);
	assert(errors == 0);
	assert(last_frame_difference > 1000); /* Lossy codec, not an identity shader. */
	assert(retro_video_encoded_packets(a) >= 40);
	assert(retro_video_encoded_packets(b) >= 40);
	assert(retro_video_encoded_bytes(a) > 0 && retro_video_encoded_bytes(b) > 0);
	printf("H263_ROUNDTRIP_PASS low_frames=%d high_frames=%d low_bytes=%llu high_bytes=%llu difference=%llu\n",
	       low_frames, high_frames, (unsigned long long)retro_video_encoded_bytes(a),
	       (unsigned long long)retro_video_encoded_bytes(b), last_frame_difference);
	retro_video_destroy(a);
	retro_video_destroy(b);
	free(in);
	free(out);
	return 0;
}
