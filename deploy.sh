#!/bin/bash
# Install the project's VPK through an already configured Vita agent bridge.
set -euo pipefail
source_dir=$(cd "$(dirname "$0")" && pwd)
. "$source_dir/tools/environment.sh"
build=$NGU_VITA_BUILD_DIR
package="$build/nguidle.vpk"
agent() { bash "${VITA_AGENT:?Set VITA_AGENT to your local Vita agent wrapper}" "$@"; }
python3 "$source_dir/tools/check_vpk.py" "$package"
# One package file, replaced at each deployment: ux0:data/nguidle holds the data, the package and nothing else.
remote="ux0:data/nguidle/nguidle.vpk"
running=$(agent app running | python3 -c 'import json, sys
reply = json.load(sys.stdin)
if reply.get("status") != "ok":
    raise SystemExit("Could not check running applications")
print(" ".join(app["title_id"] for app in reply["result"]["entries"]))')
case "$running" in
    "") ;;
    NGUI00001) agent app close NGUI00001 ;;
    *) echo "Close the other application before deploying NGU Idle." >&2; exit 1 ;;
esac
if [ "${1:-}" = pak ]; then
    agent fs upload "${NGU_VITA_PAK:-"$build/ngu.pak"}" ux0:data/nguidle/ngu.pak --overwrite
fi
agent fs upload "$package" "$remote" --overwrite
agent app install "$remote" --yes --timeout 90 | python3 -c 'import sys,json
reply=json.load(sys.stdin)
print(json.dumps(reply))
if not reply.get("result",{}).get("installed"):
    raise SystemExit(1)'
