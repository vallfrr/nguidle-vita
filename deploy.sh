#!/bin/bash
# Deploys to the Vita through vita-agent: pak (optional: "pak"), VPK, install. Usage: deploy.sh [pak]
A="bash ${VITA_AGENT:?definir VITA_AGENT (script pont vers vita-agent)}"
VPK=$HOME/build/ngu-vita/ngu.vpk
PAK=${NGUIDLE_RUST:?definir NGUIDLE_RUST (clone de nguidle-rust)}/out/ngu_pak/ngu.pak
if [ "$1" = "pak" ]; then
  echo "== upload pak"; $A fs upload $PAK ux0:data/ngu/ngu.pak --overwrite | tail -c 300; echo
fi
echo "== close"; $A app close NGUI00001 2>&1 | tail -c 200; echo
echo "== upload vpk"; $A fs upload $VPK ux0:data/ngu/ngu.vpk --overwrite | tail -c 300; echo
echo "== install"; $A app install ux0:data/ngu/ngu.vpk --yes --timeout 90 2>&1 | tail -c 500; echo
