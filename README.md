# nguidle-vita

PS Vita build of the engine from [nguidle-rust](https://github.com/vallfrr/nguidle-rust).
The Vita host uses vitaGL and the same game scene, save menus and tooltips as
the desktop port. It reflows the interface across the 960 × 544 display.

Download [Performance Preview 2](https://github.com/vallfrr/nguidle-vita/releases/tag/vita-performance-preview-2)
and prepare your own data archive below. The
[Vita port history and hardware audit](https://github.com/vallfrr/nguidle-rust/blob/main/docs/VITA.md)
records the experiments, test results and remaining issues from the first
Vita build onward.

## Controls

- D-pad or left stick moves focus. The focused control shows its in-game tooltip.
- Cross activates the focused control. Circle closes dropdowns and backs out of save menus.
- L and R switch game menus. The right stick scrolls the focused area.
- The touchscreen supports taps and dragging. Text entry uses the Vita keyboard.
- Select saves a screenshot to `ux0:data/ngu/shot.tga`.

## Build

Use WSL or Linux with VitaSDK, CMake, Make, Rustup, Git, Just and Python
3.11 or later. This checkout fetches the engine through the `engine`
submodule. Its revision must match engine tag `v0.1.0`. Setup installs
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
It sets up only the build dependencies. Console pairing, game data and
the optional native game build have their own workflow.

The default build uses the shared CIL interpreter and writes `build/vita/host/ngu.vpk`
under the workspace.
The optional translated-Rust build requires generated source from your own
installation. Generate it in the `engine` checkout using the engine
instructions, then run from the Vita repository:

```sh
source tools/environment.sh
NGU_VITA_LOGIC=native NGU_VITA_BUILD_DIR="$NGU_BUILD/vita/native" bash build.sh
```

`VITASDK` defaults to the workspace SDK or `$HOME/vitasdk`. Both builds
verify the VPK headers and LiveArea PNG format. The optional read-only
console check is `bash tools/doctor.sh --probe-console`.

## Benchmark on a console

Use `NGU_VITA_BENCHMARK=ON` when building the VPK. The benchmark reads and
writes saves under `ux0:data/ngu/bench/save/`. Copy a test autosave there
before launching the benchmark. The standard VPK uses `ux0:data/ngu/save/`.
Use a separate output directory for the benchmark package.

```sh
source tools/environment.sh
NGU_VITA_BENCHMARK=ON NGU_VITA_BUILD_DIR="$NGU_BUILD/vita/bench" bash build.sh
```

Every five seconds, `ux0:data/ngu/bench/host.txt`
reports render-loop rate, frame intervals, the longest controller poll and
dispatch, and the time spent on simulation, layout, scene emission, texture uploads,
draw submission and display sync. The display sampler counts framebuffer
changes, repeats and missed samples against the hardware vertical blanks.
The focus probe tracks a changed D-pad target from host dispatch to the
first scanout of its buffer. It reports measurements over two refreshes
and samples lost before scanout.
The log also records simulation time, pending simulation time, CPU time
for the game thread, and free memory both outside and inside vitaGL. Select
writes a screenshot to `ux0:data/ngu/bench/shot.tga`. Each window reports the
number of screenshots taken so those windows can be excluded from timing
comparisons.

`NGU_VITA_PROFILE=ON` records slow game callbacks in `ux0:data/ngu/log.txt`.
Use the default `OFF` for frame-rate comparisons. The default build requests
444 MHz CPU and 222 MHz GPU clocks and streams one vertex buffer per frame.
The interpreter uses thin link-time optimization under the compiler memory cap.
For comparisons, `NGU_VITA_FAST_CLOCKS=OFF` requests 333 MHz and 166 MHz,
and `NGU_VITA_VERTEX_BUFFER=OFF` uses client vertex arrays. The startup log
records the actual clocks. `NGU_VITA_NATIVE_OPT=1` enables an experimental
translated build profile. It exceeded the default 12 GiB compiler memory cap
in the full-game build.

## Prepare and install your data

Use the tools from the pinned engine. Set `game_data` to the
`NGUIdle_Data` folder from your Steam Windows 1.260 installation. The
preparation dependencies are listed in `engine/tools/requirements-prep.txt`.

```sh
game_data=/path/to/NGUIdle_Data
source tools/environment.sh
python3 engine/tools/verify_game_data.py "$game_data"
python3 engine/tools/nguprep.py --game-data "$game_data" --out "$NGU_WORK/prepared/vita"
python3 engine/tools/pack.py "$NGU_WORK/prepared/vita" "$game_data/Managed/Assembly-CSharp.dll" "$NGU_VITA_BUILD_DIR/ngu.pak"
```

Install the VPK and copy `ngu.pak` to `ux0:data/ngu/ngu.pak`. Repack the data if
your archive predates the inclusion of `tags.json`, which places the game menus
under the resource bar. The VPK contains only the engine and LiveArea assets.
With a paired Vita agent, run `bash deploy.sh` from this checkout. The wrapper
installed by setup uses its own private client state. Set `VITA_AGENT` to
choose another wrapper.
Pass `pak` if you also want to deploy the prepared data.
The deployment script names each uploaded VPK by its content hash. Old VPKs
can be removed from `ux0:data/ngu/` after installation to reclaim space.

## Older Vita saves

The earliest Vita build used an internal format. Copy its autosave to your
computer and create a separate compatible file from the Rust checkout.

```sh
cargo run --release -p clr --features legacy-vita-save --bin clr -- /path/to/Assembly-CSharp.dll convert-vita-save old.txt converted.txt
```

The converter checks the legacy checksum and preserves the character and its
timestamp. It creates a new file. Keep the legacy copy and use the converted
file as an autosave or copy it to a numbered slot.

## Device verification

The interpreter preview completed a 30-minute advanced-save run on hardware.
The game clock followed wall time and memory stabilized after loading menus
and textures. Seven panels were compared with desktop captures at 960 × 544.
The Vita created and reloaded a test save that the PC also read. Other
hardware checks covered legacy autosave migration, a fresh game, touch
inventory dragging, dropdown and combat controls, and right-stick scrolling.

The title reaches approximately 60 FPS. Late-game gameplay still misses
refreshes and advanced-save focus feedback exceeds two refreshes.
Sleep/resume and save naming with the system keyboard still need device
verification. The full [hardware audit](https://github.com/vallfrr/nguidle-rust/blob/main/docs/VITA.md)
includes figures for both runtime variants, known stalls and verification steps.

## License

GPL-3.0. Supply the game data from your own installation. The LiveArea images
are black placeholders pending the maintainer's artwork.
