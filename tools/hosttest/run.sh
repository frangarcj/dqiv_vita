#!/bin/bash
# Host build of the engine core (no Vita, no GL) with AddressSanitizer: fast edit/test loop for WP1.
#   tools/hosttest/run.sh [pack_dir] [fbx] [fld] [fld2]       needs clang++ (C++20) ; output build-host/selftest
set -e
HERE=$(cd "$(dirname "$0")/../.." && pwd)
OUT=$HERE/build-host; mkdir -p "$OUT"
SRC="$HERE/source/engine/math.cpp $HERE/source/engine/object.cpp $HERE/source/engine/coroutine.cpp $HERE/source/engine/statics.cpp $HERE/source/engine/engine_core.cpp
 $HERE/source/engine/pack_glue.cpp $HERE/source/engine/stubs.cpp $HERE/source/engine/components.cpp $HERE/source/engine/colliders.cpp $HERE/source/engine/animation.cpp
 $HERE/source/engine/selftest.cpp $HERE/source/engine/renderer.cpp $HERE/source/engine/shaders.cpp $HERE/source/engine/assets_gl.cpp $HERE/source/engine/textmesh.cpp $HERE/source/game/FldObject.cpp $HERE/source/game/FbxObject.cpp $HERE/tools/hosttest/host_backend.cpp $HERE/tools/hosttest/host_main.cpp"
clang -c -g -O1 -fsanitize=address,undefined -I"$HERE/source" "$HERE/source/pack/dqiv_pack.c" -o "$OUT/dqiv_pack.o"
clang++ -std=c++20 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -DDEBUG_SOLOADER -Wall -Wno-unused-function -Wno-psabi \
  -DDQIV_DEV_TOOLS -I"$HERE/source" -I"$HERE/include" -I"$HERE/tools/hosttest/shim" $SRC "$OUT/dqiv_pack.o" -o "$OUT/selftest" 2>&1 | grep -E "error|warning: (unused|comparison)|undefined" -A3 | head -80
[ -x "$OUT/selftest" ] || { echo "build failed"; exit 1; }
"$OUT/selftest" "$@"
