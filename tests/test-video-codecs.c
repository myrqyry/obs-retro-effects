#include "../src/codec/video-roundtrip.h"
#include <libavcodec/avcodec.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void input_frame(uint8_t *pixels, int w, int h, int frame)
{
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			const int off = (y * w + x) * 4;
			const int move = frame * 3;
			pixels[off] = (uint8_t)((x * 5 + move) % 256);
			pixels[off + 1] = (uint8_t)((y * 3 + (x / 16) * 17 + move) % 256);
			pixels[off + 2] = (uint8_t)((((x + move) / 16 + y / 16) % 2) * 200 + 25);
			pixels[off + 3] = 255;
		}
	}
}

static uint64_t verify(enum retro_video_codec codec, int bitrate, int *decoded_frames)
{
	const int w = 176, h = 144;
	const size_t bytes = (size_t)w * h * 4;
	uint8_t *src = malloc(bytes), *dst = malloc(bytes);
	assert(src && dst);
	struct retro_video_options options =
		{.codec = codec, .width = w, .height = h, .fps = 10, .bitrate = bitrate, .gop_size = 12};
	struct retro_video *video = retro_video_create(&options);
	assert(video);
	int decoded_count = 0;
	uint64_t lossy_difference = 0;
	for (int frame = 0; frame < 48; ++frame) {
		input_frame(src, w, h, frame);
		memset(dst, 0, bytes);
		const int got = retro_video_process(video, src, w * 4, dst, w * 4);
		assert(got >= 0);
		if (got != 1)
			continue;
		++decoded_count;
		for (size_t i = 0; i < bytes; i += 4) {
			assert(dst[i + 3] == 255);
			lossy_difference += abs((int)src[i] - (int)dst[i]);
		}
	}
	assert(decoded_count >= 45);
	assert(lossy_difference > 3000);
	assert(retro_video_encoded_packets(video) >= 45);
	if (codec == RETRO_VIDEO_MPEG4_PART2) {
		int profile = retro_video_decoded_profile(video);
		int sar_num = retro_video_decoded_sar_num(video);
		int sar_den = retro_video_decoded_sar_den(video);
		printf("MPEG4_BITSTREAM profile=%d simple=%d sar=%d:%d\n", profile, FF_PROFILE_MPEG4_SIMPLE, sar_num,
		       sar_den);
		assert(profile == FF_PROFILE_MPEG4_SIMPLE);
		assert(sar_num == 12 && sar_den == 11);
	}
	assert(retro_video_encoded_keyframes(video) >= 3);
	assert(retro_video_encoded_bytes(video) > 0);
	*decoded_frames = decoded_count;
	uint64_t bytes_out = retro_video_encoded_bytes(video);
	retro_video_destroy(video);
	free(dst);
	free(src);
	return bytes_out;
}

int main(void)
{
	assert(retro_video_supported_size(RETRO_VIDEO_H263, 176, 144));
	assert(!retro_video_supported_size(RETRO_VIDEO_H263, 320, 240));
	assert(retro_video_supported_size(RETRO_VIDEO_MPEG4_PART2, 176, 144));
	assert(retro_video_supported_size(RETRO_VIDEO_MPEG4_PART2, 320, 240));
	assert(!retro_video_supported_size(RETRO_VIDEO_MPEG4_PART2, 177, 145));

	int h263_frames = 0, mpeg4_frames = 0;
	const uint64_t h263_bytes = verify(RETRO_VIDEO_H263, 64000, &h263_frames);
	const uint64_t mpeg4_bytes = verify(RETRO_VIDEO_MPEG4_PART2, 64000, &mpeg4_frames);
	printf("VIDEO_CODEC_PASS h263_frames=%d mpeg4_frames=%d h263_bytes=%llu mpeg4_bytes=%llu\n", h263_frames,
	       mpeg4_frames, (unsigned long long)h263_bytes, (unsigned long long)mpeg4_bytes);
	return 0;
}
