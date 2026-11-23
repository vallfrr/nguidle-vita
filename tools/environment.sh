#!/usr/bin/env bash
# Resolve the current engine installation. The tag migration is a separate change.
vita_repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
vita_parent=$(dirname -- "$vita_repo")
if [ -d "$vita_parent/nguidle-rust/tools" ]; then
    ngu_engine_default="$vita_parent/nguidle-rust"
elif [ -d "$vita_parent/../nguvita/tools" ]; then
    ngu_engine_default=$(cd -- "$vita_parent/../nguvita" && pwd -P)
else
    ngu_engine_default=$vita_parent
fi
: "${NGUIDLE_RUST:=$ngu_engine_default}"
if [ ! -r "$NGUIDLE_RUST/tools/environment.sh" ]; then
    echo "Set NGUIDLE_RUST to the engine checkout." >&2
    return 1
fi
. "$NGUIDLE_RUST/tools/environment.sh"
: "${NGU_VITA_BUILD_DIR:=$NGU_BUILD/vita/host}"
vita_sdk_default=$NGUIDLE_HOME/toolchains/vitasdk
[ -d "$vita_sdk_default" ] || vita_sdk_default=$HOME/vitasdk
: "${VITASDK:=$vita_sdk_default}"
export NGUIDLE_RUST NGU_VITA_BUILD_DIR VITASDK
