#!/bin/bash
# Populate Vita3K's emulated memory card (ux0:data/dqiv) for the native port. Idempotent.
#
# ux0:data/dqiv/ is SHARED with the Unity+Mono experiment (port/): lib/, main.obb, assets/ are the same files and
# are normally already there (re/tools_setup_data.sh). This script (re)creates what is missing and ADDS pack/:
#   lib/libdq4core.so lib/libsdlib_android.so   from the APK (re/bin)
#   assets/filelist.bin                         from the extracted APK
#   main.obb                                    symlink (Vita3K follows file symlinks)
#   pack/                                       the converted asset pack (tools/convert output): symlink or --copy
#   saves/ trace/                               created by the app itself
#
# Usage: tools/setup_data.sh [--copy]          (--copy: copy files instead of symlinking, for real hardware staging)
#   DQIV_ROOT=/dir tools/setup_data.sh --copy   stage into another directory (e.g. for FTP to ux0:data/dqiv)
set -euo pipefail
MODE=link; [ "${1:-}" = "--copy" ] && MODE=copy

BASE=/Users/frangar/Fun/dqiv/work
ROOT="${DQIV_ROOT:-/Users/frangar/Fun/vita3k/build/macos/bin/portable/fs/ux0/data/dqiv}"
LIBS="${DQIV_LIBS:-$BASE/re/bin}"
APK_ASSETS="${DQIV_APK_ASSETS:-$BASE/extract/apk/assets}"
OBB="${DQIV_OBB:-$BASE/extract/cache/com.square_enix.android_googleplay.dq4/main.105.com.square_enix.android_googleplay.dq4.obb}"
PACK="${DQIV_PACK:-$BASE/extract/pack}"

mkdir -p "$ROOT/lib" "$ROOT/assets" "$ROOT/saves"

echo "[1] libs -> $ROOT/lib"
for f in libdq4core.so libsdlib_android.so; do
  [ -f "$LIBS/$f" ] || { echo "missing $LIBS/$f" >&2; exit 1; }
  cmp -s "$LIBS/$f" "$ROOT/lib/$f" 2>/dev/null || cp -f "$LIBS/$f" "$ROOT/lib/$f"
done

echo "[2] assets/filelist.bin"
[ -f "$APK_ASSETS/filelist.bin" ] && { cmp -s "$APK_ASSETS/filelist.bin" "$ROOT/assets/filelist.bin" 2>/dev/null || cp -f "$APK_ASSETS/filelist.bin" "$ROOT/assets/filelist.bin"; }

put() {  # put <src> <dst>
  local src="$1" dst="$2"
  if [ "$MODE" = link ]; then
    [ -L "$dst" ] && [ "$(readlink "$dst")" = "$src" ] && return 0
    rm -rf "$dst"; ln -s "$src" "$dst"
  else
    rm -rf "$dst"; mkdir -p "$(dirname "$dst")"
    if [ -d "$src" ]; then
      mkdir -p "$dst"
      for f in index.bin meta.pak mesh.pak tex.pak blob.pak; do cp -f "$src/$f" "$dst/$f"; done
    else
      cp -f "$src" "$dst"
    fi
  fi
}
echo "[3] main.obb ($MODE)"
if [ -e "$ROOT/main.obb" ] && [ ! -L "$ROOT/main.obb" ] && [ "$MODE" = link ]; then echo "    (already a real file, kept)"; else put "$OBB" "$ROOT/main.obb"; fi
echo "[4] pack/ ($MODE)"; put "$PACK" "$ROOT/pack"

echo "[5] cg/ (precompiled vitaGL program blobs of the CG shaders: GRAY/SEPIA; the game recompiles them with libshacccg if missing)"
mkdir -p "$ROOT/cg"; cp -f "$(dirname "$0")"/../extras/cg/*.bin "$ROOT/cg/" 2>/dev/null || true

ls -la "$ROOT" "$ROOT/lib" | head -40
