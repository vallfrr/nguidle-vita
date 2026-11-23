check:
    bash tools/engine.sh
    bash -n build.sh
    bash -n build-rust.sh
    bash -n tools/vita-cycle.sh
    python3 -B tools/vita-trace.py --help

setup:
    bash setup-tools.sh

doctor:
    bash tools/doctor.sh
