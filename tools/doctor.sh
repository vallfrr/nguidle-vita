#!/usr/bin/env bash
set -euo pipefail
. "$(dirname -- "${BASH_SOURCE[0]}")/environment.sh"
engine=$(bash "$NGU_VITA_REPO/tools/engine.sh")
printf 'OK  Engine %s %s\n' "$(cat "$NGU_VITA_REPO/engine.tag")" "$(git -C "$engine" rev-parse --short HEAD)"
nightly=$(sed -n 's/^channel = "\(.*\)"$/\1/p' "$NGU_VITA_REPO/rust-toolchain.toml")
rustup run "$nightly" rustc --version
sysroot=$(rustup run "$nightly" rustc --print sysroot)
test -d "$sysroot/lib/rustlib/src/rust/library" || { echo 'Install rust-src with bash setup-tools.sh.' >&2; exit 1; }
"$VITASDK/bin/arm-vita-eabi-gcc" --version | sed -n '1p'
bash "$NGU_VITA_REPO/setup-tools.sh" --verify
case ${1:-} in
    '') ;;
    --probe-console) timeout 20 bash "$VITA_AGENT" app running ;;
    *) echo 'Usage: bash tools/doctor.sh [--probe-console]' >&2; exit 2 ;;
esac
echo 'DOCTOR: OK'
