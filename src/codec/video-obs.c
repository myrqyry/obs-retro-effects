#include "video-obs.h"
#include "video-roundtrip.h"
#include "codec-worker.h"

#include "../obs-utils.h"

#include <math.h>
#include <string.h>

struct retro_video_obs {
	struct codec_worker *worker;
	gs_texrender_t *framed;
	gs_effect_t *mix;
	bool staged, framing_known, full_frame;
	enum retro_video_codec codec_type;
	gs_texrender_t *capture;
	gs_stagesurf_t *stage;
	gs_texture_t *decoded_texture;
	uint8_t *output_pixels;
	struct retro_video_options options;
	int width, height;
	bool decoded;
};

static bool render_scaled(gs_texture_t *source, gs_texrender_t *target, uint32_t target_w, uint32_t target_h,
			  bool crop_for_encoder, bool full_frame)
{
	if(!source)return false;
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
	if(full_frame){draw_w=(float)target_w;draw_h=(float)target_h;}
	const float x = ((float)target_w - draw_w) * 0.5f;
	const float y = ((float)target_h - draw_h) * 0.5f;

	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	if (!image || !source)
		return false;

	set_render_parameters();
	set_blending_parameters();
	gs_effect_set_texture(image, source);
	bool ok=gs_texrender_begin(target, target_w, target_h);
	if (ok) {
		struct vec4 black = {0.f, 0.f, 0.f, 1.f};
		gs_clear(GS_CLEAR_COLOR, &black, 1.0f, 0);
		gs_projection_push();
		gs_ortho(0.0f, (float)target_w, 0.0f, (float)target_h, -100.0f, 100.0f);
		gs_matrix_push();
		gs_matrix_translate3f(x, y, 0.0f);
		while (gs_effect_loop(effect, "Draw"))
			gs_draw_sprite(source, 0, (uint32_t)draw_w, (uint32_t)draw_h);
		gs_matrix_pop();
		gs_projection_pop();
		gs_texrender_end(target);
	}
	gs_blend_state_pop();
	return ok;
}

struct retro_video_obs *retro_video_obs_create(const struct retro_video_options *options)
{
	if (!options || !retro_video_supported_size(options->codec, options->width, options->height))
		return NULL;

	struct retro_video_obs *state = bzalloc(sizeof(*state));
	if (!state)
		return NULL;
	state->codec_type = options->codec;
	state->width = options->width;
	state->height = options->height;
	state->options = *options;
	state->worker = codec_worker_create(options);
	if (!state->worker)
		goto error;

	size_t bytes = (size_t)options->width * options->height * 4;
	state->output_pixels = bzalloc(bytes);
	state->capture = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	state->framed = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	state->stage = gs_stagesurface_create(options->width, options->height, GS_RGBA);
	state->decoded_texture = gs_texture_create(options->width, options->height, GS_RGBA, 1, NULL, GS_DYNAMIC);
	if (!state->framed || !state->output_pixels || !state->capture || !state->stage ||
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
	struct codec_worker_stats stats=codec_worker_stats(state->worker);
	if(stats.submitted)
		blog(LOG_INFO,"[retro-codec] %dx%d submitted=%llu completed=%llu dropped=%llu errors=%llu recovered=%llu",
		     state->width,state->height,(unsigned long long)stats.submitted,(unsigned long long)stats.completed,
		     (unsigned long long)stats.dropped,(unsigned long long)stats.errors,(unsigned long long)stats.recovered);
	codec_worker_destroy(state->worker);
	obs_enter_graphics();
	if(state->framed)gs_texrender_destroy(state->framed);
	if(state->mix)gs_effect_destroy(state->mix);
	if (state->decoded_texture)
		gs_texture_destroy(state->decoded_texture);
	if (state->stage)
		gs_stagesurface_destroy(state->stage);
	if (state->capture)
		gs_texrender_destroy(state->capture);
	obs_leave_graphics();
	bfree(state->output_pixels);
	bfree(state);
}

bool retro_video_obs_matches(const struct retro_video_obs *state, const struct retro_video_options *options)
{
	if (!state || !options)
		return false;
	const struct retro_video_options *a = &state->options;
	return a->codec == options->codec && a->width == options->width && a->height == options->height &&
	       a->fps == options->fps && a->bitrate == options->bitrate && a->gop_size == options->gop_size &&
	       a->cinepak_codebook_passes == options->cinepak_codebook_passes &&
	       a->svq1_motion_estimation == options->svq1_motion_estimation &&
	       a->mjpeg_quantizer == options->mjpeg_quantizer;
}

/* Retro Effects draws straight-alpha intermediate textures. Its existing
 * final output path handles the same representation as the other effects. */
static const char *mix_shader=
	"uniform float4x4 ViewProj; uniform texture2d image; uniform texture2d dry; "
	"uniform float amount; uniform float preserve; "
	"sampler_state samp {Filter=Linear; AddressU=Clamp; AddressV=Clamp;}; "
	"struct Vertex {float4 pos:POSITION; float2 uv:TEXCOORD0;}; "
	"Vertex vs(Vertex v) {v.pos=mul(float4(v.pos.xyz,1.0),ViewProj);return v;} "
	"float4 ps(Vertex v):TARGET {float4 d=dry.Sample(samp,v.uv);float4 w=image.Sample(samp,v.uv); "
	"float a=lerp(lerp(d.a,1.0,amount),d.a,preserve);return float4(lerp(d.rgb,w.rgb,amount),a);} "
	"technique Draw {pass {vertex_shader=vs(v);pixel_shader=ps(v);}}";

static bool composite(struct retro_video_obs *s,gs_texture_t *dry,gs_texture_t *wet,
		      gs_texrender_t *out,uint32_t width,uint32_t height,int amount,bool alpha)
{
	if(!wet || !amount)return render_scaled(dry,out,width,height,false,true);
	if(!s->mix){
		char *error=NULL;
		s->mix=gs_effect_create(mix_shader,"retro-codec-mix",&error);
		if(error){blog(LOG_WARNING,"[retro-codec] mix shader: %s",error);bfree(error);}
	}
	if(!s->mix)return render_scaled(dry,out,width,height,false,true);
	gs_effect_t *e=s->mix;
	gs_effect_set_texture(gs_effect_get_param_by_name(e,"image"),wet);
	gs_effect_set_texture(gs_effect_get_param_by_name(e,"dry"),dry);
	gs_effect_set_float(gs_effect_get_param_by_name(e,"amount"),(float)amount/100.0f);
	gs_effect_set_float(gs_effect_get_param_by_name(e,"preserve"),alpha?1.0f:0.0f);
	set_render_parameters();set_blending_parameters();
	bool ok=gs_texrender_begin(out,width,height);
	if(ok){
		gs_projection_push();gs_ortho(0,(float)width,0,(float)height,-100,100);
		while(gs_effect_loop(e,"Draw"))gs_draw_sprite(dry,0,width,height);
		gs_projection_pop();gs_texrender_end(out);
	}
	gs_blend_state_pop();return ok;
}

bool retro_video_obs_render(struct retro_video_obs *s, gs_texrender_t *input, gs_texrender_t *output,
			    uint32_t width, uint32_t height, bool encode_frame,bool full_frame,int wet,bool alpha)
{
	if(!s||!input||!output||!width||!height)return false;
	gs_texture_t *dry=gs_texrender_get_texture(input);
	if(!dry)return false;
	/* A framing change starts a fresh codec generation: never display an
	 * old cropped frame as if it were encoded at the new framing. */
	if(!s->framing_known||s->full_frame!=full_frame){
		if(s->framing_known){
			codec_worker_destroy(s->worker);
			s->worker=codec_worker_create(&s->options);
		}
		s->framing_known=true;s->full_frame=full_frame;
		s->staged=false;s->decoded=false;encode_frame=true;
	}
	if(codec_worker_faulted(s->worker))s->decoded=false;
	if(codec_worker_take(s->worker,s->output_pixels)){
		gs_texture_set_image(s->decoded_texture,s->output_pixels,s->width*4,false);
		s->decoded=true;
	}
	if(s->staged){
		uint8_t *mapped=NULL;uint32_t stride=0;
		if(gs_stagesurface_map(s->stage,&mapped,&stride)){
			codec_worker_submit(s->worker,mapped,(int)stride);
			gs_stagesurface_unmap(s->stage);
		}
		s->staged=false;
	}
	if(encode_frame){
		gs_texrender_reset(s->capture);
		if(render_scaled(dry,s->capture,s->width,s->height,true,full_frame)){
			gs_texture_t *capture=gs_texrender_get_texture(s->capture);
			if(capture){gs_stage_texture(s->stage,capture);s->staged=true;}
		}
	}
	gs_texture_t *processed=s->decoded?s->decoded_texture:NULL;
	if(processed&&!full_frame){
		gs_texrender_reset(s->framed);
		if(render_scaled(processed,s->framed,width,height,false,false))
			processed=gs_texrender_get_texture(s->framed);
		else processed=NULL;
	}
	gs_texrender_reset(output);
	return composite(s,dry,processed,output,width,height,wet,alpha);
}
