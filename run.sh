#!/bin/bash
# Launches NGUI00001, waits N seconds, fetches host.txt and log.txt. Usage: run.sh [seconds]
A="bash ${VITA_AGENT:?definir VITA_AGENT (script pont vers vita-agent)}"
OUT=${NGUIDLE_RUST:?definir NGUIDLE_RUST (clone de nguidle-rust)}/out/vitalogs
mkdir -p $OUT
$A fs trash ux0:data/ngu/host.txt >/dev/null 2>&1
$A fs trash ux0:data/ngu/log.txt >/dev/null 2>&1
echo "== launch"; $A app launch NGUI00001 2>&1 | tail -c 400; echo
sleep ${1:-30}
for f in host.txt log.txt; do
  $A fs download ux0:data/ngu/$f $OUT/$f >/dev/null 2>&1
  echo "== $f"; tail -n 40 $OUT/$f 2>/dev/null
done
