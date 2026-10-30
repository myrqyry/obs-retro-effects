#include <obs.h>
#include <obs-module.h>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>
int main(int argc, char **argv)
{
	if (argc != 3)
		return 2;
	if (!obs_startup("en-US", nullptr, nullptr))
		return 3;
	obs_video_info info{};
	info.graphics_module = "libobs-opengl";
	info.fps_num = 30;
	info.fps_den = 1;
	info.base_width = 256;
	info.base_height = 144;
	info.output_width = 256;
	info.output_height = 144;
	info.output_format = VIDEO_FORMAT_RGBA;
	info.scale_type = OBS_SCALE_DISABLE;
	if (obs_reset_video(&info) != OBS_VIDEO_SUCCESS)
		return 4;
	obs_module_t *module = nullptr;
	if (obs_open_module(&module, argv[1], argv[2]) != MODULE_SUCCESS || !obs_init_module(module))
		return 5;
	obs_data_t *config = obs_data_create();
	obs_data_set_int(config, "filter_type", 5);
	obs_source_t *filter = obs_source_create("obs_retro_effects_filter", "dither-test", config, nullptr);
	obs_data_release(config);
	if (!filter)
		return 6;
	std::this_thread::sleep_for(std::chrono::milliseconds(250));
	auto *properties = obs_source_properties(filter);
	auto *mode = properties ? obs_properties_get(properties, "dither_type") : nullptr;
	const int expected[] = {1, 2, 3, 4, 5, 6, 7, 8};
	bool all_found = mode && obs_property_list_item_count(mode) >= 8;
	for (int wanted : expected) {
		bool found = false;
		if (mode)
			for (size_t i = 0; i < obs_property_list_item_count(mode); ++i)
				if (obs_property_list_item_int(mode, i) == wanted)
					found = true;
		all_found = all_found && found;
	}
	auto *size = properties ? obs_properties_get(properties, "dither_size") : nullptr;
	bool bounds_ok = size && obs_property_float_min(size) == 0.4;
	auto *settings2 = obs_source_get_settings(filter);
	obs_data_set_int(settings2, "dither_type", 6);
	bool updated = mode && obs_property_modified(mode, settings2);
	bounds_ok = bounds_ok && updated && obs_property_float_min(size) == 1.0;
	auto *bayer = properties ? obs_properties_get(properties, "dither_bayer_size") : nullptr;
	auto *pixel_round = properties ? obs_properties_get(properties, "dither_round") : nullptr;
	bounds_ok = bounds_ok && bayer && pixel_round && !obs_property_visible(bayer) &&
		    !obs_property_visible(pixel_round);
	obs_data_set_int(settings2, "dither_type", 1);
	updated = mode && obs_property_modified(mode, settings2);
	bounds_ok = bounds_ok && updated && obs_property_float_min(size) == 0.4 && obs_property_visible(bayer) &&
		    obs_property_visible(pixel_round);
	obs_data_release(settings2);
	printf("DITHER_METHODS_EXPOSED=%d BOUNDS_AND_VISIBILITY=%d\n", int(all_found), int(bounds_ok));
	if (properties)
		obs_properties_destroy(properties);
	obs_source_release(filter);
	obs_shutdown();
	return all_found && bounds_ok ? 0 : 7;
}
