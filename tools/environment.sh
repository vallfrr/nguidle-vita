#!/usr/bin/env bash
# Platform paths for both standalone and shared developer workspaces.
NGU_VITA_REPO=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
vita_parent=$(dirname -- "$NGU_VITA_REPO")
if [ "$(basename -- "$vita_parent")" = repos ]; then
    vita_home_default=$(dirname -- "$vita_parent")
else
    vita_home_default=$NGU_VITA_REPO
fi
: "${NGUIDLE_HOME:=$vita_home_default}"
: "${NGU_WORK:=$NGUIDLE_HOME/work}"
: "${NGU_BUILD:=$NGUIDLE_HOME/build}"
: "${NGU_VITA_BUILD_DIR:=$NGU_BUILD/vita/host}"
: "${VITA_AGENT:=$NGUIDLE_HOME/vita-agent/agent.sh}"
vita_sdk_default=$NGUIDLE_HOME/toolchains/vitasdk
[ -d "$vita_sdk_default" ] || vita_sdk_default=$HOME/vitasdk
: "${VITASDK:=$vita_sdk_default}"
export NGUIDLE_HOME NGU_WORK NGU_BUILD NGU_VITA_REPO NGU_VITA_BUILD_DIR VITASDK VITA_AGENT
