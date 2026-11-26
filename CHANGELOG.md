# Changelog

## [Unreleased]

- Use engine `v0.11.2` (new look of the preparation program: game-style panel, installer with a desktop shortcut). The data folder is now `ux0:data/nguidle` and the VPK `nguidle.vpk`, matching the `nguidle` folder written by the new `nguidle-prep` program. Add the release workflow that builds the VPK and `nguidle-prep` for Windows, Linux and macOS.
- Use the game's main menu picture as the LiveArea background.
- Use engine `v0.10.0`. The host calls `ngu_set_host_tuning` before `ngu_init` (the engine no longer compiles differently for the Vita), and the benchmark profile uses `NGU_SAVE_DIR` and `NGU_PROFILE`. The legacy save converter is now `legacy-save` / `convert-legacy-save`.
- First reproducible measurements of the `tour` scenario (59.9 FPS, p99 work 48.6 to 50.9 ms) and their reference in `perf/`.
- Add versioned performance budgets (`budgets.json`), `tools/vita-budget.py` to check a measurement against them and a baseline, and `meta.json` written by `tools/vita-cycle.sh` so every figure states its commits and configuration.
- Use engine `v0.5.0`; document the engine's C ABI contract in `engine_api.h`; add `AGENTS.md` and relate this port to the engine's port template.

- Use engine `v0.3.0`, including the reference-tested random sequence corrections. Vita performance after the Cooking UI correction has not yet been measured.
- Publish texture queue state before handing uploads to the renderer.
- Give logic and rendering their own benchmark sample counts and report dropped log lines.
- Pin the engine to `v0.2.0`.

- Add local and GitHub host compilation checks with VitaSDK and C warnings enabled.

## [0.1.0]

- Pin the static library build to the engine's `v0.1.0` source revision.
- Own the Vita benchmark and screenshot tools in this repository.
- Retire the bundled Hello prototype. Its source remains in Git history.
