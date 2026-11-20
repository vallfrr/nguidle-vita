# nguidle-vita

**PS Vita** version of NGU Idle, built on the engine and conversion of [`nguidle-rust`](https://github.com/vallfrr/nguidle-rust).

## State
- **V1 (this repository, today)**: the game's code runs in the engine's interpreter (`clr`, built as a static library), vitaGL rendering, touch screen = mouse. Verified on a console: installation, title screen, ~18 img/s at 333/166 MHz, start-up ~25 s.
- **V2 (planned)**: converted-to-Rust game code (much faster), GPU rendering, buttons + touch, an interface readable on 5 inches. Planned workflow: a small "preparer" tool on PC builds, from your own copy of the game, the data folder to copy to the SD card (+ the VPK).

## Build (V1, WSL + VitaSDK)
Variables: `VITASDK` (default `~/vitasdk`), `NGUIDLE_RUST` (clone of nguidle-rust with `out/` prepared), `VITA_AGENT` (bridge script to vita-agent, for deployment).
- `build.sh`: Rust library + C executable + VPK. `deploy.sh [pak]`, `run.sh <s>`, `shot.sh`: deploy, launch, capture.
- Data: `python tools/nguprep.py` then `python tools/pack.py` in nguidle-rust -> `ngu.pak` to copy to `ux0:data/ngu/`.
- `hello/`: minimal Rust std test on Vita.

No game file is provided or published here. The LiveArea images are black placeholders until official assets are added. License: GPL-3.0.
