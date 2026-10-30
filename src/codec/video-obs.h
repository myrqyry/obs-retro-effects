#pragma once

#include "video-roundtrip.h"

#include <obs-module.h>
#include <stdbool.h>
#include <stdint.h>

struct retro_video_obs;

struct retro_video_obs *retro_video_obs_create(enum retro_video_codec codec, int width, int height, int fps,
					       int bitrate, int gop);
void retro_video_obs_destroy(struct retro_video_obs *state);
bool retro_video_obs_matches(const struct retro_video_obs *state, enum retro_video_codec codec, int width, int height,
			     int fps, int bitrate, int gop);

/* Real video encode/decode. The output is the source-canvas-sized frame,
 * preserving the QCIF display aspect. Holds the last decoded frame until due.
 * false = unavailable/errored; caller may bypass.
 */
bool retro_video_obs_render(struct retro_video_obs *state, gs_texrender_t *input, gs_texrender_t *output,
			    uint32_t output_width, uint32_t output_height, bool encode_frame);
