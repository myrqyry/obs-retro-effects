#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* A real baseline H.263 encoder -> decoder, driven one frame at a time.
   No files, muxer, synthetic blocks, or simulated compression artifacts. */
struct retro_h263;

struct retro_h263_options {
	int width;
	int height;
	int fps;
	int bitrate;  /* bits/second */
	int gop_size; /* frames between I-frames */
};

bool retro_h263_supported_size(int width, int height);
struct retro_h263 *retro_h263_create(const struct retro_h263_options *options);
void retro_h263_destroy(struct retro_h263 *state);

/* RGBA, top-to-bottom; strides >= width*4. Returns:
   1 = decoded picture written; 0 = codec buffering; -1 = error.
   Submit frames in temporal order at the configured frame rate.
 */
int retro_h263_process(struct retro_h263 *state, const uint8_t *rgba, int rgba_stride, uint8_t *decoded,
		       int decoded_stride);
uint64_t retro_h263_encoded_bytes(const struct retro_h263 *state);
uint64_t retro_h263_encoded_packets(const struct retro_h263 *state);
