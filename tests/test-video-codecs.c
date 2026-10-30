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
	/* Real distinct encoder families, not parameter aliases of MPEG-4. */
	const struct {
		enum retro_video_codec id;
		int width, height, fps, bitrate, gop;
	} gallery[] = {
		{RETRO_VIDEO_CINEPAK, 320, 240, 15, 300000, 15},  {RETRO_VIDEO_SVQ1, 320, 240, 15, 250000, 15},
		{RETRO_VIDEO_MSVIDEO1, 160, 120, 15, 180000, 15}, {RETRO_VIDEO_FLV1, 320, 240, 15, 200000, 15},
		{RETRO_VIDEO_MPEG1, 352, 288, 25, 1150000, 15},   {RETRO_VIDEO_MJPEG, 320, 240, 15, 1500000, 15}};
	for (size_t c = 0; c < sizeof(gallery) / sizeof(gallery[0]); ++c) {
		const struct retro_video_options opts = {.codec = gallery[c].id,
							 .width = gallery[c].width,
							 .height = gallery[c].height,
							 .fps = gallery[c].fps,
							 .bitrate = gallery[c].bitrate,
							 .gop_size = gallery[c].gop};
		assert(retro_video_supported_size(opts.codec, opts.width, opts.height));
		struct retro_video *enc = retro_video_create(&opts);
		assert(enc != NULL);
		size_t count = (size_t)opts.width * opts.height * 4;
		uint8_t *input = malloc(count), *output = malloc(count);
		assert(input && output);
		int pictures = 0;
		unsigned long long difference = 0;
		for (int frame = 0; frame < 18; ++frame) {
			input_frame(input, opts.width, opts.height, frame);
			memset(output, 0, count);
			int result = retro_video_process(enc, input, opts.width * 4, output, opts.width * 4);
			assert(result >= 0);
			if (result == 1) {
				++pictures;
				for (size_t n = 0; n < count; n += 4) {
					assert(output[n + 3] == 255);
					difference += abs((int)input[n] - (int)output[n]);
				}
			}
		}
		printf("GALLERY_CODEC codec=%d pictures=%d packets=%llu bytes=%llu difference=%llu\n", opts.codec,
		       pictures, (unsigned long long)retro_video_encoded_packets(enc),
		       (unsigned long long)retro_video_encoded_bytes(enc), difference);
		assert(pictures >= 15);
		assert(retro_video_encoded_packets(enc) >= 15);
		assert(difference > 3000);
		retro_video_destroy(enc);
		free(output);
		free(input);
	}
	return 0;
}
