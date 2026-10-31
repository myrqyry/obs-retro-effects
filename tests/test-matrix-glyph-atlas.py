#!/usr/bin/env python3
"""Local font-atlas tool contract; no OBS installation required."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

from PIL import Image

SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "make-matrix-atlas.py"
spec = importlib.util.spec_from_file_location("make_matrix_atlas", SCRIPT)
atlas = importlib.util.module_from_spec(spec)
if spec.loader is not None and SCRIPT.exists():
    spec.loader.exec_module(atlas)


class AtlasTests(unittest.TestCase):
    def test_horizontal_strip_with_white_cell_gutters(self):
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp) / "chars.png"
            result = atlas.build_atlas("ABC", target, font=None, font_size=27,
                                       cell_width=36, cell_height=40, padding=3)
            image = Image.open(target).convert("RGB")
            self.assertEqual(image.size, (108, 40))
            self.assertEqual(result, 3)
            self.assertLess(min(image.getpixel((x, y))[0] for x in range(36,72) for y in range(40)), 180)
            for n in range(3):
                for x in [n*36,n*36+1,(n+1)*36-1]:
                    self.assertTrue(all(image.getpixel((x,y))==(255,255,255) for y in range(40)))
    def test_register_appends_without_removing_existing_sets(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root/"data"/"images").mkdir(parents=True)
            (root/"data"/"presets").mkdir()
            registry=root/"data"/"presets"/"matrix-rain.json"
            registry.write_text(json.dumps({"settings_version":"0.0.1",
                                            "textures":[{"name":"Existing","file":"/images/x.png","chars":14}]}))
            source=root/"generated.png"
            atlas.build_atlas("アイウ", source, font=None,
                              font_size=26,cell_width=40,cell_height=40,padding=3)
            atlas.register_atlas(root, "Japanese Test", source, 3)
            entries=json.loads(registry.read_text())["textures"]
            self.assertEqual(len(entries),2)
            self.assertEqual(entries[0]["name"],"Existing")
            self.assertEqual(entries[1],{"name":"Japanese Test","file":"/images/matrix-custom-japanese-test.png","chars":3})
            self.assertTrue((root/"data"/"images"/"matrix-custom-japanese-test.png").is_file())
            with self.assertRaises(ValueError):
                atlas.register_atlas(root,"Japanese Test",source,3)
    def test_rejects_excess_glyphs_and_atlas_width(self):
        with tempfile.TemporaryDirectory() as tmp:
            output=Path(tmp)/"many.png"
            with self.assertRaises(ValueError):
                atlas.build_atlas("x"*256,output,font=None,font_size=25,
                                  cell_width=25,cell_height=40,padding=2)
            with self.assertRaises(ValueError):
                atlas.build_atlas("x"*210,output,font=None,font_size=25,
                                  cell_width=40,cell_height=40,padding=2)
            self.assertFalse(output.exists())

if __name__=="__main__":
    unittest.main()
