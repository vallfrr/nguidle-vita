#!/usr/bin/env bash
# Install the pinned vitaGL build and the independent Vita client.
set -euo pipefail
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
default_home=$(dirname -- "$(dirname -- "$source_dir")")
: "${NGUIDLE_HOME:=$default_home}"
: "${NGU_BUILD:=$NGUIDLE_HOME/build}"
: "${VITASDK:=$HOME/vitasdk}"
export VITASDK PATH="$VITASDK/bin:$PATH"
vgl_rev=2bdbe895ddc6a7e410a9ea55a8f53d8173c61ad8
agent_rev=ae140b995977829e582885b2a9bfb25b7d286970
vgl_source="$NGUIDLE_HOME/toolchains/sources/vitaGL"
vgl_build="$NGU_BUILD/vita/vitagl"
agent_dir="$NGUIDLE_HOME/vita-agent"
patch="$source_dir/patches/vitagl.patch"
archive="$vgl_build/libvitaGL.a"
receipt="$vgl_build/libvitaGL.receipt"

digest() { sha256sum < "$1" | cut -d' ' -f1; }
metadata() {
    printf 'source=%s\npatch=%s\ncompiler=%s\narchive=%s\n' \
        "$vgl_rev" "$(digest "$patch")" "$(digest "$VITASDK/bin/arm-vita-eabi-gcc")" "$(digest "$archive")"
}
verify() {
    test -s "$archive" && test -s "$receipt" &&
        test "$(cat "$receipt")" = "$(metadata)" &&
        test "$(git -C "$agent_dir/upstream" rev-parse HEAD)" = "$agent_rev" &&
        cmp -s "$source_dir/tools/agent.sh" "$agent_dir/agent.sh" &&
        bash "$agent_dir/agent.sh" --help >/dev/null &&
        echo "vitaGL $vgl_rev and Vita agent $agent_rev verified"
}
if [ "${1:-}" = --verify ]; then
    verify
    exit
fi
if [ "$#" -ne 0 ]; then
    echo "Usage: bash setup-tools.sh [--verify]" >&2
    exit 2
fi
if [ ! -x "$VITASDK/bin/arm-vita-eabi-gcc" ]; then
    echo "Set VITASDK to the installed VitaSDK." >&2
    exit 1
fi
mkdir -p -- "$NGUIDLE_HOME/toolchains/sources" "$vgl_build" "$agent_dir"
checkout() {
    local remote=$1 directory=$2 revision=$3
    if [ ! -e "$directory" ]; then
        git clone --no-checkout "$remote" "$directory"
        git -C "$directory" checkout --detach "$revision"
    fi
    if [ "$(git -C "$directory" rev-parse HEAD)" != "$revision" ]; then
        echo "Unexpected source revision in $directory" >&2
        return 1
    fi
}
checkout "${NGU_VITA_AGENT_SOURCE:-https://github.com/shoui520/vita-agent-use.git}" "$agent_dir/upstream" "$agent_rev"
install -m 755 "$source_dir/tools/agent.sh" "$agent_dir/agent.sh"
if verify; then
    exit
fi
checkout "${NGU_VITAGL_SOURCE:-https://github.com/Rinnegatamante/vitaGL.git}" "$vgl_source" "$vgl_rev"
if git -C "$vgl_source" apply --check "$patch"; then
    git -C "$vgl_source" apply "$patch"
else
    git -C "$vgl_source" apply --reverse --check "$patch"
fi
make -B -C "$vgl_source" -j"${NGU_VITA_TOOLS_JOBS:-4}"
install -m 644 "$vgl_source/libvitaGL.a" "$archive.pending"
mv -- "$archive.pending" "$archive"
metadata > "$receipt.pending"
mv -- "$receipt.pending" "$receipt"
verify
