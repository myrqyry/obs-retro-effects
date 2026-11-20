#pragma once
#include <obs-module.h>

/* Source-scoped bindings persist in OBS's own scene collection.
 * Their callbacks borrow the source only until the owning pair is
 * unregistered, and never change any shader parameters or other sources. */
static inline bool effect_enable_pressed(void *data, obs_hotkey_pair_id id,
                                         obs_hotkey_t *key, bool pressed)
{
    UNUSED_PARAMETER(id);
    UNUSED_PARAMETER(key);
    obs_source_t *source = data;
    if (!pressed || obs_source_enabled(source))
        return false;
    obs_source_set_enabled(source, true);
    return true;
}

static inline bool effect_disable_pressed(void *data, obs_hotkey_pair_id id,
                                          obs_hotkey_t *key, bool pressed)
{
    UNUSED_PARAMETER(id);
    UNUSED_PARAMETER(key);
    obs_source_t *source = data;
    if (!pressed || !obs_source_enabled(source))
        return false;
    obs_source_set_enabled(source, false);
    return true;
}

static inline obs_hotkey_pair_id effect_hotkeys_register(obs_source_t *source)
{
    return obs_hotkey_pair_register_source(
        source, "effect.enable", obs_module_text("Effect.Enable"),
        "effect.disable", obs_module_text("Effect.Disable"),
        effect_enable_pressed, effect_disable_pressed, source, source);
}

static inline void effect_hotkeys_unregister(obs_hotkey_pair_id pair)
{
    if (pair != OBS_INVALID_HOTKEY_PAIR_ID)
        obs_hotkey_pair_unregister(pair);
}
