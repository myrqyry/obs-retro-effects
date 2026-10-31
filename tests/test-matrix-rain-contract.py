#!/usr/bin/env python3
"""Focused contracts for the Matrix Rain bugfixes; supplemented by real OBS GPU tests."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
C = (ROOT / "src/filters/matrix-rain.c").read_text()
SHADER = (ROOT / "data/shaders/matrix-rain.effect").read_text()

class MatrixRainRegression(unittest.TestCase):
    def test_bloom_threshold_is_not_overwritten_every_frame(self):
        render = C.split("void matrix_rain_filter_video_render(", 1)[1].split("static void matrix_rain_set_functions", 1)[0]
        self.assertNotRegex(render, r"bloom_data->brightness_threshold\s*=\s*0\.4f")
        self.assertIn('obs_data_get_double(settings, "matrix_bloom_threshold")', C)
    def test_brightness_mapping_clamps_and_guards_zero_range(self):
        self.assertNotIn('g / (max_brightness - min_brightness)', SHADER)
        self.assertRegex(SHADER, r'max\s*\(\s*max_brightness\s*-\s*min_brightness\s*,\s*EPS\s*\)')
    def test_black_pixel_colorize_is_safe(self):
        self.assertNotIn("col.rgb / max(col.r, max(col.g, col.b))", SHADER)
        self.assertRegex(SHADER, r'max\s*\(\s*max\s*\(\s*col\.r\s*,\s*max\s*\(\s*col\.g\s*,\s*col\.b\s*\)\s*\)\s*,\s*EPS\s*\)')
    def test_texture_load_checks_failure_before_dimensions(self):
        fn = C.split("bool set_character_texture(", 1)[1].split("void matrix_rain_filter_defaults(", 1)[0]
        self.assertIn('if (!replacement)', fn)
        self.assertLess(fn.index('if (!replacement)'), fn.index('gs_texture_get_width'))
        self.assertIn('if (old)', fn)
        self.assertIn('gs_texture_get_width', fn)
    def test_min_fade_parameter_has_its_own_guard(self):
        self.assertIn('if (filter->param_min_fade_value)', C)
        render=C.split("void matrix_rain_filter_video_render(",1)[1].split("static void matrix_rain_set_functions",1)[0]
        self.assertIn('if (filter->param_min_fade_value)',render)

if __name__ == "__main__":
    unittest.main()
