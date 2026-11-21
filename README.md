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

Use WSL or Linux with VitaSDK, vitaGL, Python 3 and a Rust nightly with
`rust-src`. Clone this repository inside the Rust checkout as `nguidle-vita/`.

```sh
bash nguidle-vita/build.sh
```

The default build uses the shared CIL interpreter and writes `out/vita/ngu.vpk`.
The optional translated-Rust build requires generated source from your own
installation. Generate it using the Rust project's instructions, then run:

```sh
NGU_VITA_LOGIC=native NGU_VITA_BUILD_DIR="$PWD/out/vita-native" bash nguidle-vita/build.sh
```

`VITASDK` defaults to `$HOME/vitasdk`. Set `NGUIDLE_RUST` if the engine resides
elsewhere. Both builds verify the VPK headers and LiveArea PNG format.

## Benchmark on a console

Use `NGU_VITA_BENCHMARK=ON` when building the VPK. The benchmark reads and
writes saves under `ux0:data/ngu/bench/save/`. Copy a test autosave there
before launching the benchmark. The standard VPK uses `ux0:data/ngu/save/`.
Use a separate output directory for the benchmark package.

```sh
NGU_VITA_BENCHMARK=ON NGU_VITA_BUILD_DIR="$PWD/out/vita-bench-build" \
  bash nguidle-vita/build.sh
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

Use the tools from the Rust checkout. Set `game_data` to the
`NGUIdle_Data` folder from your Steam Windows 1.260 installation. The
preparation dependencies are listed in `tools/requirements-prep.txt`.

```sh
game_data=/path/to/NGUIdle_Data
python3 tools/verify_game_data.py "$game_data"
python3 tools/nguprep.py --game-data "$game_data" --out out/vita-data
python3 tools/pack.py out/vita-data "$game_data/Managed/Assembly-CSharp.dll" out/vita/ngu.pak
```

Install the VPK and copy `ngu.pak` to `ux0:data/ngu/ngu.pak`. Repack the data if
your archive predates the inclusion of `tags.json`, which places the game menus
under the resource bar. The VPK contains only the engine and LiveArea assets.
With a configured Vita agent, set
`VITA_AGENT` to the local wrapper and use `bash nguidle-vita/deploy.sh`.
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
