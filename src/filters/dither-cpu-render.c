#include "dither-cpu-render.h"
#include "dither-diffusion.h"
#include "../obs-utils.h"

#include <math.h>
#include <string.h>

/* Must be invoked with the OBS graphics context active. */
void dither_cpu_destroy(dither_filter_data_t *filter)
{
	if (filter->diffusion_prepass)
		gs_texrender_destroy(filter->diffusion_prepass);
	if (filter->diffusion_stage)
		gs_stagesurface_destroy(filter->diffusion_stage);
	if (filter->diffusion_texture)
		gs_texture_destroy(filter->diffusion_texture);
	bfree(filter->diffusion_source);
	bfree(filter->diffusion_output);
	filter->diffusion_prepass = NULL;
	filter->diffusion_stage = NULL;
	filter->diffusion_texture = NULL;
	filter->diffusion_source = NULL;
	filter->diffusion_output = NULL;
	filter->diffusion_width = filter->diffusion_height = 0;
}

static bool ensure_target(dither_filter_data_t *filter, int w, int h)
{
	if (filter->diffusion_width == w && filter->diffusion_height == h && filter->diffusion_prepass &&
	    filter->diffusion_stage && filter->diffusion_texture && filter->diffusion_source &&
	    filter->diffusion_output)
		return true;
	dither_cpu_destroy(filter);
	size_t bytes = (size_t)w * h * 4;
	filter->diffusion_prepass = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	filter->diffusion_stage = gs_stagesurface_create(w, h, GS_RGBA);
	filter->diffusion_texture = gs_texture_create(w, h, GS_RGBA, 1, NULL, GS_DYNAMIC);
	filter->diffusion_source = bmalloc(bytes);
	filter->diffusion_output = bmalloc(bytes);
	if (!filter->diffusion_prepass || !filter->diffusion_stage || !filter->diffusion_texture ||
	    !filter->diffusion_source || !filter->diffusion_output) {
		dither_cpu_destroy(filter);
		return false;
	}
	filter->diffusion_width = w;
	filter->diffusion_height = h;
	return true;
}

bool dither_cpu_render(dither_filter_data_t *filter, base_filter_data_t *base, gs_texture_t *source,
		       gs_effect_t *effect)
{
	if (!source || !effect || !base->width || !base->height || !filter->param_processed_image)
		return false;

	/* Explicit 1024 logical pixel cap prevents runaway per-frame CPU work at
	 * 4K. Both axes are scaled by the same effective pixel size. */
	float effective = fmaxf(fmaxf(filter->dither_size, 1.f),
				fmaxf((float)base->width / 1024.f, (float)base->height / 1024.f));
	int w = (int)ceilf((float)base->width / effective);
	int h = (int)ceilf((float)base->height / effective);
	if (w < 1 || h < 1 || w > 1024 || h > 1024 || !ensure_target(filter, w, h))
		return false;

	if (filter->param_diffusion_prepass)
		gs_effect_set_float(filter->param_diffusion_prepass, 1.f);
	gs_texrender_reset(filter->diffusion_prepass);
	set_render_parameters();
	set_blending_parameters();
	bool began = gs_texrender_begin(filter->diffusion_prepass, w, h);
	if (began) {
		struct vec4 transparent = {0.f, 0.f, 0.f, 0.f};
		gs_clear(GS_CLEAR_COLOR, &transparent, 1.f, 0);
		gs_ortho(0.f, (float)w, 0.f, (float)h, -100.f, 100.f);
		while (gs_effect_loop(effect, filter->monochromatic ? "DrawMonoSource" : "DrawColorSource"))
			gs_draw_sprite(source, 0, w, h);
		gs_texrender_end(filter->diffusion_prepass);
	}
	gs_blend_state_pop();
	if (filter->param_diffusion_prepass)
		gs_effect_set_float(filter->param_diffusion_prepass, 0.f);
	if (!began)
		return false;

	gs_texture_t *corrected = gs_texrender_get_texture(filter->diffusion_prepass);
	if (!corrected)
		return false;
	gs_stage_texture(filter->diffusion_stage, corrected);
	uint8_t *mapped = NULL;
	uint32_t stride = 0;
	if (!gs_stagesurface_map(filter->diffusion_stage, &mapped, &stride))
		return false;
	for (int y = 0; y < h; ++y)
		memcpy(filter->diffusion_source + (size_t)y * w * 4, mapped + (size_t)y * stride, (size_t)w * 4);
	gs_stagesurface_unmap(filter->diffusion_stage);
	if (!retro_dither_diffuse(filter->diffusion_source, w, h, w * 4, filter->diffusion_output, w * 4,
				  filter->color_steps,
				  (enum retro_dither_diffusion_method)(filter->dither_type - DITHER_TYPE_FLOYD + 1)))
		return false;

	gs_texture_set_image(filter->diffusion_texture, filter->diffusion_output, w * 4, false);
	gs_effect_set_texture(filter->param_processed_image, filter->diffusion_texture);
	set_render_parameters();
	set_blending_parameters();
	if (!gs_texrender_begin(base->output_texrender, base->width, base->height)) {
		gs_blend_state_pop();
		return false;
	}
	gs_ortho(0.f, (float)base->width, 0.f, (float)base->height, -100.f, 100.f);
	while (gs_effect_loop(effect, "DrawDiffused"))
		gs_draw_sprite(filter->diffusion_texture, 0, base->width, base->height);
	gs_texrender_end(base->output_texrender);
	gs_blend_state_pop();
	return true;
}
