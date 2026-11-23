#!/usr/bin/env bash
# Print the verified source directory of the tagged engine dependency.
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
engine=$repo/engine
tag=$(cat "$repo/engine.tag")
expected=$(git -C "$repo" ls-files --stage -- engine | awk '$1 == "160000" && $3 == 0 {print $2}')
if [[ ! $tag =~ ^v0\.[0-9]+\.[0-9]+$ || ! $expected =~ ^[0-9a-f]{40}$ ]]; then
    echo "Missing or malformed engine pin." >&2
    exit 1
fi
if [ ! -f "$engine/.git" ]; then
    git -C "$repo" submodule update --init -- engine >&2
fi
if ! tagged=$(git -C "$engine" rev-parse --verify --quiet "$tag^{commit}"); then
    git -C "$engine" fetch --no-tags origin "refs/tags/$tag:refs/tags/$tag" >&2
    tagged=$(git -C "$engine" rev-parse "$tag^{commit}")
fi
if [ "$tagged" != "$expected" ] || [ "$(git -C "$engine" rev-parse HEAD)" != "$expected" ]; then
    echo "Engine checkout, gitlink and $tag must point to $expected." >&2
    exit 1
fi
if ! git -C "$engine" diff --quiet HEAD --; then
    echo "The engine dependency has uncommitted tracked changes." >&2
    exit 1
fi
printf '%s\n' "$engine"
