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

static const char *input_name(void *) { return "Retro foundation input"; }
static uint32_t input_width(void *) { return W; }
static uint32_t input_height(void *) { return H; }
static void *input_create(obs_data_t *, obs_source_t *)
{
    auto *in = new TestInput;
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
    obs_set_output_source(0, nullptr);
    obs_source_filter_remove(input, filter);
    obs_source_release(filter);
    obs_source_release(palette);
    obs_source_release(input);
    obs_shutdown();
    return passed ? 0 : 7;
}
