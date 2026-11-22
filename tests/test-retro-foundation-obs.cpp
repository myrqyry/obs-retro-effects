#include <obs.h>
#include <obs-module.h>
#include <graphics/graphics.h>
#include <chrono>
#include <thread>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cstdlib>

static constexpr int W = 160, H = 120;
struct TestInput { gs_texture_t *texture = nullptr; };
static TestInput *test_video_input = nullptr;

static const char *input_name(void *) { return "Retro foundation input"; }
static uint32_t input_width(void *) { return W; }
static uint32_t input_height(void *) { return H; }
static void *input_create(obs_data_t *, obs_source_t *)
{
    auto *in = new TestInput;
    if (!test_video_input) test_video_input = in;
    std::vector<uint8_t> rgba(W * H * 4, 255);
    obs_enter_graphics();
    in->texture = gs_texture_create(W, H, GS_RGBA, 1, nullptr, GS_DYNAMIC);
    if (in->texture)
        gs_texture_set_image(in->texture, rgba.data(), W * 4, false);
    obs_leave_graphics();
    return in;
}
static void input_destroy(void *data)
{
    auto *in = (TestInput *)data;
    if (test_video_input == in) test_video_input = nullptr;
    obs_enter_graphics();
    if (in->texture)
        gs_texture_destroy(in->texture);
    obs_leave_graphics();
    delete in;
}
static void input_render(void *data, gs_effect_t *)
{
    auto *in = (TestInput *)data;
    if (in->texture)
        obs_source_draw(in->texture, 0, 0, W, H, false);
}
static obs_source_info input_info = {
    .id = "retro_foundation_input",
    .type = OBS_SOURCE_TYPE_INPUT,
    .output_flags = OBS_SOURCE_VIDEO,
    .get_name = input_name,
    .create = input_create,
    .destroy = input_destroy,
    .get_width = input_width,
    .get_height = input_height,
    .video_render = input_render,
};
static bool render_pixels(obs_source_t *source, std::vector<uint8_t> &pixels)
{
    obs_enter_graphics();
    auto *target = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
    auto *stage = gs_stagesurface_create(W, H, GS_RGBA);
    bool ok = target && stage && gs_texrender_begin_with_color_space(target, W, H, GS_CS_SRGB);
    if (ok) {
        struct vec4 clear{};
        gs_clear(GS_CLEAR_COLOR, &clear, 1, 0);
        gs_projection_push();
        gs_ortho(0, W, 0, H, -100, 100);
        obs_source_video_render(source);
        gs_projection_pop();
        gs_texrender_end(target);
        gs_stage_texture(stage, gs_texrender_get_texture(target));
        uint8_t *mapped = nullptr;
        uint32_t pitch = 0;
        ok = gs_stagesurface_map(stage, &mapped, &pitch);
        if (ok) {
            pixels.resize(W * H * 4);
            for (int y = 0; y < H; ++y)
                memcpy(pixels.data() + y * W * 4, mapped + y * pitch, W * 4);
            gs_stagesurface_unmap(stage);
        }
    }
    if (stage) gs_stagesurface_destroy(stage);
    if (target) gs_texrender_destroy(target);
    obs_leave_graphics();
    return ok;
}
static void set_video_color(uint8_t r, uint8_t g, uint8_t b)
{
    std::vector<uint8_t> rgba(W * H * 4, 255);
    for (size_t i = 0; i < rgba.size(); i += 4) {
        rgba[i] = r;
        rgba[i + 1] = g;
        rgba[i + 2] = b;
    }
    obs_enter_graphics();
    gs_texture_set_image(test_video_input->texture, rgba.data(), W * 4, false);
    obs_leave_graphics();
}
static bool pixel_is(const std::vector<uint8_t> &frame, unsigned int rgb, int tolerance = 12)
{
    if (frame.size() != W * H * 4) return false;
    size_t at = (H / 2 * W + W / 2) * 4;
    return abs((int)frame[at] - (int)(rgb >> 16 & 255)) <= tolerance &&
           abs((int)frame[at + 1] - (int)(rgb >> 8 & 255)) <= tolerance &&
           abs((int)frame[at + 2] - (int)(rgb & 255)) <= tolerance;
}
static void settle()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
}
int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    setbuf(stdout, nullptr);
    if (!obs_startup("en-US", nullptr, nullptr)) return 3;
    obs_video_info info{};
    info.graphics_module = "libobs-opengl";
    info.fps_num = 30; info.fps_den = 1;
    info.base_width = W; info.base_height = H;
    info.output_width = W; info.output_height = H;
    info.output_format = VIDEO_FORMAT_RGBA;
    info.scale_type = OBS_SCALE_DISABLE;
    if (obs_reset_video(&info) != OBS_VIDEO_SUCCESS) return 4;
    obs_register_source(&input_info);
    obs_module_t *module = nullptr;
    if (obs_open_module(&module, argv[1], argv[2]) != MODULE_SUCCESS || !obs_init_module(module))
        return 5;
    bool passed = true;

    // A new VHS filter starts with a visible wrinkle probability.
    auto *fresh = obs_data_create();
    obs_data_set_int(fresh, "filter_type", 11);
    auto *fresh_vhs = obs_source_create("obs_retro_effects_filter", "retro-fresh-vhs", fresh, nullptr);
    obs_data_release(fresh);
    settle();
    auto *fresh_values = fresh_vhs ? obs_source_get_settings(fresh_vhs) : nullptr;
    bool default_ok = fresh_values && obs_data_get_double(fresh_values, "vhs_wrinkle_occurrence") == 10.0 &&
                      !obs_data_has_user_value(fresh_values, "vhs_wrinkle_occurrence");
    printf("VHS_WRINKLE_FRESH_DEFAULT=%d\n", int(default_ok));
    passed &= default_ok;
    if (fresh_values) obs_data_release(fresh_values);
    if (fresh_vhs) obs_source_release(fresh_vhs);

    // Real settings migration, including an explicit legacy user value.
    auto *legacy = obs_data_create();
    obs_data_set_int(legacy, "filter_type", 11);
    obs_data_set_double(legacy, "vhs_wrinkle_occurrence_prob", 23.0);
    auto *vhs = obs_source_create("obs_retro_effects_filter", "retro-legacy-vhs", legacy, nullptr);
    obs_data_release(legacy);
    settle();
    auto *vhs_settings = vhs ? obs_source_get_settings(vhs) : nullptr;
    bool migrated = vhs_settings && obs_data_get_double(vhs_settings, "vhs_wrinkle_occurrence") == 23.0 &&
                    !obs_data_has_user_value(vhs_settings, "vhs_wrinkle_occurrence_prob");
    printf("VHS_WRINKLE_MIGRATION=%d\n", int(migrated));
    passed &= migrated;
    if (vhs_settings) obs_data_release(vhs_settings);
    if (vhs) obs_source_release(vhs);

    // A newer explicit setting must win over conflicting legacy data.
    auto *both = obs_data_create();
    obs_data_set_int(both, "filter_type", 11);
    obs_data_set_double(both, "vhs_wrinkle_occurrence_prob", 23.0);
    obs_data_set_double(both, "vhs_wrinkle_occurrence", 7.0);
    auto *both_vhs = obs_source_create("obs_retro_effects_filter", "retro-both-vhs", both, nullptr);
    obs_data_release(both);
    settle();
    auto *both_values = both_vhs ? obs_source_get_settings(both_vhs) : nullptr;
    bool current_wins = both_values && obs_data_get_double(both_values, "vhs_wrinkle_occurrence") == 7.0 &&
                        !obs_data_has_user_value(both_values, "vhs_wrinkle_occurrence_prob");
    printf("VHS_CURRENT_SETTING_PRESERVED=%d\n", int(current_wins));
    passed &= current_wins;
    if (both_values) obs_data_release(both_values);
    if (both_vhs) obs_source_release(both_vhs);

    auto *input = obs_source_create("retro_foundation_input", "retro-foundation-video", nullptr, nullptr);
    auto *palette = obs_source_create("retro_foundation_input", "retro-foundation-palette", nullptr, nullptr);
    auto *settings = obs_data_create();
    obs_data_set_int(settings, "filter_type", 13);
    obs_data_set_int(settings, "scanlines_profile", 0);
    obs_data_set_double(settings, "scanlines_period", 16.0);
    obs_data_set_double(settings, "scanlines_intensity", 100.0);
    obs_data_set_double(settings, "scanlines_speed", 0.0);
    auto *filter = obs_source_create("obs_retro_effects_filter", "retro-foundation-filter", settings, nullptr);
    obs_data_release(settings);
    if (!input || !palette || !filter) return 6;
    obs_source_filter_add(input, filter);
    obs_set_output_source(0, input);
    settle();

    std::vector<uint8_t> sine, sawtooth;
    bool renders = render_pixels(input, sine);
    auto *new_settings = obs_source_get_settings(filter);
    obs_data_set_int(new_settings, "scanlines_profile", 2);
    obs_source_update(filter, new_settings);
    obs_data_release(new_settings);
    settle();
    renders &= render_pixels(input, sawtooth);
    size_t changed = 0, visible = 0;
    if (renders)
        for (size_t i = 0; i < sine.size(); i += 4) {
            changed += abs((int)sine[i] - (int)sawtooth[i]) > 5;
            visible += sawtooth[i] > 10;
        }
    bool sawtooth_works = renders && changed > 1000 && visible > 1000;
    printf("SCANLINES_SAWTOOTH_RENDERED=%d DIFFERENT_PIXELS=%zu VISIBLE=%zu\n",
           int(sawtooth_works), changed, visible);
    passed &= sawtooth_works;

    // Exercise source-map target reuse, switching to passthrough and back.
    new_settings = obs_source_get_settings(filter);
    obs_data_set_int(new_settings, "filter_type", 4);
    obs_data_set_int(new_settings, "posterize_technique", 2);
    obs_data_set_string(new_settings, "posterize_map_source", "retro-foundation-palette");
    obs_source_update(filter, new_settings);
    obs_data_release(new_settings);
    settle();
    std::vector<uint8_t> mapped;
    bool posterize_works = render_pixels(input, mapped) && render_pixels(input, mapped);
    new_settings = obs_source_get_settings(filter);
    obs_data_set_string(new_settings, "posterize_map_source", "");
    obs_source_update(filter, new_settings);
    obs_data_release(new_settings);
    settle();
    posterize_works &= render_pixels(input, mapped);
    new_settings = obs_source_get_settings(filter);
    obs_data_set_string(new_settings, "posterize_map_source", "retro-foundation-palette");
    obs_source_update(filter, new_settings);
    obs_data_release(new_settings);
    settle();
    posterize_works &= render_pixels(input, mapped);
    printf("POSTERIZE_SOURCE_RESELECT_RENDER=%d\n", int(posterize_works));
    passed &= posterize_works;

    // Hardware palettes use actual RGB nearest-neighbor matching, not luma ramps.
    set_video_color(255, 255, 255);
    new_settings = obs_source_get_settings(filter);
    obs_data_set_int(new_settings, "posterize_technique", 3);
    obs_data_set_int(new_settings, "posterize_palette", 1);
    obs_source_update(filter, new_settings);
    obs_data_release(new_settings);
    settle();
    bool dmg = render_pixels(input, mapped) && pixel_is(mapped, 0x9bbc0f);
    printf("POSTERIZE_DMG_GREEN=%d PIXEL=%u,%u,%u\n", int(dmg), mapped[(H/2*W+W/2)*4],
           mapped[(H/2*W+W/2)*4+1], mapped[(H/2*W+W/2)*4+2]);
    passed &= dmg;
    auto *posterize_props = obs_source_properties(filter);
    auto *posterize_mode = posterize_props ? obs_properties_get(posterize_props, "posterize_technique") : nullptr;
    auto *posterize_palette = posterize_props ? obs_properties_get(posterize_props, "posterize_palette") : nullptr;
    auto *posterize_levels = posterize_props ? obs_properties_get(posterize_props, "posterize_levels") : nullptr;
    bool posterize_ui = posterize_mode && posterize_palette && posterize_levels &&
                        obs_property_list_item_count(posterize_mode) == 4 &&
                        obs_property_visible(posterize_palette) && !obs_property_visible(posterize_levels);
    auto *posterize_ui_values = obs_source_get_settings(filter);
    obs_data_set_int(posterize_ui_values, "posterize_technique", 0);
    if (posterize_mode) obs_property_modified(posterize_mode, posterize_ui_values);
    posterize_ui &= posterize_palette && !obs_property_visible(posterize_palette) &&
                    posterize_levels && obs_property_visible(posterize_levels);
    obs_data_release(posterize_ui_values);
    if (posterize_props) obs_properties_destroy(posterize_props);
    printf("POSTERIZE_PALETTE_UI=%d\n", int(posterize_ui));
    passed &= posterize_ui;

    // Ordered GPU Dither should mix only actual CGA colors on mid-tone gray.
    set_video_color(128, 128, 128);
    new_settings = obs_source_get_settings(filter);
    obs_data_set_int(new_settings, "filter_type", 5);
    obs_data_set_int(new_settings, "dither_type", 1);
    obs_data_set_int(new_settings, "dither_palette", 2);
    obs_data_set_int(new_settings, "dither_size", 1);
    obs_source_update(filter, new_settings);
    obs_data_release(new_settings);
    settle();
    bool dither_renders = render_pixels(input, mapped);
    const unsigned int cga_colors[] = {0x000000, 0x55ffff, 0xff55ff, 0xffffff};
    size_t matches[4] = {}, outside = 0;
    if (dither_renders) {
        for (size_t i = 0; i < mapped.size(); i += 4) {
            int matched = -1;
            for (int c = 0; c < 4; ++c) {
                int r = (cga_colors[c] >> 16) & 255, g = (cga_colors[c] >> 8) & 255;
                int b = cga_colors[c] & 255;
                if (abs((int)mapped[i] - r) < 12 &&
                    abs((int)mapped[i+1] - g) < 12 && abs((int)mapped[i+2] - b) < 12) {
                    matched = c;
                    break;
                }
            }
            if (matched == -1) ++outside;
            else ++matches[matched];
        }
    }
    size_t distinct = 0;
    for (size_t n : matches) distinct += n > 100;
    bool cga_dither = dither_renders && distinct >= 2 && outside < 100;
    printf("DITHER_CGA_COUNTS=%zu,%zu,%zu,%zu OUTSIDE=%zu PASS=%d\n",
           matches[0], matches[1], matches[2], matches[3], outside, int(cga_dither));
    passed &= cga_dither;
    set_video_color(255, 0, 77);
    new_settings = obs_source_get_settings(filter);
    obs_data_set_int(new_settings, "dither_type", 11);
    obs_data_set_int(new_settings, "dither_palette", 3);
    obs_source_update(filter, new_settings);
    obs_data_release(new_settings);
    settle();
    bool pico8 = render_pixels(input, mapped) && pixel_is(mapped, 0xff004d);
    printf("DITHER_PICO8_PALETTE=%d PIXEL=%u,%u,%u\n", int(pico8), mapped[(H/2*W+W/2)*4],
           mapped[(H/2*W+W/2)*4+1], mapped[(H/2*W+W/2)*4+2]);
    passed &= pico8;
    // Properties must reflect that CPU diffusion does not consume the GPU palette.
    auto *dither_props = obs_source_properties(filter);
    auto *dither_mode = dither_props ? obs_properties_get(dither_props, "dither_type") : nullptr;
    auto *palette_prop = dither_props ? obs_properties_get(dither_props, "dither_palette") : nullptr;
    auto *color_steps = dither_props ? obs_properties_get(dither_props, "dither_color_steps") : nullptr;
    bool palette_ui = dither_mode && palette_prop && color_steps &&
                      obs_property_list_item_count(palette_prop) == 4 &&
                      obs_property_visible(palette_prop) && !obs_property_visible(color_steps);
    auto *ui_values = obs_source_get_settings(filter);
    obs_data_set_int(ui_values, "dither_type", 6); // Floyd-Steinberg CPU
    if (dither_mode) obs_property_modified(dither_mode, ui_values);
    palette_ui &= palette_prop && !obs_property_visible(palette_prop) &&
                  color_steps && obs_property_visible(color_steps);
    obs_data_set_int(ui_values, "dither_type", 11);
    obs_data_set_int(ui_values, "dither_palette", 0);
    if (dither_mode) obs_property_modified(dither_mode, ui_values);
    palette_ui &= palette_prop && obs_property_visible(palette_prop) &&
                  color_steps && obs_property_visible(color_steps);
    obs_data_release(ui_values);
    if (dither_props) obs_properties_destroy(dither_props);
    printf("DITHER_PALETTE_UI_MODE_SWITCH=%d\n", int(palette_ui));
    passed &= palette_ui;

    // Saved palette choice must not alter the legacy CPU diffusion result.
    set_video_color(122, 122, 122);
    new_settings = obs_source_get_settings(filter);
    obs_data_set_int(new_settings, "dither_type", 6);
    obs_data_set_int(new_settings, "dither_palette", 0);
    obs_source_update(filter, new_settings);
    obs_data_release(new_settings);
    settle();
    std::vector<uint8_t> cpu_original, cpu_with_palette;
    bool cpu_renders = render_pixels(input, cpu_original);
    new_settings = obs_source_get_settings(filter);
    obs_data_set_int(new_settings, "dither_palette", 3);
    obs_source_update(filter, new_settings);
    obs_data_release(new_settings);
    settle();
    cpu_renders &= render_pixels(input, cpu_with_palette);
    bool cpu_unaffected = cpu_renders && cpu_original == cpu_with_palette;
    printf("DITHER_CPU_UNAFFECTED_BY_PALETTE=%d\n", int(cpu_unaffected));
    passed &= cpu_unaffected;
    obs_set_output_source(0, nullptr);
    obs_source_filter_remove(input, filter);
    obs_source_release(filter);
    obs_source_release(palette);
    obs_source_release(input);
    obs_shutdown();
    return passed ? 0 : 7;
}
