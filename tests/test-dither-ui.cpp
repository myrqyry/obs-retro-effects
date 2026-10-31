#include <obs.h>
#include <obs-module.h>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>
#include <initializer_list>
#include <utility>
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
	const int expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
	bool all_found = mode && obs_property_list_item_count(mode) == 12;
	for (int wanted : expected) {
		bool found = false;
		if (mode)
			for (size_t i = 0; i < obs_property_list_item_count(mode); ++i)
				if (obs_property_list_item_int(mode, i) == wanted)
					found = true;
		all_found = all_found && found;
		if (mode)
			for (size_t i = 0; i < obs_property_list_item_count(mode); ++i) {
				const char *label = obs_property_list_item_name(mode, i);
				all_found = all_found && label && !strstr(label, "Noita") && !strstr(label, "noita");
			}
	}
	auto *size = properties ? obs_properties_get(properties, "dither_size") : nullptr;
	bool bounds_ok = size && obs_property_get_type(size) == OBS_PROPERTY_INT && obs_property_int_min(size) == 1 &&
			 obs_property_int_max(size) == 128 && obs_property_int_step(size) == 1;
	auto *bayer = properties ? obs_properties_get(properties, "dither_bayer_size") : nullptr;
	bounds_ok = bounds_ok && !bayer; // All Bayer sizes belong in Dither Type.
	auto *settings2 = obs_source_get_settings(filter);
	obs_data_set_int(settings2, "dither_type", 6);
	bool updated = mode && obs_property_modified(mode, settings2);
	bounds_ok = bounds_ok && updated && obs_property_int_min(size) == 1 && obs_property_int_max(size) == 128;
	auto *pixel_round = properties ? obs_properties_get(properties, "dither_round") : nullptr;
	bounds_ok = bounds_ok && pixel_round && !obs_property_visible(pixel_round);
	obs_data_set_int(settings2, "dither_type", 1);
	updated = mode && obs_property_modified(mode, settings2);
	bounds_ok = bounds_ok && updated && obs_property_int_min(size) == 1 && obs_property_int_max(size) == 128 &&
		    obs_property_visible(pixel_round);
	// Bayer 2x2 and 8x8 remain GPU modes with whole-pixel size limits
	// and the existing optional pixel-rounding control.
	for (int bayer_mode : {9, 10}) {
		obs_data_set_int(settings2, "dither_type", bayer_mode);
		bool changed = mode && obs_property_modified(mode, settings2);
		bool gpu_options = changed && obs_property_int_min(size) == 1 && obs_property_int_max(size) == 128 &&
				   obs_property_visible(pixel_round);
		bounds_ok = bounds_ok && gpu_options;
		printf("DITHER_GPU_BAYER_OPTIONS mode=%d pass=%d\n", bayer_mode, int(gpu_options));
	}
	// Old saved fractional sizes must normalize once to the nearest whole
	// size and remain integer-valued when OBS reads/writes the settings.
	for (const auto &sample : {std::pair<double, int>{0.4, 1}, {1.4, 1}, {2.6, 3}, {9.8, 10}, {128.7, 128}}) {
		auto *old_preset = obs_data_create();
		obs_data_set_int(old_preset, "filter_type", 5);
		obs_data_set_double(old_preset, "dither_size", sample.first);
		auto *old_filter =
			obs_source_create("obs_retro_effects_filter", "fractional-preset", old_preset, nullptr);
		obs_data_release(old_preset);
		std::this_thread::sleep_for(std::chrono::milliseconds(120));
		auto *updated_settings = old_filter ? obs_source_get_settings(old_filter) : nullptr;
		double saved = updated_settings ? obs_data_get_double(updated_settings, "dither_size") : -1.0;
		bool normalized = updated_settings && saved == (double)sample.second &&
				  obs_data_get_int(updated_settings, "dither_size") == sample.second;
		if (updated_settings)
			obs_data_release(updated_settings);
		if (old_filter)
			obs_source_release(old_filter);
		printf("DITHER_INTEGER_SIZE old=%.1f new=%.1f pass=%d\n", sample.first, saved, int(normalized));
		bounds_ok = bounds_ok && normalized;
	}
	obs_data_release(settings2);
	for (int old_size : {2, 4, 8}) {
		obs_data_t *legacy = obs_data_create();
		obs_data_set_int(legacy, "filter_type", 5);
		obs_data_set_int(legacy, "dither_type", 1);
		obs_data_set_int(legacy, "dither_bayer_size", old_size);
		obs_source_t *source =
			obs_source_create("obs_retro_effects_filter", "legacy-dither-test", legacy, nullptr);
		obs_data_release(legacy);
		// OBS dispatches filter initialization asynchronously.
		std::this_thread::sleep_for(std::chrono::milliseconds(260));
		if (!source) {
			bounds_ok = false;
			continue;
		}
		obs_data_t *loaded = obs_source_get_settings(source);
		int expected_id = old_size == 2 ? 9 : old_size == 8 ? 10 : 1;
		bool migrated = loaded && obs_data_get_int(loaded, "dither_type") == expected_id;
		migrated = migrated && !obs_data_has_user_value(loaded, "dither_bayer_size");
		// Selecting the legacy id explicitly afterwards should remain 4x4.
		obs_data_set_int(loaded, "dither_type", 1);
		obs_source_update(source, loaded);
		std::this_thread::sleep_for(std::chrono::milliseconds(140));
		migrated = migrated && obs_data_get_int(loaded, "dither_type") == 1;
		if (loaded)
			obs_data_release(loaded);
		obs_source_release(source);
		std::this_thread::sleep_for(std::chrono::milliseconds(120));
		bounds_ok = bounds_ok && migrated;
		printf("DITHER_LEGACY_BAYER_SIZE=%d MIGRATION=%d\n", old_size, int(migrated));
	}
	// Unordered presets may retain a legacy Bayer size. That key must
	// disappear on initial load, or a later 4x4 selection is overridden.
	for (int old_size : {2, 8}) {
		auto *legacy = obs_data_create();
		obs_data_set_int(legacy, "filter_type", 5);
		obs_data_set_int(legacy, "dither_type", 2); // old Unordered
		obs_data_set_int(legacy, "dither_bayer_size", old_size);
		auto *legacy_filter =
			obs_source_create("obs_retro_effects_filter", "unordered-preset", legacy, nullptr);
		obs_data_release(legacy);
		std::this_thread::sleep_for(std::chrono::milliseconds(130));
		auto *saved = legacy_filter ? obs_source_get_settings(legacy_filter) : nullptr;
		bool migrated = saved && obs_data_get_int(saved, "dither_type") == 2 &&
				!obs_data_has_user_value(saved, "dither_bayer_size");
		if (saved) {
			obs_data_set_int(saved, "dither_type", 1); // explicitly select Bayer 4x4
			obs_source_update(legacy_filter, saved);
			std::this_thread::sleep_for(std::chrono::milliseconds(130));
			migrated = migrated && obs_data_get_int(saved, "dither_type") == 1;
			obs_data_release(saved);
		}
		if (legacy_filter)
			obs_source_release(legacy_filter);
		printf("DITHER_UNORDERED_LEGACY_BAYER=%d PRESERVES_NEW_SELECTION=%d\n", old_size, int(migrated));
		bounds_ok = bounds_ok && migrated;
	}
	for (int extra : {11, 12}) {
		obs_data_t *option = obs_source_get_settings(filter);
		obs_data_set_int(option, "dither_type", extra);
		bool accepted = mode && obs_property_modified(mode, option);
		auto *offset = properties ? obs_properties_get(properties, "dither_offset_x") : nullptr;
		accepted = accepted && offset && (obs_property_visible(offset) == (extra == 12));
		obs_data_release(option);
		bounds_ok = bounds_ok && accepted;
		printf("DITHER_ADDITIONAL_MODE=%d controls=%d\n", extra, int(accepted));
	}
	printf("DITHER_METHODS_EXPOSED=%d BOUNDS_AND_VISIBILITY=%d\n", int(all_found), int(bounds_ok));
	if (properties)
		obs_properties_destroy(properties);
	obs_source_release(filter);
	obs_shutdown();
	return all_found && bounds_ok ? 0 : 7;
}
