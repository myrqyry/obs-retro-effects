#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
checker = root / "tests/check_source_archive.py"

missing = root / "release/definitely-missing-source.tar.xz"
proc = subprocess.run(
    [sys.executable, str(checker), str(missing)],
    text=True,
    capture_output=True,
)
assert proc.returncode != 0
combined = proc.stdout + proc.stderr
assert "FAIL:" in combined, combined
assert "Traceback" not in combined, combined

defaults = (root / "cmake/linux/defaults.cmake").read_text()
assert "_cpack_source_root_regex" in defaults
for directory in (
    ".deps",
    ".meristem/local",
    "build",
    "build-test",
    "build_test",
    "tests/build",
    "install",
    ".cache",
    "memory",
    ".grepai",
    "release",
):
    normalized = defaults.replace("\\", "")
    expected = '${_cpack_source_root_regex}/' + directory
    assert expected in normalized, f"{directory} exclusion is not anchored to the source root"

workflow = (root / ".github/workflows/build-project.yaml").read_text()
assert "Source archive not found or empty" in workflow
print("PASS: source archive failures are clean and directory ignores are source-root anchored")
