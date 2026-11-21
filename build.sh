#!/bin/bash
# Build a VPK. The verified game assets stay in an external ngu.pak.
set -euo pipefail
source_dir=$(cd "$(dirname "$0")" && pwd)
engine=${NGUIDLE_RUST:-$(dirname "$source_dir")}
build=${NGU_VITA_BUILD_DIR:-"$engine/out/vita"}
export NGUIDLE_RUST="$engine" NGU_VITA_BUILD_DIR="$build"
export VITASDK=${VITASDK:-"$HOME/vitasdk"}
export PATH="$VITASDK/bin:$HOME/tools/cmake/bin:$HOME/.local/bin:$PATH"
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
    -DNGU_RUST_LIB="$archive" -DNGU_NATIVE_LOGIC="$native"
cmake --build "$build" -j4
python3 "$source_dir/.claude/skills/vita-port/scripts/check_vpk.py" "$build/ngu.vpk"
printf 'VPK: %s\n' "$build/ngu.vpk"
