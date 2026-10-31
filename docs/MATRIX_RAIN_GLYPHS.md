# Matrix Rain: make your own character set locally

Matrix Rain already accepts a **horizontal PNG atlas**, with one equally
sized character per cell. The texture is white with dark glyphs: the shader
inverts it into a mask and colors the characters itself.

The local, inspectable generator at `scripts/make-matrix-atlas.py` uses
[Pillow](https://pillow.readthedocs.io/) and fonts already on your computer.
No account, upload, browser, cloud service, or external generator is required.

## Use a glyph set immediately (no plugin rebuild)

```bash
# Dependency only for the standalone generator, not the OBS plugin:
uv run --with pillow scripts/make-matrix-atlas.py \
  --characters 'アイウエオカキクケコ0123456789' \
  --font /path/to/a/font-with-these-glyphs.ttf \
  --output "$HOME/Pictures/matrix-kana.png"
```

If `Pillow` is already installed, simply use
`python3 scripts/make-matrix-atlas.py ...`. Omit `--font` for your system
default font (chosen by fontconfig); for Japanese, symbols, and non-Latin
characters, explicitly select a font containing those glyphs. Fonts with
missing characters may display replacement boxes.

In OBS, open **Retro Effects → Matrix Rain**:
1. Set **Character Set** to **Custom Character Texture**.
2. Browse to the generated PNG under **Texture File**.
3. Set **Character Count** to the printed count.

The PNG is independent of the repo. You can generate it anywhere; the
plugin's existing custom-file control loads it without recompiling.

## Register a reusable dropdown option (development source)

To add a named set to the built-in dropdown of a *new build*:

```bash
uv run --with pillow scripts/make-matrix-atlas.py \
  --characters '01ABCDEF+-*/' \
  --output /tmp/matrix-bits.png \
  --register-name 'Bits and Letters'
```

This copies the generated PNG to `data/images/matrix-custom-bits-and-letters.png`
and **appends**, without replacing existing entries, to
`data/presets/matrix-rain.json`. Review the two new assets and rebuild/reinstall
the plugin to expose the new preset. It does not touch the installed OBS data
or running OBS application automatically.

## Options and constraints

- `--characters-file file.txt`: UTF-8 text; line breaks are removed, spaces
  are retained as cells. Multiple codepoints that form a combined emoji or
  shaping sequence are **not** treated as one grapheme.
- `--font /path/file.ttf`, `--font-size 29`, `--cell-width 40`,
  `--cell-height 40`, `--padding 3` adjust typography and layout.
- Maximum 255 glyph cells, or 8192 total texture pixels wide, whichever is
  smaller. Each glyph is centered and separated by clean white gutters to
  reduce neighbor bleed under the current linear texture sampler.
- The tool refuses to overwrite existing generated PNGs or existing
  registered set names. Duplicate characters are retained intentionally.

A built-in **font picker / editable glyph grid inside OBS** is a potential
future UI improvement. This CLI provides the same atlas format without
introducing a Qt or web-renderer dependency into the plugin.
