#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
# Use the verified engine supplied by the shared workflow.
engine="${OBS_CI_ENGINE_ROOT:?Missing verified shared engine}"
pin="${ENGINE_REVISION:?Missing immutable shared engine revision}"
[[ "$(git -C "$engine" rev-parse HEAD)" == "$pin" ]]
export OBS_CI_SCRIPTS="$engine/scripts"
python "$root/.github/scripts/test-native-caller.py"
if [[ "$TARGET" == linux-x86_64 ]]; then
  exec bash "$engine/scripts/run-linux-caller.sh" "$root"
fi
exec python "$engine/scripts/native_caller.py" "$root"
