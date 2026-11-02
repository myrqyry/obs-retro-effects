#!/usr/bin/env python3
from pathlib import Path
root = Path(__file__).resolve().parents[1]
workflow = (root / ".github/workflows/obs33-compatibility.yml").read_text()
builder = (root / ".github/scripts/build-obs33-compat.sh").read_text()
assert "OBS 33.0.1" in workflow
assert "ubuntu-26.04" in workflow
assert "libopengl-dev" in workflow
assert 'obs_version="33.0.1"' in builder
print("PASS: retro-effects OBS33 compatibility gate pins stable 33.0.1 with explicit OpenGL dev dependency")

assert "--target obs-frontend-api libobs-opengl" in builder, (
    "OBS33 SDK install must build frontend API before Development install"
)
