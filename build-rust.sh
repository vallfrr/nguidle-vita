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
export CARGO_PROFILE_RELEASE_LTO="${CARGO_PROFILE_RELEASE_LTO:-thin}"
export CARGO_PROFILE_RELEASE_CODEGEN_UNITS="${CARGO_PROFILE_RELEASE_CODEGEN_UNITS:-4}"
cargo "+$nightly" build --locked --release --package clr --lib --target "$target" \
    -Z build-std=std,panic_abort --jobs "${NGU_VITA_BUILD_JOBS:-2}"
