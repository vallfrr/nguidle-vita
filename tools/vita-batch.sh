#!/bin/bash
# vita-batch.sh label:scenario[:VAR=val,VAR=val] ... : runs several measurements one after the other (tools/vita-cycle.sh),
# prints one comparison table at the end. Example (overnight): vita-batch.sh base:session fast:session:NGU_VITA_FAST_CLOCKS=ON monkey:monkey
cd "$(dirname "$0")/.."
. tools/environment.sh
traces=$NGU_WORK/traces/vita-60
mkdir -p "$traces"
for spec in "$@"; do
  IFS=: read -r label sc vars <<< "$spec"
  log=$traces/$label.txt
  BUILD_VARS=${vars//,/ } bash tools/vita-cycle.sh "$sc" "$label" > "$log" 2>&1 || echo "$label: stopped: $(tail -n 1 "$log")"
done
printf '%-16s %7s %7s %7s %7s\n' label p50 p99 max over
for spec in "$@"; do
  label=${spec%%:*}; f=$traces/$label/summary.json
  [ -f "$f" ] && python3 -c "import json,sys; d=json.load(open(sys.argv[1])); print('%-16s %7.1f %7.1f %7.1f %7d' % (sys.argv[2], d['p50'], d['p99'], d['max'], d['over']))" "$f" "$label"
done
