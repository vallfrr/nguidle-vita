#!/bin/bash
# Downloads ux0:data/ngu/shot.tga and converts it to PNG (out/vitalogs/shot.png)
A="bash ${VITA_AGENT:?definir VITA_AGENT (script pont vers vita-agent)}"
OUT=${NGUIDLE_RUST:?definir NGUIDLE_RUST (clone de nguidle-rust)}/out/vitalogs
$A fs download ux0:data/ngu/shot.tga $OUT/shot.tga >/dev/null 2>&1
python3 - <<PY
from PIL import Image
im=Image.open("$OUT/shot.tga"); im.save("$OUT/shot.png"); print(im.size)
PY
