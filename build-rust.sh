#!/bin/bash
# Build the Rust archive for the Vita host.
set -euo pipefail
. "$HOME/.cargo/env"
source_dir=$(cd "$(dirname "$0")" && pwd)
. "$source_dir/tools/environment.sh"
engine=$(bash "$source_dir/tools/engine.sh")
build=$NGU_VITA_BUILD_DIR
nightly=$(sed -n 's/^channel = "\(.*\)"$/\1/p' "$source_dir/rust-toolchain.toml")
test -n "$nightly"
target=armv7-sony-vita-newlibeabihf
export VITASDK=${VITASDK:-"$HOME/vitasdk"}
export PATH="$VITASDK/bin:$PATH"
export CARGO_TARGET_DIR="$build/cargo"
export CARGO_TARGET_ARMV7_SONY_VITA_NEWLIBEABIHF_LINKER=arm-vita-eabi-gcc
export CC_armv7_sony_vita_newlibeabihf=arm-vita-eabi-gcc
export AR_armv7_sony_vita_newlibeabihf=arm-vita-eabi-ar
cd "$engine"
configuration=()
case ${NGU_VITA_LOGIC:-interpreter} in
    interpreter)
        crate=clr
        profile=release
        panic_runtime=panic_abort
        export CARGO_PROFILE_RELEASE_LTO="${CARGO_PROFILE_RELEASE_LTO:-thin}"
        export CARGO_PROFILE_RELEASE_CODEGEN_UNITS="${CARGO_PROFILE_RELEASE_CODEGEN_UNITS:-4}"
        ;;
    native)
        crate=ngu-gen
        profile=game
        panic_runtime=panic_unwind
        if [ ! -s crates/ngu-gen/src/gen.rs ]; then
            echo 'Generate the game code using the Rust project first.' >&2
            exit 1
        fi
        if [ "${NGU_VITA_NATIVE_OPT:-0}" = 1 ]; then
            configuration+=(--config 'profile.game.package.ngu-gen.opt-level=1'
                --config 'profile.game.package.ngu-gen.codegen-units=128' -j2)
        fi
        ;;
    *)
        echo 'NGU_VITA_LOGIC must be interpreter or native.' >&2
        exit 2
        ;;
esac
bash tools/capbuild.sh "${NGU_VITA_BUILD_MEMORY_GB:-12}" cargo "+$nightly" build "${configuration[@]}" --locked --profile "$profile" \
    --package "$crate" --lib --target "$target" -Z "build-std=std,$panic_runtime"
