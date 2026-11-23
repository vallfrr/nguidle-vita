#!/bin/bash
# Download the Vita screenshot and convert it to a PNG in work/traces/vitalogs.
set -e
. "$(dirname -- "${BASH_SOURCE[0]}")/tools/environment.sh"
OUT=$NGU_WORK/traces/vitalogs
mkdir -p "$OUT"
bash "$VITA_AGENT" fs download ux0:data/ngu/shot.tga "$OUT/shot.tga" >/dev/null
python3 "$NGUIDLE_RUST/tools/tga2png.py" "$OUT/shot.tga"
