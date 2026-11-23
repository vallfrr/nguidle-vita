#!/usr/bin/env bash
# Installed as vita-agent/agent.sh by setup-tools.sh.
set -euo pipefail
agent_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
: "${VITA_AGENT_CONFIG_DIR:=${XDG_STATE_HOME:-$HOME/.local/state}/ngu-vita-agent}"
export VITA_AGENT_CONFIG_DIR
exec python3 -B "$agent_dir/upstream/client/vita_agent.py" "$@"
