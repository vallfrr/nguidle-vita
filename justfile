check:
    python3 -B .github/repo-check.py
    bash tools/engine.sh
    bash -n build.sh
    bash -n build-rust.sh
    bash -n tools/vita-cycle.sh
    bash -n setup-tools.sh
    python3 -B -m ruff check --select E9,F63,F7,F82 .github/repo-check.py tools
    python3 -B tools/vita-trace.py --help
    bash setup-tools.sh --build-only
    bash -c 'source tools/environment.sh && NGU_VITA_BUILD_DIR="$NGU_BUILD/vita/check" bash build.sh'

setup:
    bash setup-tools.sh

doctor:
    bash tools/doctor.sh
