#!/bin/bash
# Coverage smoke tests: runs every tools/coverage/<name>.warp (WarpTest script, see game/WarpTest.hpp) one after the other in Vita3K (muted, one emulator at a time),
# keeps screenshots + log extracts in build/coverage/<name>/ and prints a one-line verdict per scenario (counts of STUB / UNPORTED / ERR / DABT / fatal).
#   tools/coverage_run.sh [name ...]       (default: all)       header lines of a .warp:  `#shots 450,650`  `#dur 60`  `#input <script file>`
set -u
HERE=$(cd "$(dirname "$0")/.." && pwd)
FS=/Users/frangar/Fun/vita3k/build/macos/bin/portable/fs/ux0/data/dqiv
SLOT=${SLOT:-$(grep -s "^DQIVN_SLOT:" "$HERE/build/CMakeCache.txt" | cut -d= -f2)}; SLOT=${SLOT:-1}
OUT=$HERE/build/coverage; mkdir -p "$OUT"
NAMES=("$@"); [ ${#NAMES[@]} -eq 0 ] && NAMES=($(cd "$HERE/tools/coverage" && ls *.warp | sed 's/\.warp$//'))
for n in "${NAMES[@]}"; do
  W=$HERE/tools/coverage/$n.warp
  SHOTS=$(sed -n 's/^#shots //p' "$W" | head -1); SHOTS=${SHOTS:-450,650}
  DUR=$(sed -n 's/^#dur //p' "$W" | head -1); DUR=${DUR:-60}
  INP=$(sed -n 's/^#input //p' "$W" | head -1)
  grep -v '^#shots\|^#dur\|^#input' "$W" > "$FS/warp$SLOT.on"
  [ -s "$FS/warp$SLOT.on" ] || rm -f "$FS/warp$SLOT.on"      # a scenario without warp lines starts from the title (cold start, e.g. play_load)
  if [ -n "$INP" ]; then export INPUT_SCRIPT="$HERE/tools/coverage/$INP"; else unset INPUT_SCRIPT; fi
  rm -rf "$OUT/$n"; mkdir -p "$OUT/$n"
  VITA3K_MUTE=1 timeout $((DUR+1800)) "$HERE/tools/run_vita3k.sh" "$DUR" "$SHOTS" > "$OUT/$n/run.out" 2>&1
  cp "$HERE"/build/shots/* "$OUT/$n/" 2>/dev/null
  L=$HERE/build/dqivn.log
  sed -n '/WarpTest/,$p' "$L" | grep -E "STUB|UNPORTED|\[ERR\]|\[FTL\]|\[WRN\]" | sort | uniq -c > "$OUT/$n/issues.txt"
  grep -E "map_name|WarpTest" "$L" | cut -c1-140 > "$OUT/$n/maps.txt"
  D=$(grep -c "DABT" "$HERE/build/vita3k-run.log"); S=$(grep -c "STUB" "$OUT/$n/issues.txt"); U=$(grep -c "UNPORTED" "$OUT/$n/issues.txt")
  E=$(grep -c "\[ERR\]\|\[FTL\]" "$OUT/$n/issues.txt"); N=$(ls "$OUT/$n" | grep -c jpg); F=$(grep -c "^\[INF\] F[0-9]" "$L")
  echo "$n: shots=$N frames=$F DABT=$D STUB=$S UNPORTED=$U ERR=$E"
done
rm -f "$FS/warp$SLOT.on"
