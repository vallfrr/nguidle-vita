#!/bin/bash
set -e
. ~/.cargo/env
export VITASDK=${VITASDK:-$HOME/vitasdk}
export PATH=$VITASDK/bin:$PATH
cd "$(dirname "$0")"
export CARGO_TARGET_DIR=$HOME/build/ngu_hello   # not under /mnt/j (9p is slow)
cargo build --release
E=$CARGO_TARGET_DIR/armv7-sony-vita-newlibeabihf/release/ngu_hello.elf
B=$CARGO_TARGET_DIR
vita-elf-create $E $B/ngu_hello.velf
vita-make-fself -s $B/ngu_hello.velf $B/ngu_hello.self
vita-mksfoex -s TITLE_ID=NGUI00001 "NGU Hello" $B/ngu_hello.sfo
vita-pack-vpk -s $B/ngu_hello.sfo -b $B/ngu_hello.self $HOME/build/ngu_hello/ngu_hello.vpk
ls -la $HOME/build/ngu_hello/ngu_hello.vpk
