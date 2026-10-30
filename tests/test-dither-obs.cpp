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

static constexpr int W = 96, H = 64;
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
	obs_source_t *source = obs_source_create("dither_test_input", "dither-src", nullptr, nullptr);
	auto *settings = obs_data_create();
	obs_data_set_int(settings, "filter_type", 5);
	obs_data_set_int(settings, "dither_type", 1);
	obs_data_set_int(settings, "dither_color_steps", 1);
	obs_data_set_bool(settings, "dither_mono", true);
	obs_data_set_bool(settings, "dither_round", true);
	obs_data_set_double(settings, "dither_size", 1.0);
	auto *filter = obs_source_create("obs_retro_effects_filter", "dither-filter", settings, nullptr);
	obs_data_release(settings);
	if (!source || !filter)
		return 6;
	obs_source_filter_add(source, filter);
	obs_set_output_source(0, source);
	std::array<uint64_t, 12> fingerprints{};
	for (int mode = 1; mode <= 12; ++mode) {
		auto *choice = obs_source_get_settings(filter);
		obs_data_set_int(choice, "dither_type", mode);
		obs_source_update(filter, choice);
		obs_data_release(choice);
		std::this_thread::sleep_for(std::chrono::milliseconds(160));
		std::vector<uint8_t> pixels;
		bool ok = false;
		for (int attempt = 0; attempt < 3 && !ok; ++attempt) {
			std::this_thread::sleep_for(std::chrono::milliseconds(35));
			ok = render_pixels(source, pixels);
		}
		if (!ok)
			return 7;
		uint64_t hash = 1469598103934665603ULL;
		int bright = 0, dark = 0, other = 0;
		for (size_t p = 0; p < pixels.size(); p += 4) {
			if (pixels[p] > 220)
				++bright;
			else if (pixels[p] < 35)
				++dark;
			else
				++other;
			hash ^= pixels[p];
			hash *= 1099511628211ULL;
		}
		printf("DITHER_GPU mode=%d bright=%d dark=%d mid=%d hash=%llu\n", mode, bright, dark, other,
		       (unsigned long long)hash);
		if (!bright || !dark || bright + dark < 3000)
			return 8;
		fingerprints[mode - 1] = hash;
	}
	/* A uniform gray image must remain uniform under None (Posterize)
	 * while real blue noise distributes quantized levels spatially.
	 * Since the source is held steady, a second render must reproduce
	 * the exact same blue-noise pixels (no RNG/flicker). */
	std::vector<uint8_t> flat(W * H * 4);
	for (size_t i = 0; i < flat.size(); i += 4) {
		flat[i] = flat[i + 1] = flat[i + 2] = 128;
		flat[i + 3] = 255;
	}
	obs_enter_graphics();
	gs_texture_set_image(active_input->texture, flat.data(), W * 4, false);
	obs_leave_graphics();
	auto select_and_draw = [&](int mode, std::vector<uint8_t> &pixels) {
		auto *choice = obs_source_get_settings(filter);
		obs_data_set_int(choice, "dither_type", mode);
		obs_source_update(filter, choice);
		obs_data_release(choice);
		std::this_thread::sleep_for(std::chrono::milliseconds(160));
		return render_pixels(source, pixels);
	};
	std::vector<uint8_t> no_dither, blue_a, blue_b;
	if (!select_and_draw(11, no_dither) || !select_and_draw(12, blue_a) || !render_pixels(source, blue_b))
		return 10;
	bool posterize_uniform = true;
	size_t blue_dark = 0, blue_bright = 0;
	for (size_t i = 0; i < flat.size(); i += 4) {
		posterize_uniform = posterize_uniform && no_dither[i] == no_dither[0];
		blue_dark += blue_a[i] < 40;
		blue_bright += blue_a[i] > 220;
	}
	bool mask_ok = posterize_uniform && blue_dark > 1500 && blue_bright > 1500 && blue_a == blue_b;
	printf("NONE_UNIFORM=%d BLUE_NOISE_MASK_DARK=%zu BRIGHT=%zu STABLE=%d\n", int(posterize_uniform), blue_dark,
	       blue_bright, int(blue_a == blue_b));
	if (!mask_ok)
		return 11;
	// Exercise genuinely large 32px / 128px logical pixels through OBS
	// rendering in both GPU and sequential CPU dithering paths.
	std::vector<uint8_t> textured(W * H * 4);
	for (int y = 0; y < H; ++y)
		for (int x = 0; x < W; ++x) {
			size_t i = ((size_t)y * W + x) * 4;
			textured[i] = textured[i + 1] = textured[i + 2] =
				(uint8_t)(25 + (x * 37 + y * 61 + x * y * 3) % 205);
			textured[i + 3] = 255;
		}
	obs_enter_graphics();
	gs_texture_set_image(active_input->texture, textured.data(), W * 4, false);
	obs_leave_graphics();
	for (int mode : {1, 2, 6, 12}) {
		for (double size : {32.0, 128.0}) {
			auto *config = obs_source_get_settings(filter);
			obs_data_set_int(config, "dither_type", mode);
			obs_data_set_double(config, "dither_size", size);
			obs_source_update(filter, config);
			obs_data_release(config);
			std::this_thread::sleep_for(std::chrono::milliseconds(170));
			std::vector<uint8_t> oversized;
			if (!render_pixels(source, oversized) || oversized.size() != W * H * 4)
				return 12;
			int opaque = 0, valid = 0;
			for (size_t i = 0; i < oversized.size(); i += 4) {
				opaque += oversized[i + 3] > 250;
				valid += oversized[i] < 20 || oversized[i] > 235;
			}
			bool successful = opaque >= W * H - 50 && valid >= W * H - 50;
			printf("DITHER_LARGE_SIZE mode=%d size=%.0f opaque=%d binary=%d pass=%d\n", mode, size, opaque,
			       valid, int(successful));
			if (!successful)
				return 13;
		}
	}
	std::set<uint64_t> unique(fingerprints.begin(), fingerprints.end());
	printf("DITHER_GPU_UNIQUE=%zu of 12\n", unique.size());
	obs_set_output_source(0, nullptr);
	obs_source_filter_remove(source, filter);
	obs_source_release(filter);
	obs_source_release(source);
	obs_shutdown();
	return unique.size() == 12 ? 0 : 9;
}
