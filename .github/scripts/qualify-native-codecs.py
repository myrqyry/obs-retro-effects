"""Exercise the actual native FFmpeg libraries pinned by OBS 32.2.0.
No OBS installation, release packaging, or unrelated dependency substitution.
"""
import hashlib
import os
import platform
from pathlib import Path
import subprocess
import sys
import tempfile

BASE = "https://github.com/obsproject/obs-deps/releases/download/2026-07-15/"
REPO = Path(__file__).resolve().parents[2]


def run(*args, env=None):
    subprocess.run([str(arg) for arg in args], check=True, env=env)


def main():
    asset, expected_sha, arch = sys.argv[1:]
    host = platform.system()
    if host not in ("Windows", "Darwin"):
        raise SystemExit(f"Native qualification requires Windows or macOS, got {host}")
    machine = platform.machine().lower()
    expected_machine = {"x64": {"amd64", "x86_64"}, "arm64": {"arm64", "aarch64"}, "x86_64": {"x86_64"}}
    if machine not in expected_machine[arch]:
        raise SystemExit(f"Runner architecture {machine} does not match requested native architecture {arch}")
    with tempfile.TemporaryDirectory(prefix="retro-codecs-") as temp:
        work = Path(temp)
        archive = work / asset
        run("curl", "--fail", "--location", "--retry", "3", BASE + asset, "--output", archive)
        with archive.open("rb") as stream:
            actual = hashlib.file_digest(stream, "sha256").hexdigest()
        if actual != expected_sha:
            raise RuntimeError(f"OBS SDK SHA256 mismatch: {actual}")
        unpacked = work / "sdk"
        unpacked.mkdir()
        # The exact release payload has been checked against the pinned digest.
        run("tar", "-xf", archive, "-C", unpacked)
        roots = {p.parent.parent.parent for p in unpacked.rglob("avcodec.h")
                 if p.parent.name == "libavcodec" and p.parent.parent.name == "include"}
        roots = {p for p in roots if (p / "include/libavutil/avutil.h").is_file()
                 and (p / "include/libswscale/swscale.h").is_file()}
        if len(roots) != 1:
            raise RuntimeError(f"Expected exactly one complete FFmpeg SDK prefix, found {roots}")
        prefix = roots.pop()
        build = work / "build"
        args = ["cmake", "-S", REPO / "tests/codec-native", "-B", build,
                f"-DRETRO_FFMPEG_ROOT={prefix}", "-DCMAKE_BUILD_TYPE=Release"]
        env = os.environ.copy()
        if host == "Windows":
            args += ["-G", "Visual Studio 17 2022", "-A", "x64"]
            env["PATH"] = os.pathsep.join([str(prefix / "bin"), str(prefix / "lib"), env.get("PATH", "")])
        else:
            args += ["-DCMAKE_OSX_ARCHITECTURES=" + arch]
            env["DYLD_LIBRARY_PATH"] = str(prefix / "lib")
        run(*args, env=env)
        run("cmake", "--build", build, "--config", "Release", "--parallel", "4", env=env)
        run("ctest", "--test-dir", build, "-C", "Release", "--no-tests=error", "--output-on-failure", env=env)


if __name__ == "__main__":
    main()
