ci_dependencies(){
  sudo apt-get install -y --no-install-recommends python3-pil fontconfig fonts-dejavu-core
  /usr/bin/python3 -c 'import PIL'
}
ci_build(){
  cmake -S "$CI_ROOT" -B "$CI_BUILD" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -Dlibobs_DIR="$CI_SDK/lib/cmake/libobs" -DBUILD_TESTING=ON \
    -DPython3_EXECUTABLE=/usr/bin/python3 \
    -DENABLE_VIDEO_CODECS=ON -DENABLE_RUST_DITHER=OFF \
    -DENABLE_QT=OFF -DENABLE_FRONTEND_API=OFF
  cmake --build "$CI_BUILD" --parallel 4
}
ci_test(){
  python3 "$CI_ROOT/tests/test_obs33_compat.py"
  ctest --test-dir "$CI_BUILD" -N -R "^matrix_rain_glyph_atlas$" | grep -E "Test +#[0-9]+: matrix_rain_glyph_atlas$"
  verify_ctest
}
ci_stage(){
  component=Legacy; rel=bin/64bit; folder=obs-plugins
  if [[ "$CI_LAYOUT" == modern ]]; then component=Modern; rel=.; folder=obs-modules/plugins; fi
  cmake --install "$CI_BUILD" --prefix "$CI_WORK/install" --component "$component"
  mapfile -t binaries < <(find "$CI_WORK/install" -type f -name "$MODULE.so")
  [[ "${#binaries[@]}" == 1 ]]
  data="$CI_WORK/install/share/obs/$folder/$MODULE"
  test -d "$data"
  dest="$CI_STAGE/$MODULE"
  mkdir -p "$dest/$rel"
  cp "${binaries[0]}" "$dest/$rel/$MODULE.so"
  cp -a "$data" "$dest/data"
  cp "$CI_ROOT/LICENSE" "$dest/LICENSE"
  python3 - "$CI_ROOT" "$dest/data" <<'PYDATA'
from pathlib import Path
import sys,subprocess
root, data=map(Path,sys.argv[1:])
expected={Path(p).relative_to('data').as_posix():root/p for p in subprocess.check_output(['git','-C',str(root),'ls-files','data'],text=True).splitlines()}
expected['manifest.json']=root/'manifest.json'
actual={p.relative_to(data).as_posix():p for p in data.rglob('*') if p.is_file()}
assert expected.keys()==actual.keys(),expected.keys()^actual.keys()
assert all(p.read_bytes()==actual[n].read_bytes() for n,p in expected.items())
print(f"Verified {len(actual)} installed assets")
PYDATA
}
