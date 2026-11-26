#!/bin/bash
# Launches NGUI00001, waits N seconds, fetches host.txt and log.txt. Usage: run.sh [seconds]
. "$(dirname -- "${BASH_SOURCE[0]}")/tools/environment.sh" || exit
agent() { bash "$VITA_AGENT" "$@"; }
OUT=$NGU_WORK/traces/vitalogs
mkdir -p "$OUT"
agent fs trash ux0:data/nguidle/host.txt >/dev/null 2>&1
agent fs trash ux0:data/nguidle/log.txt >/dev/null 2>&1
echo "== launch"; agent app launch NGUI00001 2>&1 | tail -c 400; echo
sleep "${1:-30}"
for f in host.txt log.txt; do
  agent fs download "ux0:data/nguidle/$f" "$OUT/$f" >/dev/null 2>&1
  echo "== $f"; tail -n 40 "$OUT/$f" 2>/dev/null
done
