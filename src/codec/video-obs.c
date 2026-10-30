#include "video-obs.h"
#include "video-roundtrip.h"

#include "../obs-utils.h"

#include <math.h>
#include <string.h>

struct retro_video_obs {
	struct retro_video *codec;
	enum retro_video_codec codec_type;
	gs_texrender_t *capture;
	gs_stagesurf_t *stage;
	gs_texture_t *decoded_texture;
	uint8_t *input_pixels;
	uint8_t *output_pixels;
	int width, height, fps, bitrate, gop;
	bool decoded;
};

static void render_scaled(gs_texture_t *source, gs_texrender_t *target, uint32_t target_w, uint32_t target_h,
			  bool crop_for_encoder)
{
	/* QCIF (176x144) is displayed as 4:3 with a 12:11 sample aspect.
	   Encode a centered 4:3 crop, not a stretched 16:9 image. When displaying
	   the result, letterbox/pillarbox it back into OBS's source canvas. */
	const float native_aspect =
		crop_for_encoder ? (float)gs_texture_get_width(source) / gs_texture_get_height(source) : 4.0f / 3.0f;
	const float target_aspect = crop_for_encoder ? 4.0f / 3.0f : (float)target_w / target_h;
	const float scale = crop_for_encoder ? fmaxf(target_aspect / native_aspect, 1.0f)
					     : fminf(target_aspect / native_aspect, 1.0f);

	float draw_w, draw_h;
	if (crop_for_encoder) {
		/* Scale the incoming image to cover a 4:3 capture, in display units,
		   then map to the QCIF sample lattice (PAR 12:11). */
		draw_w = (float)target_w * (native_aspect / target_aspect) * scale;
		draw_h = (float)target_h * scale;
	} else {
		draw_w = (float)target_h * 4.0f / 3.0f;
		draw_h = (float)target_h;
		if (draw_w > target_w) {
			draw_w = (float)target_w;
			draw_h = (float)target_w * 3.0f / 4.0f;
		}
	}
	const float x = ((float)target_w - draw_w) * 0.5f;
	const float y = ((float)target_h - draw_h) * 0.5f;

	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	if (!image)
		return;

	set_render_parameters();
	set_blending_parameters();
	gs_effect_set_texture(image, source);
	if (gs_texrender_begin(target, target_w, target_h)) {
		struct vec4 black = {0.f, 0.f, 0.f, 1.f};
		gs_clear(GS_CLEAR_COLOR, &black, 1.0f, 0);
		gs_ortho(0.0f, (float)target_w, 0.0f, (float)target_h, -100.0f, 100.0f);
		gs_matrix_push();
		gs_matrix_translate3f(x, y, 0.0f);
		while (gs_effect_loop(effect, "Draw"))
			gs_draw_sprite(source, 0, (uint32_t)draw_w, (uint32_t)draw_h);
		gs_matrix_pop();
		gs_texrender_end(target);
	}
	gs_blend_state_pop();
}

struct retro_video_obs *retro_video_obs_create(enum retro_video_codec codec, int width, int height, int fps,
					       int bitrate, int gop)
{
	if (!retro_video_supported_size(codec, width, height))
		return NULL;

	struct retro_video_obs *state = bzalloc(sizeof(*state));
	state->codec_type = codec;
	state->width = width;
	state->height = height;
	state->fps = fps;
	state->bitrate = bitrate;
	state->gop = gop;
	state->codec = retro_video_create(&(struct retro_video_options){.codec = codec,
									.width = width,
									.height = height,
									.fps = fps,
									.bitrate = bitrate,
									.gop_size = gop});
	if (!state->codec)
		goto error;

	size_t bytes = (size_t)width * height * 4;
	state->input_pixels = bzalloc(bytes);
	state->output_pixels = bzalloc(bytes);
	state->capture = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	state->stage = gs_stagesurface_create(width, height, GS_RGBA);
	state->decoded_texture = gs_texture_create(width, height, GS_RGBA, 1, NULL, GS_DYNAMIC);
	if (!state->input_pixels || !state->output_pixels || !state->capture || !state->stage ||
	    !state->decoded_texture)
		goto error;
	return state;
error:
	retro_video_obs_destroy(state);
	return NULL;
}

void retro_video_obs_destroy(struct retro_video_obs *state)
{
	if (!state)
		return;
	retro_video_destroy(state->codec);
	obs_enter_graphics();
	if (state->decoded_texture)
		gs_texture_destroy(state->decoded_texture);
	if (state->stage)
		gs_stagesurface_destroy(state->stage);
	if (state->capture)
		gs_texrender_destroy(state->capture);
	obs_leave_graphics();
	bfree(state->input_pixels);
	bfree(state->output_pixels);
	bfree(state);
}

bool retro_video_obs_matches(const struct retro_video_obs *state, enum retro_video_codec codec, int width, int height,
			     int fps, int bitrate, int gop)
{
	return state && state->codec_type == codec && state->width == width && state->height == height &&
	       state->fps == fps && state->bitrate == bitrate && state->gop == gop;
}

bool retro_video_obs_render(struct retro_video_obs *state, gs_texrender_t *input, gs_texrender_t *output,
			    uint32_t output_width, uint32_t output_height, bool encode_frame)
{
	if (!state || !input || !output || !output_width || !output_height)
		return false;

	if (encode_frame || !state->decoded) {
		gs_texture_t *source = gs_texrender_get_texture(input);
		if (!source)
			return false;
		gs_texrender_reset(state->capture);
		render_scaled(source, state->capture, state->width, state->height, true);
		gs_texture_t *captured = gs_texrender_get_texture(state->capture);
		if (!captured)
			return false;
		gs_stage_texture(state->stage, captured);
		uint8_t *mapped = NULL;
		uint32_t linesize = 0;
		if (!gs_stagesurface_map(state->stage, &mapped, &linesize))
			return false;
		for (int y = 0; y < state->height; ++y)
			memcpy(state->input_pixels + (size_t)y * state->width * 4, mapped + (size_t)y * linesize,
			       (size_t)state->width * 4);
		gs_stagesurface_unmap(state->stage);
		int result = retro_video_process(state->codec, state->input_pixels, state->width * 4,
						 state->output_pixels, state->width * 4);
		if (result < 0)
			return false;
		if (result == 1) {
			gs_texture_set_image(state->decoded_texture, state->output_pixels, state->width * 4, false);
			state->decoded = true;
		}
	}
	if (!state->decoded)
		return false;
	gs_texrender_reset(output);
	render_scaled(state->decoded_texture, output, output_width, output_height, false);
	return true;
}
