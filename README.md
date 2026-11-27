# nguidle-vita

PS Vita build of the engine from [nguidle-rust](https://github.com/vallfrr/nguidle-rust).
The Vita host uses vitaGL and the same game scene, save menus and tooltips as
the desktop port. It reflows the interface across the 960 × 544 display.

## Install

You need to own NGU Idle on Steam (version 1.260). The game files are never
included: you prepare them yourself from your own copy.

1. Download `nguidle.vpk` and the `nguidle-prep` file for your computer from the
   [latest release](https://github.com/vallfrr/nguidle-vita/releases).
2. Run `nguidle-prep`. It finds NGU Idle in your Steam folders (or lets you pick the
   folder), then asks where to write the result, for example a USB stick.
3. Copy the `nguidle` folder it creates into `ux0:data/` on the Vita.
4. Install `nguidle.vpk` with VitaShell and start NGU Idle from the LiveArea.

Saves live in `ux0:data/nguidle/save/`. The
[Vita port history and hardware audit](https://github.com/vallfrr/nguidle-rust/blob/main/docs/VITA.md)
records the experiments, test results and remaining issues from the first
Vita build onward.

## Controls

- D-pad or left stick moves focus. The focused control shows its in-game tooltip.
- Cross activates the focused control. Circle closes dropdowns and backs out of save menus.
- L and R switch game menus. The right stick scrolls the focused area.
- The touchscreen supports taps and dragging. Text entry uses the Vita keyboard.
- Select saves a screenshot to `ux0:data/nguidle/shot.tga`.

## Build

Use WSL or Linux with VitaSDK, CMake, Make, Rustup, Git, Just and Python
3.11 or later. This checkout fetches the engine through the `engine`
submodule. Its revision must match the tag in `engine.tag`. Setup installs
the pinned Rust nightly, patched vitaGL and standalone Vita client.

```sh
just check
just setup
just doctor
bash build.sh
```

Install Ruff with `python3 -m pip install ruff==0.11.13` in your Python
environment. `just check` validates the engine pin and builds the
interpreter package with `-Wall -Wextra` under `build/vita/check`.
It sets up the build dependencies. Game data and console pairing are
prepared separately. The resulting VPK is `build/vita/host/nguidle.vpk`
when using the default build directory.

`VITASDK` defaults to the workspace SDK or `$HOME/vitasdk`. The build
verifies the VPK headers and LiveArea PNG format. The optional read-only
console check is `bash tools/doctor.sh --probe-console`.

## Benchmark on a console

Use `NGU_VITA_BENCHMARK=ON` when building the VPK. The benchmark reads and
writes saves under `ux0:data/nguidle/bench/save/`. Copy a test autosave there
before launching the benchmark. The standard VPK uses `ux0:data/nguidle/save/`.
Use a separate output directory for the benchmark package.

```sh
source tools/environment.sh
NGU_VITA_BENCHMARK=ON NGU_VITA_BUILD_DIR="$NGU_BUILD/vita/bench" bash build.sh
```

Every five seconds, `ux0:data/nguidle/bench/host.txt`
reports render-loop rate, frame intervals, the longest controller poll and
dispatch, and the time spent on simulation, layout, scene emission, texture uploads,
draw submission and display sync. The display sampler counts framebuffer
changes, repeats and missed samples against the hardware vertical blanks.
The focus probe tracks a changed D-pad target from host dispatch to the
first scanout of its buffer. It reports measurements over two refreshes
and samples lost before scanout.
The log also records simulation time, pending simulation time, CPU time
for the game thread, and free memory both outside and inside vitaGL. Select
writes a screenshot to `ux0:data/nguidle/bench/shot.tga`. Each window reports the
number of screenshots taken so those windows can be excluded from timing
comparisons.

Reproducible measurements: `bash tools/vita-cycle.sh tour <label>` builds, installs, runs the scripted `tour` scenario
and writes `summary.json` and `meta.json` (commits, package, build switches, clocks) under the workspace's
`traces/vita-60/<label>/`. `python3 tools/vita-budget.py <that folder> [--baseline summary.json]` compares it with the
versioned budgets in `budgets.json` and the reference in `perf/` and exits with an error on a regression over the limit; the 16.7 ms goal is reported but not enforced. A performance figure may be published only
with the commit and configuration that `meta.json` records; the counters were repaired in lot 4 (one consistent snapshot
per window), so figures taken before that are historical.

The `[autoplay] check` line at the end of a scenario is a digest of the game's save text. It differs between runs on the console,
because the game reads the wall clock there (`DateTime.Now`); the PC regression runs pin the clock with `NGU_FIXED_NOW`. Compare
digests only between runs with the same fixed clock, never between two console runs.

`NGU_VITA_PROFILE=ON` records slow game callbacks in `ux0:data/nguidle/log.txt`.
Use the default `OFF` for frame-rate comparisons. The default build requests
444 MHz CPU and 222 MHz GPU clocks and streams one vertex buffer per frame.
The interpreter uses thin link-time optimization.
For comparisons, `NGU_VITA_FAST_CLOCKS=OFF` requests 333 MHz and 166 MHz,
and `NGU_VITA_VERTEX_BUFFER=OFF` uses client vertex arrays. The startup log
records the actual clocks.

## Prepare your data from source

`nguidle-prep` and its command-line twin come from the pinned engine and need no Python.
In the Rust checkout, `cargo run --release -p ngu-prep --bin ngu-prepare -- vita GAME_FOLDER OUTPUT_FOLDER`
(GAME_FOLDER is your Steam `NGU IDLE` folder, version 1.260) writes
`OUTPUT_FOLDER/nguidle/ngu.pak`, to copy to `ux0:data/nguidle/ngu.pak`.
The VPK contains only the engine and LiveArea assets.

With a paired Vita agent, run `bash deploy.sh` from this checkout. The wrapper
installed by setup uses its own private client state. Set `VITA_AGENT` to
choose another wrapper. Pass `pak` if you also want to deploy the prepared data.
The deployment script uploads the package as `ux0:data/nguidle/nguidle.vpk`,
replacing the previous one, so that folder holds the game data, that package and
nothing else.
The older `ux0:data/ngu/` folder is no longer used and can be deleted.

## Device verification

The interpreter preview completed a 30-minute advanced-save run on hardware.
The game clock followed wall time and memory stabilized after loading menus
and textures. Seven panels were compared with desktop captures at 960 × 544.
The Vita created and reloaded a test save that the PC also read. Other
hardware checks covered legacy autosave migration, a fresh game, touch
inventory dragging, dropdown and combat controls, and right-stick scrolling.

On the scripted `tour` scenario the render loop averages 59.9 FPS, but the work per frame has a
p99 of about 49 to 51 ms against a 16.7 ms goal, so dense menus still miss refreshes (two runs, 444/222 MHz,
see `perf/tour-baseline.json`). Advanced-save focus feedback exceeds two refreshes (earlier measurement).
Sleep/resume and save naming with the system keyboard still need device
verification. The full [hardware audit](https://github.com/vallfrr/nguidle-rust/blob/main/docs/VITA.md)
includes figures for both runtime variants, known stalls and verification steps.

## License

GPL-3.0. Supply the game data from your own installation.
