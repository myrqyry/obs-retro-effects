#!/usr/bin/env python3
"""Source contracts for small OBS filter fixes; real render smoke tests supplement these."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]

def source(path):
    return (ROOT / path).read_text()

class RetroFoundationContract(unittest.TestCase):
    def test_bloom_allocates_its_own_struct(self):
        self.assertIn("bzalloc(sizeof(bloom_data_t))", source("src/blur/bloom.c"))

    def test_vhs_wrinkle_default_and_unset_match_live_key(self):
        c = source("src/filters/vhs.c")
        self.assertIn('obs_data_set_default_double(settings, "vhs_wrinkle_occurrence", 10.0)', c)
        self.assertIn('obs_data_unset_user_value(settings, "vhs_wrinkle_occurrence")', c)
        self.assertIn('obs_data_unset_user_value(settings, "vhs_wrinkle_occurrence_prob")', c)

    def test_ntsc_saturation_uses_its_own_parameter_guard(self):
        c = source("src/filters/ntsc.c")
        self.assertRegex(c, r'if\s*\(filter->param_decode_saturation\)\s*\{\s*gs_effect_set_float\(filter->param_decode_saturation,')

    def test_scanlines_sawtooth_is_implemented_and_selected(self):
        c = source("src/filters/scan-lines.c")
        shader = source("data/shaders/scan-lines.effect")
        self.assertIn("case SCANLINES_PROFILE_SAWTOOTH:", c)
        self.assertIn('"DrawSawtooth"', c)
        self.assertIn("float4 sawtoothImage(", shader)
        self.assertRegex(shader, r'technique\s+DrawSawtooth\b')

    def test_scanlines_intensity_defaults_and_resets(self):
        c = source("src/filters/scan-lines.c")
        self.assertRegex(c, r'obs_data_set_default_double\(settings, "scanlines_intensity",\s*\d+(?:\.\d+)?\)')
        self.assertIn('obs_data_unset_user_value(settings, "scanlines_intensity")', c)

    def test_posterize_failed_source_render_passes_through(self):
        c = source("src/filters/posterize.c")
        mapped = c.split("source_texture = rendered ?", 1)[1].split("set_render_parameters();", 1)[0]
        self.assertIn("if (!source_texture)", mapped)

    def test_posterize_reuses_render_target_and_releases_resources(self):
        c = source("src/filters/posterize.c")
        h = source("src/filters/posterize.h")
        self.assertIn("gs_texrender_t *color_source_render;", h)
        self.assertIn("gs_texrender_reset(filter->color_source_render)", c)
        self.assertIn("gs_texrender_destroy(data->color_source_render)", c)
        self.assertIn("dstr_free(&data->color_source_name)", c)
        self.assertNotIn("gs_texrender_t *color_source_render = NULL", c)
        self.assertRegex(c, r'if\s*\(filter->color_source\)\s*\{\s*obs_weak_source_release\(filter->color_source\)')

if __name__ == "__main__":
    unittest.main()
