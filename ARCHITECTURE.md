# Vita architecture

This checkout owns the VitaSDK host, Vita controller and touch input,
VPK packaging, shell tools and hardware measurements. The `engine`
submodule is pinned by a Git revision and a corresponding engine release
tag in `engine.tag`. Move both together when upgrading the engine.

The build links the engine's C ABI as a Rust static library.
`tools/engine.sh` checks the gitlink, tag and working copy before building.

The engine's original-game verification, extraction and packing tools
prepare a local data archive separately from the VPK. The game loads that
archive and writes to the regular Vita profile. Benchmark builds use their
own profile and record hardware timing separately.

## Relation to the engine's port template
This port follows the layout of the engine's port template (`just new-port` in nguidle-rust): `AGENTS.md`, `engine.tag`,
`justfile` with `check` and `doctor`, the hygiene check and the same GitHub workflow shape. The justified differences are the
platform: the host is C (`main.c`, `host_*.c`) over the engine's C ABI (`engine_api.h`) instead of a Rust binary over
`clr::session::Session`; the engine is a submodule linked as a static library because the Vita toolchain is not the host
toolchain; rendering runs on a separate thread with a triple buffer, which is why the draw list is copied after `ngu_emit`; and CI
runs inside the VitaSDK container. Moving the host onto `Session` is not planned: its timing is tuned on hardware.
