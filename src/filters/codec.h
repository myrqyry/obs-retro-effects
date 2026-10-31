#pragma once

#include <obs-module.h>
#include "../obs-retro-effects.h"

#define CODEC_TYPE_RPZA 1
#define CODEC_TYPE_RPZA_LABEL "RetroEffects.Codec.RPZA"
#ifdef ENABLE_VIDEO_CODECS
#define CODEC_TYPE_H263 2
#define CODEC_TYPE_H263_LABEL "RetroEffects.Codec.H263"
#define CODEC_TYPE_MPEG4_PART2 3
#define CODEC_TYPE_MPEG4_PART2_LABEL "RetroEffects.Codec.MPEG4Part2"
#define CODEC_TYPE_CINEPAK 4
#define CODEC_TYPE_CINEPAK_LABEL "RetroEffects.Codec.Cinepak"
#define CODEC_TYPE_SVQ1 5
#define CODEC_TYPE_SVQ1_LABEL "RetroEffects.Codec.SVQ1"
#define CODEC_TYPE_MSVIDEO1 6
#define CODEC_TYPE_MSVIDEO1_LABEL "RetroEffects.Codec.MSVideo1"
#define CODEC_TYPE_FLV1 7
#define CODEC_TYPE_FLV1_LABEL "RetroEffects.Codec.FLV1"
#define CODEC_TYPE_MPEG1 8
#define CODEC_TYPE_MPEG1_LABEL "RetroEffects.Codec.MPEG1"
#define CODEC_TYPE_MJPEG 9
#define CODEC_TYPE_MJPEG_LABEL "RetroEffects.Codec.MJPEG"
struct retro_video_obs;
#endif

struct codec_filter_data;
typedef struct codec_filter_data codec_filter_data_t;

struct codec_filter_data {
	gs_effect_t *effect_codec;

	// Shader params

	gs_eparam_t *param_image;
	gs_eparam_t *param_uv_size;
	gs_eparam_t *param_prev_frame;
	gs_eparam_t *param_prev_frame_valid;
	gs_eparam_t *param_is_keyframe;

	gs_eparam_t *param_colors_per_channel;

	gs_eparam_t *param_rpza_threshold_prev_frame;
	gs_eparam_t *param_rpza_threshold_solid;
	gs_eparam_t *param_rpza_threshold_gradient;

	// Render textures

	gs_texrender_t *texrender_downsampled_input;
	gs_texrender_t *texrender_previous_frame;
	gs_texrender_t *texrender_downsampled_output;

	// Settings

	uint32_t codec_type;

	float px_scale;
	int colors_per_channel;

	float quality;
	float keyframe_interval;
	bool custom_thresholds;

	float rpza_threshold_prev_frame;
	float rpza_threshold_solid;
	float rpza_threshold_gradient;

	// Internal

	bool loading_effect;
	bool reload_effect;

	float time_since_keyframe;
#ifdef ENABLE_VIDEO_CODECS
	struct retro_video_obs *video;
	int video_fps, video_bitrate, video_gop;
	int video_cinepak_passes, video_svq1_motion, video_mjpeg_quantizer;
	float video_elapsed;
	bool video_due;
#endif
};

extern void codec_create(retro_effects_filter_data_t *filter);
extern void codec_destroy(retro_effects_filter_data_t *filter);
extern void codec_unset_settings(retro_effects_filter_data_t *filter);

extern void codec_filter_video_tick(retro_effects_filter_data_t *data, float seconds);
extern void codec_filter_video_render(retro_effects_filter_data_t *data);
extern void codec_filter_properties(retro_effects_filter_data_t *data, obs_properties_t *props);
extern void codec_filter_defaults(obs_data_t *settings);
extern void codec_filter_update(retro_effects_filter_data_t *data, obs_data_t *settings);
static void codec_set_functions(retro_effects_filter_data_t *filter);
static void codec_load_effect(codec_filter_data_t *filter);

static bool codec_type_modified(obs_properties_t *props, obs_property_t *p, obs_data_t *settings);
