// Isolated OBS runtime test for the authentic H.263 filter.
// Requires libobs/OpenGL and an Xvfb environment. No installed OBS files.
#include <obs.h>
#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/vec2.h>
#include <chrono>
#include <cstring>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>

static constexpr int WIDTH = 256, HEIGHT = 144;
struct TestInput {
	gs_texture_t *texture;
	std::atomic<int> ticks{0};
};
static const char *input_name(void *)
{
	return "H.263 test video";
}
static uint32_t input_width(void *)
{
	return WIDTH;
}
static uint32_t input_height(void *)
{
	return HEIGHT;
}
static void *input_create(obs_data_t *, obs_source_t *)
{
	auto *in = new TestInput{};
	std::vector<uint8_t> rgba(WIDTH * HEIGHT * 4);
	for (int y = 0; y < HEIGHT; y++)
		for (int x = 0; x < WIDTH; x++) {
			int i = (y * WIDTH + x) * 4;
			rgba[i] = uint8_t((x * 3 + (y / 16) * 20) & 255);
			rgba[i + 1] = uint8_t((y * 2 + (x / 16) * 10) & 255);
			rgba[i + 2] = uint8_t(((x / 16 + y / 16) % 2) ? 200 : 30);
			rgba[i + 3] = 255;
		}
	obs_enter_graphics();
	in->texture = gs_texture_create(WIDTH, HEIGHT, GS_RGBA, 1, nullptr, GS_DYNAMIC);
	if (in->texture)
		gs_texture_set_image(in->texture, rgba.data(), WIDTH * 4, false);
	obs_leave_graphics();
	return in;
}
static void input_destroy(void *data)
{
	auto *in = (TestInput *)data;
	obs_enter_graphics();
	if (in->texture)
		gs_texture_destroy(in->texture);
	obs_leave_graphics();
	delete in;
}
static void input_tick(void *data, float)
{
	auto *in = (TestInput *)data;
	in->ticks.fetch_add(1);
}
static void input_render(void *data, gs_effect_t *)
{
	auto *in = (TestInput *)data;
	if (!in->texture)
		return;
	// The red channel changes on every OBS tick. This distinguishes an actual
	// sequence of H.263 pictures from one decoded still frame.
	const int elapsed = in->ticks.load();
	std::vector<uint8_t> rgba(WIDTH * HEIGHT * 4);
	for (int y = 0; y < HEIGHT; y++)
		for (int x = 0; x < WIDTH; x++) {
			int i = (y * WIDTH + x) * 4;
			rgba[i] = uint8_t((x * 3 + (y / 16) * 20 + elapsed * 4) & 255);
			rgba[i + 1] = uint8_t((y * 2 + (x / 16) * 10) & 255);
			rgba[i + 2] = uint8_t(((x / 16 + y / 16) % 2) ? 200 : 30);
			rgba[i + 3] = 255;
		}
	gs_texture_set_image(in->texture, rgba.data(), WIDTH * 4, false);
	obs_source_draw(in->texture, 0, 0, WIDTH, HEIGHT, false);
}
static obs_source_info input_info = {.id = "h263_test_input",
				     .type = OBS_SOURCE_TYPE_INPUT,
				     .output_flags = OBS_SOURCE_VIDEO,
				     .get_name = input_name,
				     .create = input_create,
				     .destroy = input_destroy,
				     .get_width = input_width,
				     .get_height = input_height,
				     .video_render = input_render};
int main(int argc, char **argv)
{
	struct CodecCase {
		const char *name;
		int ui_id, fps, bitrate, gop;
	};
	const CodecCase cases[] = {{"h263", 2, 12, 68000, 12},      {"mpeg4", 3, 12, 68000, 12},
				   {"cinepak", 4, 15, 300000, 15},  {"svq1", 5, 15, 250000, 15},
				   {"msvideo1", 6, 15, 180000, 15}, {"flv1", 7, 15, 200000, 15},
				   {"mpeg1", 8, 25, 1150000, 15},   {"mjpeg", 9, 15, 1500000, 15}};
	const CodecCase *selected = nullptr;
	if (argc == 4)
		for (const auto &item : cases)
			if (std::strcmp(argv[3], item.name) == 0)
				selected = &item;
	if (!selected) {
		std::fprintf(stderr, "Usage: %s <plugin.so> <data-dir> <codec name>\n", argv[0]);
		return 2;
	}
	setbuf(stdout, nullptr);
	const int selected_codec = selected->ui_id;
	std::printf("VIDEO_SELECTED_CODEC %s\n", argv[3]);
	if (!obs_startup("en-US", nullptr, nullptr))
		return 3;
	obs_video_info info{};
	info.graphics_module = "libobs-opengl";
	info.fps_num = 30;
	info.fps_den = 1;
	info.base_width = WIDTH;
	info.base_height = HEIGHT;
	info.output_width = WIDTH;
	info.output_height = HEIGHT;
	info.output_format = VIDEO_FORMAT_RGBA;
	info.scale_type = OBS_SCALE_DISABLE;
	if (obs_reset_video(&info) != OBS_VIDEO_SUCCESS)
		return 4;
	input_info.video_tick = input_tick;
	obs_register_source(&input_info);
	obs_module_t *module = nullptr;
	auto status = obs_open_module(&module, argv[1], argv[2]);
	if (status != MODULE_SUCCESS || !obs_init_module(module)) {
		std::fprintf(stderr, "PLUGIN_LOAD_FAILED %d\n", (int)status);
		return 5;
	}
	auto *source = obs_source_create("h263_test_input", "h263-test-source", nullptr, nullptr);
	auto *settings = obs_data_create();
	obs_data_set_int(settings, "filter_type", 10);
	obs_data_set_int(settings, "codec_type", selected_codec);
	if (selected_codec <= 3) {
		// Verify actual migration from the previous H.263-only build.
		obs_data_set_int(settings, "codec_h263_fps", 12);
		obs_data_set_int(settings, "codec_h263_bitrate", 68000);
		obs_data_set_int(settings, "codec_h263_gop", 12);
	}
	// For new choices, verify the active codec supplies its distinct preset.
	auto *filter = obs_source_create("obs_retro_effects_filter", "h263-filter", settings, nullptr);
	obs_data_release(settings);
	if (!source || !filter) {
		fprintf(stderr, "SOURCE_CREATE_FAILED\n");
		return 6;
	}
	obs_source_filter_add(source, filter);
	obs_set_output_source(0, source);
	int center_luma = 0, bar_luma = 0, nonzero_frames = 0;
	int first_red = -1, last_red = -1;
	for (int attempt = 0; attempt < 7; attempt++) {
		std::this_thread::sleep_for(std::chrono::milliseconds(120));
		obs_enter_graphics();
		auto *target = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
		bool ok = target && gs_texrender_begin_with_color_space(target, WIDTH, HEIGHT, GS_CS_SRGB);
		if (ok) {
			vec4 clear{};
			gs_clear(GS_CLEAR_COLOR, &clear, 1, 0);
			gs_projection_push();
			gs_ortho(0, WIDTH, 0, HEIGHT, -100, 100);
			obs_source_video_render(source);
			gs_projection_pop();
			gs_texrender_end(target);
			auto *stage = gs_stagesurface_create(WIDTH, HEIGHT, GS_RGBA);
			if (stage) {
				gs_stage_texture(stage, gs_texrender_get_texture(target));
				uint8_t *data = nullptr;
				uint32_t stride = 0;
				if (gs_stagesurface_map(stage, &data, &stride)) {
					auto *middle = data + 72 * stride + 128 * 4;
					auto *bar = data + 72 * stride + 8 * 4;
					center_luma = (int)middle[0] + (int)middle[1] + (int)middle[2];
					bar_luma = (int)bar[0] + (int)bar[1] + (int)bar[2];
					if (center_luma > 25)
						nonzero_frames++;
					if (first_red < 0 && center_luma > 25)
						first_red = middle[0];
					last_red = middle[0];
					std::printf("VIDEO_GPU sample=%d center=%d,%d,%d,%d bar=%d,%d,%d,%d\n", attempt,
						    middle[0], middle[1], middle[2], middle[3], bar[0], bar[1], bar[2],
						    bar[3]);
					gs_stagesurface_unmap(stage);
				}
				gs_stagesurface_destroy(stage);
			}
		}
		if (target)
			gs_texrender_destroy(target);
		obs_leave_graphics();
	}
	auto *migrated_settings = obs_source_get_settings(filter);
	bool migrated = migrated_settings && obs_data_get_int(migrated_settings, "codec_video_fps") == selected->fps &&
			obs_data_get_int(migrated_settings, "codec_video_bitrate") == selected->bitrate &&
			obs_data_get_int(migrated_settings, "codec_video_gop") == selected->gop;
	printf("VIDEO_SETTINGS_MIGRATED codec=%s pass=%d\n", argv[3], int(migrated));
	if (migrated_settings)
		obs_data_release(migrated_settings);
	if (!migrated)
		return 10;
	obs_properties_t *properties = obs_source_properties(filter);
	if (properties) {
		obs_property_t *prop = obs_properties_first(properties);
		while (prop) {
			printf("VIDEO_GUI_PROPERTY name=%s visible=%d\n", obs_property_name(prop),
			       int(obs_property_visible(prop)));
			if (!obs_property_next(&prop))
				break;
		}
	}
	obs_property_t *codec_choice = properties ? obs_properties_get(properties, "codec_type") : nullptr;
	obs_property_t *video_options = properties ? obs_properties_get(properties, "codec_video_options") : nullptr;
	bool codec_exposed = false;
	bool h263_exposed = false, mpeg4_exposed = false;
	if (codec_choice)
		for (size_t i = 0; i < obs_property_list_item_count(codec_choice); ++i) {
			long long codec = obs_property_list_item_int(codec_choice, i);
			if (codec == selected_codec)
				codec_exposed = true;
			if (codec == 2)
				h263_exposed = true;
			if (codec == 3)
				mpeg4_exposed = true;
		}
	codec_exposed = codec_exposed && h263_exposed && mpeg4_exposed;
	if (codec_choice)
		for (const auto &item : cases) {
			bool found = false;
			for (size_t i = 0; i < obs_property_list_item_count(codec_choice); ++i)
				if (obs_property_list_item_int(codec_choice, i) == item.ui_id)
					found = true;
			codec_exposed = codec_exposed && found;
		}
	bool controls_ok = codec_exposed && video_options && obs_property_visible(video_options);
	printf("VIDEO_GUI_CONFIG enabled=%d controls_visible=%d\n", int(codec_exposed), int(controls_ok));
	if (properties)
		obs_properties_destroy(properties);
	if (!controls_ok)
		return 8;

	// Switching back to the original RPZA mode must restore its settings
	// and hide H.263-only controls; the existing mode stays available.
	auto *rpza_settings = obs_source_get_settings(filter);
	obs_data_set_int(rpza_settings, "codec_type", 1);
	obs_source_update(filter, rpza_settings);
	obs_data_release(rpza_settings);
	auto *rpza_properties = obs_source_properties(filter);
	auto *rpza_controls = rpza_properties ? obs_properties_get(rpza_properties, "codec_px_scale") : nullptr;
	auto *h263_hidden = rpza_properties ? obs_properties_get(rpza_properties, "codec_video_options") : nullptr;
	bool rpza_restored = rpza_controls && h263_hidden && obs_property_visible(rpza_controls) &&
			     !obs_property_visible(h263_hidden);
	printf("VIDEO_RPZA_SWITCH restored=%d\n", int(rpza_restored));
	if (rpza_properties)
		obs_properties_destroy(rpza_properties);
	if (!rpza_restored)
		return 9;

	// Simulate a genuine UI selection after RPZA. The callback must switch
	// property visibility and load that codec's distinct starting profile.
	auto *selection_props = obs_source_properties(filter);
	auto *choice = selection_props ? obs_properties_get(selection_props, "codec_type") : nullptr;
	auto *chosen_settings = obs_source_get_settings(filter);
	obs_data_set_int(chosen_settings, "codec_type", selected_codec);
	bool callback_updated = choice && obs_property_modified(choice, chosen_settings);
	const int expected_fps = selected_codec <= 3 ? 10 : selected->fps;
	const int expected_bitrate = selected_codec <= 3 ? 64000 : selected->bitrate;
	const int expected_gop = selected_codec <= 3 ? 30 : selected->gop;
	bool preset_ok = callback_updated && obs_data_get_int(chosen_settings, "codec_video_fps") == expected_fps &&
			 obs_data_get_int(chosen_settings, "codec_video_bitrate") == expected_bitrate &&
			 obs_data_get_int(chosen_settings, "codec_video_gop") == expected_gop;
	auto *video_group = selection_props ? obs_properties_get(selection_props, "codec_video_options") : nullptr;
	preset_ok = preset_ok && video_group && obs_property_visible(video_group);
	printf("VIDEO_UI_PRESET codec=%s pass=%d\n", argv[3], int(preset_ok));
	obs_source_update(filter, chosen_settings);
	obs_data_release(chosen_settings);
	if (selection_props)
		obs_properties_destroy(selection_props);
	if (!preset_ok)
		return 11;
	obs_set_output_source(0, nullptr);
	obs_source_filter_remove(source, filter);
	obs_source_release(filter);
	obs_source_release(source);
	// OBS owns the loaded module until shutdown.
	obs_shutdown();
	printf("VIDEO_GPU_RESULT nonzero=%d center_sum=%d bar_sum=%d first_red=%d last_red=%d\n", nonzero_frames,
	       center_luma, bar_luma, first_red, last_red);
	if (nonzero_frames < 2 || bar_luma > 20 || first_red < 0 || std::abs(last_red - first_red) < 8)
		return 7;
	printf("VIDEO_OBS_RENDER_PASS codec=%s\n", argv[3]);
	return 0;
}
