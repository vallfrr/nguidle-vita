# Vita architecture

This checkout owns the VitaSDK host, Vita controller and touch input,
VPK packaging, shell tools and hardware measurements. The `engine`
submodule is pinned by a Git revision and a corresponding engine release
tag in `engine.tag`. Move both together when upgrading the engine.

The normal build links the engine's C ABI as a Rust static library.
The experimental native build also uses the engine converter and its
specialized compilation profile. `tools/engine.sh` checks the gitlink,
tag and working copy before the build.

The engine's original-game verification, extraction and packing tools
prepare a local data archive separately from the VPK. The game loads that
archive and writes to the regular Vita profile. Benchmark builds use their
own profile and record hardware timing separately.
