#!/bin/bash
# Build the Rust archive for the Vita host.
set -euo pipefail
. "$HOME/.cargo/env"
source_dir=$(cd "$(dirname "$0")" && pwd)
engine=${NGUIDLE_RUST:-$(dirname "$source_dir")}
build=${NGU_VITA_BUILD_DIR:-"$engine/out/vita"}
target=armv7-sony-vita-newlibeabihf
export VITASDK=${VITASDK:-"$HOME/vitasdk"}
export PATH="$VITASDK/bin:$PATH"
export CARGO_TARGET_DIR="$build/cargo"
export CARGO_TARGET_ARMV7_SONY_VITA_NEWLIBEABIHF_LINKER=arm-vita-eabi-gcc
export CC_armv7_sony_vita_newlibeabihf=arm-vita-eabi-gcc
export AR_armv7_sony_vita_newlibeabihf=arm-vita-eabi-ar
cd "$engine"
case ${NGU_VITA_LOGIC:-interpreter} in
    interpreter)
        crate=clr
        profile=release
        panic_runtime=panic_abort
        ;;
    native)
        crate=ngu-gen
        profile=game
        panic_runtime=panic_unwind
        if [ ! -s crates/ngu-gen/src/gen.rs ]; then
            echo 'Generate the game code using the Rust project first.' >&2
            exit 1
        fi
        ;;
    *)
        echo 'NGU_VITA_LOGIC must be interpreter or native.' >&2
        exit 2
        ;;
esac
bash tools/capbuild.sh "${NGU_VITA_BUILD_MEMORY_GB:-12}" cargo build --locked --profile "$profile" \
    --package "$crate" --lib --target "$target" -Z "build-std=std,$panic_runtime"
