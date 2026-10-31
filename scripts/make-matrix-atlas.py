#!/usr/bin/env python3
"""Generate a locally rendered, horizontal glyph atlas for Retro Effects Matrix Rain.

Requires Pillow and an installed TrueType/OpenType font; no web service.
The plugin interprets a WHITE background with DARK glyphs as its mask.
Each glyph must occupy exactly one equal-width cell.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from PIL import Image, ImageDraw, ImageFont

MAX_GLYPHS = 255
MAX_TEXTURE_WIDTH = 8192


def choose_font(font: str | None, size: int) -> ImageFont.FreeTypeFont:
    if size <= 0:
        raise ValueError("font size must be positive")
    if font:
        path = Path(font).expanduser()
        if not path.is_file():
            raise ValueError(f"Font not found: {path}")
        return ImageFont.truetype(str(path), size)
    try:
        result = subprocess.run(["fc-match", "-f", "%{file}", "sans"],
                                check=True, capture_output=True, text=True)
        path = result.stdout.strip()
        if path and Path(path).is_file():
            return ImageFont.truetype(path, size)
    except (OSError, subprocess.CalledProcessError):
        pass
    return ImageFont.truetype("DejaVuSans.ttf", size)


def build_atlas(chars: str, output: Path, *, font: str | None,
                font_size: int, cell_width: int, cell_height: int, padding: int) -> int:
    if not chars or not chars.strip() or "\n" in chars or "\r" in chars:
        raise ValueError("Provide 1–255 characters in a single line (spaces are allowed)")
    count = len(chars)
    if count > MAX_GLYPHS:
        raise ValueError(f"Too many characters: {count}; max {MAX_GLYPHS}")
    if cell_width < 8 or cell_height < 8 or padding < 1 or padding * 2 >= min(cell_width, cell_height):
        raise ValueError("Invalid cell width, height, or padding")
    width = cell_width * count
    if width > MAX_TEXTURE_WIDTH:
        raise ValueError(f"Atlas width {width} exceeds {MAX_TEXTURE_WIDTH}; use narrower cells/fewer glyphs")
    output = Path(output).expanduser()
    if output.exists():
        raise ValueError(f"Refusing to overwrite existing file: {output}")
    font_obj = choose_font(font, font_size)
    atlas = Image.new("RGB", (width, cell_height), "white")
    for index, char in enumerate(chars):
        cell = Image.new("RGB", (cell_width, cell_height), "white")
        brush = ImageDraw.Draw(cell)
        box = brush.textbbox((0, 0), char, font=font_obj)
        glyph_w, glyph_h = box[2] - box[0], box[3] - box[1]
        x = (cell_width - glyph_w) / 2 - box[0]
        y = (cell_height - glyph_h) / 2 - box[1]
        # Independently clipped cell prevents the next glyph bleeding in.
        brush.text((x, y), char, font=font_obj, fill="black", stroke_width=0)
        if glyph_w > cell_width - padding * 2 or glyph_h > cell_height - padding * 2:
            raise ValueError(f"Glyph {index + 1} ({char!r}) does not fit its cell; increase cell dimensions")
        # Keep a clean white gutter for the plugin's linear atlas sampler.
        for px in range(cell_width):
            for py in range(cell_height):
                if px < padding or px >= cell_width - padding or py < padding or py >= cell_height - padding:
                    cell.putpixel((px, py), (255, 255, 255))
        atlas.paste(cell, (index * cell_width, 0))
    output.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(output, format="PNG")
    return count


def register_atlas(repo_root: Path, name: str, source_png: Path, count: int) -> Path:
    """Optional dev-only operation: append to shipped dropdown, never replace entries."""
    repo_root = Path(repo_root)
    registry = repo_root / "data/presets/matrix-rain.json"
    info = json.loads(registry.read_text(encoding="utf-8"))
    presets = info.get("textures")
    if not isinstance(presets, list):
        raise ValueError(f"Invalid Matrix Rain texture registry: {registry}")
    slug = re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")
    if not slug:
        raise ValueError("Name needs a letter or digit")
    if any(entry.get("name", "").casefold() == name.casefold() for entry in presets):
        raise ValueError(f"Character set already registered: {name}")
    file_name = f"matrix-custom-{slug}.png"
    dest = repo_root / "data/images" / file_name
    if dest.exists():
        raise ValueError(f"Character set image already exists: {dest}")
    if not source_png.is_file():
        raise ValueError(f"Atlas not found: {source_png}")
    if not 1 <= count <= MAX_GLYPHS:
        raise ValueError("Character count must be in 1..255")
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source_png, dest)
    presets.append({"name": name, "file": f"/images/{file_name}", "chars": count})
    # Replace the registry atomically, preserving every pre-existing entry.
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=registry.parent,
                                     prefix=".matrix-", suffix=".json", delete=False) as tmp:
        temp_path = Path(tmp.name)
        json.dump(info, tmp, indent=2, ensure_ascii=False)
        tmp.write("\n")
    try:
        os.replace(temp_path, registry)
    finally:
        temp_path.unlink(missing_ok=True)
    return dest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    chars = parser.add_mutually_exclusive_group(required=True)
    chars.add_argument("--characters", help="Characters to render (one codepoint per cell)")
    chars.add_argument("--characters-file", type=Path, help="UTF-8 file, newline-separated lines are concatenated")
    parser.add_argument("--font", help="Local TTF/OTF path; default: fontconfig sans font")
    parser.add_argument("--font-size", type=int, default=29)
    parser.add_argument("--cell-width", type=int, default=40)
    parser.add_argument("--cell-height", type=int, default=40)
    parser.add_argument("--padding", type=int, default=3)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--register-name", help="Optionally append atlas to shipped plugin dropdown (requires rebuild/reinstall)")
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    selected = args.characters if args.characters is not None else args.characters_file.read_text(encoding="utf-8").replace("\n", "")
    try:
        count = build_atlas(selected, args.output, font=args.font, font_size=args.font_size,
                            cell_width=args.cell_width, cell_height=args.cell_height, padding=args.padding)
        print(f"Created {args.output.expanduser()} ({count} glyphs, {count * args.cell_width} × {args.cell_height})")
        print(f"Use OBS Matrix Rain → Character Set: Custom Character Texture → Texture File: {args.output.expanduser()}")
        print(f"Set Character Count: {count}")
        if args.register_name:
            result = register_atlas(args.repo_root, args.register_name, args.output.expanduser(), count)
            print(f"Registered {args.register_name!r} in the plugin sources ({result}); rebuild/install plugin to show it.")
    except (ValueError, OSError) as exc:
        parser.error(str(exc))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
