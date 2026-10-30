#!/bin/bash
# Usage: tools/re/decomp.sh <lib> <out_file> <target>...
#   lib:    dq4core | sdlib
#   target: exact symbol name | ~substring (matches demangled names) | 0xADDR (Ghidra address) | xref:SYMBOL
# NOTE: Ghidra image base is +0x10000 vs readelf/so_util offsets (Ghidra 0x6e2268 == .so offset 0x6d2268).
# The shared project work/re/proj_game is never opened directly (Ghidra project locks, parallel agents): each call works on a
# throw-away copy that is deleted afterwards (no per-agent copies are kept on disk).
LIB=$1; OUT=$2; shift 2
HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$(cd "$HERE/../.." && pwd)/work/re/proj_game
case $LIB in
  dq4core) PROG=libdq4core.so ;;
  sdlib) PROG=libsdlib_android.so ;;
  *) echo "unknown lib $LIB (dq4core|sdlib)"; exit 1 ;;
esac
[ -f "$SRC/dqiv.gpr" ] || { echo "Ghidra project $SRC not ready"; exit 1; }
PD=$(mktemp -d "${TMPDIR:-/tmp}/dqiv-ghidra.XXXXXX"); trap 'rm -rf "$PD"' EXIT
cp -R "$SRC/." "$PD/" && rm -f "$PD"/*.lock*
mkdir -p "$(dirname "$OUT")"
ABS="$(cd "$(dirname "$OUT")" && pwd)/$(basename "$OUT")"
/opt/homebrew/Cellar/ghidra/12.2/libexec/support/analyzeHeadless "$PD" dqiv -process "$PROG" -noanalysis -readOnly \
  -scriptPath "$HERE" -postScript Decompile.java "$ABS" "$@" > "$ABS.log" 2>&1
grep -c "=====" "$ABS" 2>/dev/null || { echo "failed, see $ABS.log"; tail -5 "$ABS.log"; }
