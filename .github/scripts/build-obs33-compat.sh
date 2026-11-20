#!/usr/bin/env bash
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
obs_version="33.0.1"
obs_sha256="9174ee0e3677c92d0d060bdce4c5d44dfee9fd3afddfb6333f098fd029b8de2b"
work="${RUNNER_TEMP:-/tmp}/obs-retro-effects-obs33"
obs_archive="$work/obs-studio-$obs_version.tar.gz"
obs_source="$work/obs-studio-$obs_version"
obs_build="$work/obs-build"
obs_sdk="$work/obs-sdk"
plugin_build="$repo/build_obs33_compat"

rm -rf "$work" "$plugin_build"
mkdir -p "$work"

curl -fsSL --retry 5 \
  "https://github.com/obsproject/obs-studio/archive/refs/tags/$obs_version.tar.gz" \
  -o "$obs_archive"
echo "$obs_sha256  $obs_archive" | sha256sum --check
tar -xzf "$obs_archive" -C "$work"

cmake -S "$obs_source" -B "$obs_build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$obs_sdk" \
  -DOBS_CMAKE_VERSION:STRING=3.0.0 \
  -DOBS_VERSION_OVERRIDE:STRING="$obs_version" \
  -DENABLE_PLUGINS:BOOL=OFF \
  -DENABLE_FRONTEND:BOOL=OFF

cmake --build "$obs_build" --target obs-frontend-api libobs-opengl --parallel
cmake --install "$obs_build" --component Development --config Release --prefix "$obs_sdk"
cmake --install "$obs_build" --component Runtime --config Release --prefix "$obs_sdk"

cmake -S "$repo" -B "$plugin_build" -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_COMPILE_WARNING_AS_ERROR=ON \
  -Dlibobs_DIR="$obs_sdk/lib/cmake/libobs" \
  -DENABLE_FRONTEND_API=OFF \
  -DENABLE_QT=OFF

cmake --build "$plugin_build" --target obs-retro-effects --parallel
test -s "$(find "$plugin_build" -type f \( -name 'obs-retro-effects.so' -o -name 'libobs-retro-effects.so' \) -print -quit)"
echo "PASS: obs-retro-effects builds against OBS $obs_version"
