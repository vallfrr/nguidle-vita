#!/bin/bash
# Builds ngu.vpk: Rust (staticlib) + C host. Output: ~/build/ngu-vita/ngu.vpk
set -e
export VITASDK=${VITASDK:-$HOME/vitasdk}
export PATH=$VITASDK/bin:$HOME/tools/cmake/bin:$HOME/.local/bin:$PATH
bash ${NGUIDLE_VITA:-$(cd "$(dirname "$0")" && pwd)}/build-rust.sh 12
B=$HOME/build/ngu-vita
mkdir -p $B
cmake -S ${NGUIDLE_VITA:-$(cd "$(dirname "$0")" && pwd)} -B $B -DCMAKE_BUILD_TYPE=Release > $B/cmake.log 2>&1 || { tail -20 $B/cmake.log; exit 1; }
cmake --build $B -j8 2>&1 | grep -E "error|undefined|warning: .*implicit|Built target|Linking|\.vpk" | head -40
ls -la $B/ngu.vpk
