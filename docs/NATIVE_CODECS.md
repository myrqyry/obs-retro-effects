# Native codec qualification

The historical profiles belong in Retro Effects' Codec effect; framerot
uses the same codec family for a distinct datamosh instrument. The goal
is to qualify the dependency contract once, then reuse it for both plugins.

## Build dependency contract

With ENABLE_VIDEO_CODECS=ON, Linux defaults to the system FFmpeg package
via pkg-config. An optional RETRO_FFMPEG_ROOT points at a single SDK prefix
with include/libavcodec, include/libavutil, include/libswscale and
corresponding libraries. Windows/macOS use the existing OBS bootstrap
prefixes when no explicit root is supplied. A missing or incomplete
explicit root is an error, never permission to use unrelated dependencies.
Use an SDK matching the actual OBS runtime; a successful standalone
codec test does not establish OBS module ABI or loader compatibility.

The shared CMake target retro_ffmpeg links the same three libraries for the
plugin and codec tests. ffmpeg-compat.h selects the public pixel-format
query for newer SDKs and handles the FFmpeg MPEG-4 profile constant rename.
No codec is emulated or replaced when its real encoder is unavailable.

## Independent native codec test

From the repository root:

    cmake -S tests/codec-native -B build-codec-native -DRETRO_FFMPEG_ROOT=/path/to/sdk -DCMAKE_BUILD_TYPE=Release
    cmake --build build-codec-native --config Release
    ctest --test-dir build-codec-native -C Release --no-tests=error --output-on-failure

Ensure the matching runtime libraries are available to the test executables
(e.g. PATH on Windows, DYLD_LIBRARY_PATH on macOS, LD_LIBRARY_PATH on
Linux when using an isolated SDK). Do not copy a different FFmpeg ABI into
an existing OBS installation.

The Native codec qualification workflow pins and checks the SHA-256 of the
official OBS 32.2.0 dependency archives (obs-deps 2026-07-15), with Windows
x64, macOS Apple Silicon and macOS Intel jobs. Each native runner builds
and executes the independent real codec and bounded worker tests, including all eight
historical encoders, MPEG-4 profile/SAR, MJPEG quantizer behavior and
Microsoft Video 1 channel order, queue overload and failure backoff. This workflow intentionally creates no
OBS plugin packages.

OBS 32.2.0 dependency pin: https://github.com/obsproject/obs-studio/blob/32.2.0/CMakePresets.json
Dependency archives: https://github.com/obsproject/obs-deps/releases/tag/2026-07-15

## Verification and open work

- Linux: standalone real-codec tests pass with system FFmpeg 6 and an
  isolated FFmpeg 8.0 build. The full codec-enabled OBS suite covers the
  historical switch, rendering and returning to editable settings.
- Native Windows/macOS workflow: prepared, not yet run. Matching the native
  OBS SDK, verifying plugin dependencies and module load, exercising
  Direct3D/macOS rendering, and packaging remain separate acceptance gates.
- Retro Effects now supports full-source framing, optional 4:3 crop,
  wet mix, live alpha and a standard C++17 bounded worker. The native
  codec test project exercises the worker with both Linux FFmpeg 6 and
  isolated FFmpeg 8. Native Windows/macOS runs remain unverified.
  The OBS graphics path is not qualified on those platforms; sustained
  streaming, actual user-scene comparison and HDR are still open.
  Keep framerot's Codec Crush until the replacement is accepted.

The native qualification path is deliberately not part of the existing
release workflow until native evidence is available.
