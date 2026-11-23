#!/bin/bash
# Build a VPK. The verified game assets stay in an external ngu.pak.
set -euo pipefail
source_dir=$(cd "$(dirname "$0")" && pwd)
. "$source_dir/tools/environment.sh"
build=$NGU_VITA_BUILD_DIR
export PATH="$VITASDK/bin:$HOME/tools/cmake/bin:$HOME/.local/bin:$PATH"
vgl=${NGU_VGL_LIB:-"$NGU_BUILD/vita/vitagl/libvitaGL.a"}
if [ ! -s "$vgl" ]; then
    echo "Run tools/setup --vita from the engine checkout to build the patched vitaGL." >&2
    exit 1
fi
bash "$source_dir/build-rust.sh"
target=armv7-sony-vita-newlibeabihf
if [ "${NGU_VITA_LOGIC:-interpreter}" = native ]; then
    archive="$build/cargo/$target/game/libngu_native_vita.a"
    native=ON
else
    archive="$build/cargo/$target/release/libclr.a"
    native=OFF
fi
cmake -S "$source_dir" -B "$build" -DCMAKE_BUILD_TYPE=Release \
    -DNGU_RUST_LIB="$archive" -DNGU_NATIVE_LOGIC="$native" -DNGU_VGL_LIB="$vgl" \
    -DNGU_VITA_BENCHMARK="${NGU_VITA_BENCHMARK:-OFF}" \
    -DNGU_VITA_PROFILE="${NGU_VITA_PROFILE:-OFF}" \
    -DNGU_VITA_FAST_CLOCKS="${NGU_VITA_FAST_CLOCKS:-ON}" \
    -DNGU_VITA_VERTEX_BUFFER="${NGU_VITA_VERTEX_BUFFER:-ON}"
cmake --build "$build" -j4
python3 "$source_dir/.claude/skills/vita-port/scripts/check_vpk.py" "$build/ngu.vpk"
printf 'VPK: %s\n' "$build/ngu.vpk"
