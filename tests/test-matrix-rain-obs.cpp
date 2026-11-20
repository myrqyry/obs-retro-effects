#include <obs.h>
#include <obs-module.h>
#include <graphics/graphics.h>
#include <chrono>
#include <thread>
#include <vector>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>

static constexpr int W = 192, H = 128;
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
static obs_source_info input_info = {.id = "matrix_test_input",
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
	if (argc != 3 && argc != 5)
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

	auto *source = obs_source_create("matrix_test_input", "matrix-input", nullptr, nullptr);
	auto *settings = obs_data_create();
	obs_data_set_int(settings, "filter_type", 9);
	obs_data_set_int(settings, "matrix_char_set", 1);
	obs_data_set_double(settings, "matrix_rain_scale", 0.7);
	obs_data_set_double(settings, "matrix_min_fade_value", 0.8);
	obs_data_set_double(settings, "matrix_speed_factor", 0.0); // Freeze rain between parameter comparisons.
	obs_data_set_double(settings, "matrix_bloom_intensity", 3.0);
	obs_data_set_double(settings, "matrix_bloom_radius", 7.0);
	obs_data_set_double(settings, "matrix_bloom_threshold", 0.0);
	auto *filter = obs_source_create("obs_retro_effects_filter", "matrix-filter", settings, nullptr);
	obs_data_release(settings);
	if (!source || !filter)
		return 6;
	obs_source_filter_add(source, filter);
	obs_set_output_source(0, source);

	auto update = [&](double threshold, double black, double white, bool colorize) {
		auto *choice = obs_source_get_settings(filter);
		obs_data_set_double(choice, "matrix_bloom_threshold", threshold);
		obs_data_set_double(choice, "matrix_min_brightness", black);
		obs_data_set_double(choice, "matrix_max_brightness", white);
		obs_data_set_bool(choice, "matrix_rain_colorize", colorize);
		obs_source_update(filter, choice);
		obs_data_release(choice);
		std::this_thread::sleep_for(std::chrono::milliseconds(240));
	};
	std::vector<uint8_t> low, high, equal;
	update(0.0, 0.0, 1.0, false);
	if (!render_pixels(source, low) || low.size() != size_t(W) * H * 4)
		return 7;
	update(1.0, 0.0, 1.0, false);
	if (!render_pixels(source, high))
		return 8;
	size_t changed = 0, glowing = 0;
	for (size_t i = 0; i < low.size(); i += 4) {
		changed += low[i] != high[i] || low[i + 1] != high[i + 1] || low[i + 2] != high[i + 2];
		glowing += low[i] > 20 || low[i + 1] > 20 || low[i + 2] > 20;
	}
	printf("MATRIX_BLOOM_THRESHOLD_CHANGED=%zu NONBLACK=%zu\n", changed, glowing);
	if (glowing < 50 || changed < 20)
		return 9;

	update(0.2, 0.4, 0.4, true);
	if (!render_pixels(source, equal))
		return 10;
	// The shader must remain operational at coincident black/white levels.
	size_t alpha_pixels = 0;
	for (size_t i = 3; i < equal.size(); i += 4)
		alpha_pixels += equal[i] > 0;
	printf("MATRIX_EQUAL_BRIGHTNESS_RENDER_ALPHA_PIXELS=%zu\n", alpha_pixels);
	if (!alpha_pixels)
		return 11;

	// A missing custom atlas must not null-deref; keep the prior loaded font.
	auto *invalid = obs_source_get_settings(filter);
	obs_data_set_int(invalid, "matrix_char_set", 0);
	obs_data_set_int(invalid, "matrix_rain_texture_chars", 17);
	obs_data_set_string(invalid, "matrix_rain_texture", "/this/matrix-atlas-does-not-exist-obs-tests.png");
	obs_source_update(filter, invalid);
	obs_data_release(invalid);
	std::this_thread::sleep_for(std::chrono::milliseconds(240));
	std::vector<uint8_t> after_invalid;
	if (!render_pixels(source, after_invalid))
		return 12;
	const bool kept_previous_font = after_invalid == equal;
	printf("MATRIX_INVALID_ATLAS_SAFE=%d PREVIOUS_ATLAS_RETAINED=%d\n", int(!after_invalid.empty()),
	       int(kept_previous_font));
	if (!kept_previous_font)
		return 15;

	if (argc == 5) {
		const char *path = argv[3];
		const int glyphs = std::atoi(argv[4]);
		auto *valid = obs_source_get_settings(filter);
		obs_data_set_string(valid, "matrix_rain_texture", path);
		obs_data_set_int(valid, "matrix_rain_texture_chars", glyphs);
		obs_source_update(filter, valid);
		obs_data_release(valid);
		std::this_thread::sleep_for(std::chrono::milliseconds(240));
		std::vector<uint8_t> custom;
		if (!render_pixels(source, custom))
			return 13;
		size_t changed_custom = 0;
		for (size_t i = 0; i < custom.size(); ++i)
			changed_custom += custom[i] != after_invalid[i];
		printf("MATRIX_GENERATED_CUSTOM_ATLAS_CHANGED=%zu\n", changed_custom);
		if (changed_custom < 100)
			return 14;
	}

	obs_set_output_source(0, nullptr);
	obs_source_filter_remove(source, filter);
	obs_source_release(filter);
	obs_source_release(source);
	obs_shutdown();
	puts("MATRIX_GPU_PASS=1");
	return 0;
}
