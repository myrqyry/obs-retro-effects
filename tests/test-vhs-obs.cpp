#include <obs.h>
#include <obs-module.h>
#include <graphics/graphics.h>
#include <chrono>
#include <thread>
#include <vector>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>

static constexpr int W = 320, H = 240;
struct Input {
	gs_texture_t *texture = nullptr;
};
static Input *active_input = nullptr;

static const char *input_name(void *)
{
	return "Dither gradient source";
}
static uint32_t input_width(void *)
{
	return W;
}
static uint32_t input_height(void *)
{
	return H;
}
static void *input_create(obs_data_t *, obs_source_t *)
{
	auto *in = new Input;
	active_input = in;
	std::vector<uint8_t> rgba(W * H * 4);
	for (int y = 0; y < H; ++y)
		for (int x = 0; x < W; ++x) {
			int i = (y * W + x) * 4;
			// Mix a smooth ramp with a deterministic high-frequency texture to distinguish masks.
			uint8_t lum = uint8_t(25 + (x * 37 + y * 61 + x * y * 3) % 205);
			rgba[i] = lum;
			rgba[i + 1] = lum;
			rgba[i + 2] = lum;
			rgba[i + 3] = 255;
		}
	obs_enter_graphics();
	in->texture = gs_texture_create(W, H, GS_RGBA, 1, nullptr, GS_DYNAMIC);
	if (in->texture)
		gs_texture_set_image(in->texture, rgba.data(), W * 4, false);
	obs_leave_graphics();
	return in;
}
static void input_destroy(void *data)
{
	auto *in = (Input *)data;
	obs_enter_graphics();
	if (in->texture)
		gs_texture_destroy(in->texture);
	obs_leave_graphics();
	delete in;
}
static void input_render(void *data, gs_effect_t *)
{
	auto *in = (Input *)data;
	if (in->texture)
		obs_source_draw(in->texture, 0, 0, W, H, false);
}
static obs_source_info input_info = {.id = "dither_test_input",
				     .type = OBS_SOURCE_TYPE_INPUT,
				     .output_flags = OBS_SOURCE_VIDEO,
				     .get_name = input_name,
				     .create = input_create,
				     .destroy = input_destroy,
				     .get_width = input_width,
				     .get_height = input_height,
				     .video_render = input_render};
static bool render_pixels(obs_source_t *source, std::vector<uint8_t> &output)
{
	obs_enter_graphics();
	auto *target = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	auto *stage = gs_stagesurface_create(W, H, GS_RGBA);
	bool ok = target && stage && gs_texrender_begin_with_color_space(target, W, H, GS_CS_SRGB);
	if (ok) {
		vec4 clear{};
		gs_clear(GS_CLEAR_COLOR, &clear, 1, 0);
		gs_projection_push();
		gs_ortho(0, W, 0, H, -100, 100);
		obs_source_video_render(source);
		gs_projection_pop();
		gs_texrender_end(target);
		gs_stage_texture(stage, gs_texrender_get_texture(target));
		uint8_t *data = nullptr;
		uint32_t pitch = 0;
		ok = gs_stagesurface_map(stage, &data, &pitch);
		if (ok) {
			output.resize(W * H * 4);
			for (int y = 0; y < H; ++y)
				memcpy(output.data() + y * W * 4, data + y * pitch, W * 4);
			gs_stagesurface_unmap(stage);
		}
	}
	if (stage)
		gs_stagesurface_destroy(stage);
	if (target)
		gs_texrender_destroy(target);
	obs_leave_graphics();
	return ok;
}

int main(int argc, char **argv)
{
	if (argc != 3)
		return 2;
	setbuf(stdout, nullptr);
	if (!obs_startup("en-US", nullptr, nullptr))
		return 3;
	obs_video_info info{};
	info.graphics_module = "libobs-opengl";
	info.fps_num = 30;
	info.fps_den = 1;
	info.base_width = W;
	info.base_height = H;
	info.output_width = W;
	info.output_height = H;
	info.output_format = VIDEO_FORMAT_RGBA;
	info.scale_type = OBS_SCALE_DISABLE;
	if (obs_reset_video(&info) != OBS_VIDEO_SUCCESS)
		return 4;
	obs_register_source(&input_info);
	obs_module_t *module = nullptr;
	if (obs_open_module(&module, argv[1], argv[2]) != MODULE_SUCCESS || !obs_init_module(module))
		return 5;
	auto *source = obs_source_create("dither_test_input", "vhs-osd-test-src", nullptr, nullptr);
	auto *settings = obs_data_create();
	obs_data_set_int(settings, "filter_type", 11);
	obs_data_set_int(settings, "vhs_osd_mode", 0);
	// Isolate overlay integration by disabling the upstream analogue motion.
	for (const char *key : {"vhs_wrinkle_occurrence", "vhs_wrinkle_size", "vhs_pop_lines_amount",
				"vhs_head_switch_primary_thickness", "vhs_head_switch_primary_offset",
				"vhs_head_switch_secondary_thickness", "vhs_head_switch_secondary_horiz_offset",
				"vhs_head_switch_secondary_vert_amount", "vhs_jitter_min_size", "vhs_jitter_max_size"})
		obs_data_set_double(settings, key, 0.0);
	auto *filter = obs_source_create("obs_retro_effects_filter", "vhs-osd-test-filter", settings, nullptr);
	obs_data_release(settings);
	if (!source || !filter)
		return 6;
	obs_source_filter_add(source, filter);
	obs_set_output_source(0, source);
	std::this_thread::sleep_for(std::chrono::milliseconds(200));

	auto *properties = obs_source_properties(filter);
	auto *osd_mode = properties ? obs_properties_get(properties, "vhs_osd_mode") : nullptr;
	auto *osd_style = properties ? obs_properties_get(properties, "vhs_osd_style") : nullptr;
	auto *osd_transport = properties ? obs_properties_get(properties, "vhs_osd_transport") : nullptr;
	bool choices = osd_mode && osd_style && osd_transport && obs_property_list_item_count(osd_mode) == 4 &&
		       obs_property_list_item_count(osd_style) == 2 && obs_property_list_item_count(osd_transport) == 6;
	printf("VHS_OSD_UI_VISIBLE=%d\n", int(choices));
	if (properties)
		obs_properties_destroy(properties);
	if (!choices)
		return 7;

	auto render = [&](int mode, int style, int transport, std::vector<uint8_t> &pixels) {
		auto *value = obs_source_get_settings(filter);
		obs_data_set_int(value, "vhs_osd_mode", mode);
		obs_data_set_int(value, "vhs_osd_style", style);
		obs_data_set_int(value, "vhs_osd_transport", transport);
		obs_source_update(filter, value);
		obs_data_release(value);
		std::this_thread::sleep_for(std::chrono::milliseconds(160));
		return render_pixels(source, pixels);
	};
	std::vector<uint8_t> off, vcr, cam, disabled_again;
	if (!render(0, 0, 1, off) || !render(2, 0, 1, vcr) || !render(2, 1, 5, cam) || !render(0, 0, 1, disabled_again))
		return 8;
	size_t vcr_top = 0, vcr_bottom = 0, vcr_mid = 0, cam_top = 0, cam_bottom = 0;
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++) {
			size_t index = ((size_t)y * W + x) * 4;
			int vdiff = abs((int)vcr[index] - (int)off[index]);
			int cdiff = abs((int)cam[index] - (int)off[index]);
			if (vdiff > 30) {
				if (y < 60)
					vcr_top++;
				else if (y > 185)
					vcr_bottom++;
				else
					vcr_mid++;
			}
			if (cdiff > 30) {
				if (y < 60)
					cam_top++;
				else if (y > 185)
					cam_bottom++;
			}
		}
	bool success = vcr_top > 20 && vcr_bottom > 15 && vcr_mid == 0 && cam_top > 20 && cam_bottom > 15 &&
		       vcr != cam && off == disabled_again;
	printf("VHS_OSD_GPU_VCR_TOP=%zu BOTTOM=%zu MID=%zu CAM_TOP=%zu BOTTOM=%zu OFF_RESTORED=%d PASS=%d\n", vcr_top,
	       vcr_bottom, vcr_mid, cam_top, cam_bottom, int(off == disabled_again), int(success));
	// OSD characters are generated by the VHS hardware, not inherited
	// from the footage alpha. They should remain visible over transparent video.
	std::vector<uint8_t> transparent(W * H * 4, 0);
	obs_enter_graphics();
	gs_texture_set_image(active_input->texture, transparent.data(), W * 4, false);
	obs_leave_graphics();
	std::vector<uint8_t> clear_off, clear_on;
	if (!render(0, 0, 1, clear_off) || !render(2, 0, 1, clear_on))
		return 10;
	size_t visible_osd_alpha = 0, unchanged_middle = 0;
	for (int y = 0; y < H; ++y)
		for (int x = 0; x < W; ++x) {
			size_t i = ((size_t)y * W + x) * 4;
			if (clear_on[i + 3] > clear_off[i + 3] + 40)
				++visible_osd_alpha;
			if (y >= 60 && y <= 185 && clear_on[i + 3] != clear_off[i + 3])
				++unchanged_middle;
		}
	printf("VHS_OSD_TRANSPARENT_SOURCE_OPAQUE_PIXELS=%zu MID_ALPHA_CHANGES=%zu\n", visible_osd_alpha,
	       unchanged_middle);
	success = success && visible_osd_alpha > 30 && unchanged_middle == 0;
	obs_set_output_source(0, nullptr);
	obs_source_filter_remove(source, filter);
	obs_source_release(filter);
	obs_source_release(source);
	obs_shutdown();
	return success ? 0 : 9;
}
