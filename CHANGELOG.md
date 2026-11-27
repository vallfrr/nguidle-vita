# Changelog

## [Unreleased]

- Console mode on the console (M5): the host hands the raw buttons and sticks to the engine (`ngu_gamepad`), which decodes them. The mode is always on, scale 120 % by default (100 to 130 in Settings, page 2), Start opens Settings, Select opens Info 'n Stuff, triangle shows or hides the information, square is the right click, R held gives the game's keyboard shortcuts (R + L changes the page of the adventure skills), the right stick moves a free cursor, a number pad in the game's style replaces the system keyboard for numbers, the 4G's Sellout Shop button is in Info 'n Stuff, and the tutorial prompts show the buttons. Engine `v0.12.7`. The build option `NGU_VITA_SLICE_MS` (milliseconds of game simulation per display frame, 0 = off) is available for measurements.
- The LiveArea shows no start button frame: the NGU IDLE logo (white with a black outline) is a clickable item that starts the game (style `ad0`, `target psla:eboot`, as in other working packages). Use engine `v0.12.4`: the time a console was asleep is applied without the "You were offline" screen.
- LiveArea art: the bubble icon (the game's yellow-framed logo), the start button (the Steam logo without background, white letters with a black outline) and a launch picture (`sce_sys/pic0.png`, the main menu picture at 960x544). `deploy.sh` uploads the package to `ux0:data/nguidle/nguidle.vpk`, installs it and removes it again, so the data folder holds only the game data, the saves and the log.
- Use engine `v0.12.3`: when the console comes out of sleep (or the game was suspended), the time away is applied as the game's own offline progress, like after closing and opening the game. Measured on the console: 81 ms (the maintainer's PC save) to 309 ms (late-game state, a year) with the scenario `scenarios/away.txt` (`<tick> away <seconds>`); not yet tried with a real suspend.
- Use engine `v0.12.2`: float arithmetic and `GetComponentInChildren` behave like the original game (found by comparing with it), offline progress is tested against it. The engine no longer converts old internal-format Vita saves (nobody used that format), so the README section about them is removed. Measured on the console (`tour`, 444/222 MHz): 59.9 FPS, work per frame p99 48.4 ms against 48.6 ms before, no crash; the game data was prepared with `ngu-prepare vita`.
- Use engine `v0.11.4` (the preparation program uses the game's own sprites, a system folder dialog, fixed window sizes, the system dialog for the output folder and installs itself with a desktop shortcut). The data folder is now `ux0:data/nguidle` and the VPK `nguidle.vpk`, matching the `nguidle` folder written by the new `nguidle-prep` program. Add the release workflow that builds the VPK and `nguidle-prep` for Windows, Linux and macOS.
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
