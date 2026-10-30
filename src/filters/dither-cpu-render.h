#pragma once
#include "dither.h"
#include "../base-filter.h"

bool dither_cpu_render(dither_filter_data_t *filter, base_filter_data_t *base,
		       gs_texture_t *source, gs_effect_t *effect);
void dither_cpu_destroy(dither_filter_data_t *filter);
