# nguidle-vita

PS Vita build of the engine from [nguidle-rust](https://github.com/vallfrr/nguidle-rust).
The Vita host uses vitaGL and the same game scene, save menus and tooltips as
the desktop port. It reflows the interface across the 960 × 544 display.

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
before launching the benchmark. Every five seconds, `ux0:data/ngu/bench/host.txt`
reports frame rate, the median and 95th/99th percentile frame intervals (ms),
and the time spent on logic, scene emission, drawing and display sync. Select
writes a screenshot to `ux0:data/ngu/bench/shot.tga`. Each window reports the
number of screenshots taken so those windows can be excluded from timing
comparisons.

`NGU_VITA_PROFILE=ON` records slow game callbacks in `ux0:data/ngu/log.txt`.
Use the default `OFF` for frame-rate comparisons. The default build requests
444 MHz CPU and 222 MHz GPU clocks and streams one vertex buffer per frame.
For comparisons, `NGU_VITA_FAST_CLOCKS=OFF` requests 333 MHz and 166 MHz,
and `NGU_VITA_VERTEX_BUFFER=OFF` uses client vertex arrays. The startup log
records the actual clocks. `NGU_VITA_NATIVE_OPT=1` enables an experimental
translated build profile. It exceeded the default 12 GiB compiler memory cap
in the full-game build.

## Prepare and install your data

Verify and extract your Steam Windows 1.260 data using the Rust project's
tools. Pack the prepared files and original game assembly from that installation:

```sh
python3 tools/pack.py out/pc /path/to/NGUIdle_Data/Managed/Assembly-CSharp.dll out/vita/ngu.pak
```

Install the VPK and copy `ngu.pak` to `ux0:data/ngu/ngu.pak`. The VPK contains
only the engine and LiveArea assets. With a configured Vita agent, set
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

The interpreter VPK has run on a console. The smoke test opened the slot
picker with the D-pad and Cross, then used touch to load a converted legacy
autosave and enter Basic Training. The game wrote a subsequent autosave.
The benchmark also verified right-stick scrolling, touch inventory dragging
and saving and loading numbered slots. The generated-Rust VPK reached Basic
Training on the same advanced save. The interpreter had more free memory at
startup and presented more frames per second in that test. See the
[engine status](https://github.com/vallfrr/nguidle-rust/blob/main/docs/STATUS.md)
for the latest device results and remaining tests.

## License

GPL-3.0. Supply the game data from your own installation. The LiveArea images
are black placeholders pending the maintainer's artwork.
