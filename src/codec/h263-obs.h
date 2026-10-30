#pragma once

#include <obs-module.h>
#include <stdbool.h>
#include <stdint.h>

struct retro_h263_obs;

struct retro_h263_obs *retro_h263_obs_create(int width, int height, int fps, int bitrate, int gop);
void retro_h263_obs_destroy(struct retro_h263_obs *state);
bool retro_h263_obs_matches(const struct retro_h263_obs *state, int width, int height, int fps, int bitrate, int gop);

/* Input contains the current source pixels; output is the OBS-size frame.
   This uses real H.263 encoding and decoding; frames are held at 'fps'.
   Returns false until a decoded frame is available.
 */
bool retro_h263_obs_render(struct retro_h263_obs *state, gs_texrender_t *input, gs_texrender_t *output,
			   uint32_t output_width, uint32_t output_height, bool encode_frame);
