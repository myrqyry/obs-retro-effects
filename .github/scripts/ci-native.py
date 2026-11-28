import shutil
import subprocess
from pathlib import Path
from native_caller import layout, verify_payload

MODULE = 'obs-retro-effects'
FEATURES = ['-DRETRO_EXTERNAL_SDK=ON', '-DENABLE_VIDEO_CODECS=ON', '-DENABLE_RUST_DITHER=OFF', '-DENABLE_QT=OFF', '-DENABLE_FRONTEND_API=OFF', '-DBUILD_TESTING=OFF']

def configure(root, work, target, version):
    return FEATURES.copy()

def assets(root):
    tracked = subprocess.check_output(["git", "-C", str(root), "ls-files", "data"], text=True).splitlines()
    expected = {Path(p).relative_to("data").as_posix(): (root/p).read_bytes() for p in tracked}
    if MODULE == "obs-retro-effects":
        expected["manifest.json"] = (root/"manifest.json").read_bytes()
    return expected

def stage(root, installed, payload, target, version):
    binary, data, notice = layout(payload,MODULE,target,version)
    if target.startswith("macos-"):
        shutil.copytree(installed/(MODULE+".plugin"),payload/(MODULE+".plugin"))
    else:
        if MODULE == "obs-retro-effects":
            modern = installed/MODULE
            legacy = installed/(MODULE+"_legacy")
            original = legacy/"bin/64bit"/(MODULE+".dll") if version=="32.2.0" else modern/(MODULE+".dll")
            if (legacy/"bin/64bit"/(MODULE+".dll")).read_bytes() != (modern/(MODULE+".dll")).read_bytes():
                raise ValueError("Staged OBS32/33 binaries differ")
            source_data = (legacy if version=="32.2.0" else modern)/"data"
        else:
            original = installed/"bin/64bit"/(MODULE+".dll")
            source_data = installed/"data/obs-plugins"/MODULE
        binary.parent.mkdir(parents=True)
        shutil.copy2(original,binary)
        shutil.copytree(source_data,data)
    license_bytes=(root/"LICENSE").read_bytes()
    notice.write_bytes(license_bytes)
    return verify_payload(payload,MODULE,target,version,assets(root),license_bytes)
