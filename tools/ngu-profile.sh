#!/bin/bash
# ngu-profile.sh <scenario> [top] : replays a benchmark scenario headless on PC (virtual 60 Hz, fixed clock) and prints a short report:
# frame work summary (tools/vita-trace.py) and the `top` methods by exclusive interpreted instructions (hardware-independent cost).
# Output stays in work/traces. Run from WSL with prepared game data.
set -e
. "$(dirname -- "${BASH_SOURCE[0]}")/environment.sh"
engine=$(bash "$NGU_VITA_REPO/tools/engine.sh")
. "$engine/tools/environment.sh"
. ~/.cargo/env
SC=$(realpath "${1:?scenario file}"); TOP=${2:-20}
cd "$engine"
export CARGO_TARGET_DIR="${CARGO_TARGET_DIR:-$NGU_CLR_TARGET}"
(cd clr && cargo build --release -q)
NGU_STATE_DIR=$(ngu_new_state_run profile)
assets=$(ngu_fixture_view)
prefix=$NGU_STATE_DIR/$(basename "$SC" .txt)
mkdir -p "$NGU_STATE_DIR/save"
CLR_PROF=1 NGU_VITA_SAVE_DIR="$NGU_STATE_DIR/save" NGU_FIXED_NOW=1700000000 \
  "$CARGO_TARGET_DIR/release/clr" x autoplay "$assets" "$SC" "$prefix.csv" > "$prefix.prof.txt" 2>&1
grep -a "^\[autoplay\]" "$assets/log.txt" > "$prefix.log" || true
python3 "$NGU_VITA_REPO/tools/vita-trace.py" "$prefix.csv" --scenario "$SC" --log "$prefix.log"
echo "== top $TOP methods (exclusive instr, inclusive, calls)"
sed -n "/^total /,\$p" "$prefix.prof.txt" | head -$((TOP + 1))
