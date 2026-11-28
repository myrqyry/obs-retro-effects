# Shared CI qualification

Additive Linux OBS32.2.0/33.0.3 caller pinned to obs-plugin-ci 238f06a.
Preserves existing native package, OBS33.0.1 and codec-native qualification.
Requires the 25 actual codec, shader and libobs render tests with the
FFmpeg gallery enabled, and validates the 35 tracked data assets plus
the root manifest, license and native module.
Local installed OBS32.2.0: 25/25 CTests passed. Staged manifest inventory
initially failed; corrected to account for the existing root manifest.
Fresh stage verified all 36 assets and the 38-file archive. Hosted
OBS33/native SDK qualification pending. Downloadable binary test artifacts enabled; existing release workflow unchanged.

Full matrix extension: pin e9eb704 now selects all eight OBS32/33
native package targets. Linux retains the complete existing test gate.
Windows/macOS use the pinned native SDK with the existing full plugin
features and actual staged module/data/license verification.
The six native layout fixtures pass locally; hosted build and actual
native module load are separate qualifications. Binary uploads enabled.

Shared engine/cache update (73cc0ec): previous hosted full matrix passed
all six native Windows/macOS jobs, but both Linux jobs registered only
24 of 25 CTests because the runner's Python lacked Pillow. Install
python3-pil, fontconfig and DejaVu fonts before the SDK fingerprint;
select /usr/bin/python3 explicitly for CMake and require the named
matrix_rain_glyph_atlas test as well as the unchanged 25-test floor.
Local clean OBS32 build with the pinned shared SDK passed all 25 CTests,
including codec and actual libobs rendering, verified all 36 installed
assets and the 38-file archive. New hosted full matrix and SDK cache
restoration remain to be qualified. Existing release workflows unchanged.
