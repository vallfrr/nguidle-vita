#!/bin/bash
# Builds the Rust library clr (staticlib) for the PS Vita.
set -e
. ~/.cargo/env
export VITASDK=${VITASDK:-$HOME/vitasdk}
export PATH=$VITASDK/bin:$PATH
cd ${NGUIDLE_RUST:?definir NGUIDLE_RUST (clone de nguidle-rust)}/clr
export CARGO_TARGET_DIR=$HOME/build/clr-vita
export CARGO_TARGET_ARMV7_SONY_VITA_NEWLIBEABIHF_LINKER=arm-vita-eabi-gcc
export CC_armv7_sony_vita_newlibeabihf=arm-vita-eabi-gcc
export AR_armv7_sony_vita_newlibeabihf=arm-vita-eabi-ar
cargo build --release --lib --target armv7-sony-vita-newlibeabihf -Z build-std=std,panic_abort 2>&1 | grep -E "^(error|warning: unused)|Compiling clr|Finished|could not" | head -${1:-30}
ls -la $CARGO_TARGET_DIR/armv7-sony-vita-newlibeabihf/release/libclr.a
