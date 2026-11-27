import importlib.util
import os
from pathlib import Path
import plistlib
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,os.environ["OBS_CI_SCRIPTS"])
spec=importlib.util.spec_from_file_location("hook",Path(__file__).with_name("ci-native.py"))
hook=importlib.util.module_from_spec(spec);spec.loader.exec_module(hook)
class Test(unittest.TestCase):
    def test_native_package_layouts(self):
        module=hook.MODULE
        for target in ("windows-x64","macos-arm64","macos-x86_64"):
            for version in ("32.2.0","33.0.3"):
                with self.subTest(target=target,version=version), tempfile.TemporaryDirectory() as tmp:
                    work=Path(tmp)
                    installed=work/"installed"
                    if target.startswith("macos"):
                        binary=installed/(module+".plugin")/"Contents/MacOS"/module
                        data=binary.parent.parent/"Resources"
                        binary.parent.mkdir(parents=True)
                        (binary.parent.parent/"Info.plist").write_bytes(plistlib.dumps({"CFBundleExecutable":module,"CFBundlePackageType":"BNDL"}))
                        pairs=[(binary,data)]
                    elif module=="obs-retro-effects":
                        pairs=[(installed/module/(module+".dll"),installed/module/"data"),
                               (installed/(module+"_legacy")/"bin/64bit"/(module+".dll"),installed/(module+"_legacy")/"data")]
                    else:
                        pairs=[(installed/"bin/64bit"/(module+".dll"),installed/"data/obs-plugins"/module)]
                    for binary,data in pairs:
                        binary.parent.mkdir(parents=True,exist_ok=True)
                        binary.write_bytes(b"module")
                        for name,value in hook.assets(ROOT).items():
                            path=data/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(value)
                    out=hook.stage(ROOT,installed,work/"payload",target,version)
                    self.assertEqual(out.read_bytes(),b"module")
if __name__=="__main__":unittest.main()
