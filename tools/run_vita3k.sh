#!/bin/bash
# Install the freshly built VPK into Vita3K's portable fs, run it headless-ish with automatic screenshots,
# kill it, and collect the logs.   Usage: tools/run_vita3k.sh [duration_s=40] [shot_frames="300,600,900"]
# Output (under build/): vita3k-run.log (emulator log), dqivn.log (our log), vita3k-stdout.log, shots/
#
# Several agents share ONE Vita3K install: this script never `pkill`s anything, it only kills the emulator process it started, and it waits for the machine-wide lock below.
set -u
DUR=${1:-40}
SHOTS=${2:-300,600,900}
HERE=$(cd "$(dirname "$0")/.." && pwd)
V3K=/Users/frangar/Fun/vita3k/build/macos/bin
BIN=$V3K/Vita3K.app/Contents/MacOS/Vita3K
FS=$V3K/portable/fs
SLOT=${SLOT:-$(grep -s "^DQIVN_SLOT:" "$HERE/build/CMakeCache.txt" | cut -d= -f2)}; SLOT=${SLOT:-1}
TITLEID=DQIVN000$SLOT
VPK=${VPK:-$HERE/build/dqivn.vpk}
APPTITLE="Dragon Quest IV"

[ -f "$VPK" ] || { echo "missing $VPK (build first: cd build && make)"; exit 1; }
mkdir -p "$HERE/build"

# NOTE: other agents run `pkill -9 -f "MacOS/Vita3K"` before their runs. The emulator is therefore started with a different argv[0]
# (exec -a v3k-dqivn): their pkill/pgrep patterns do not match it, and we only ever kill our own PID. Two emulators may run at the same time
# (separate title ids, shared portable fs). Consequences: we cannot truncate/copy portable/vita3k.log (shared) - the emulator log is taken from
# our own stdout capture - and screenshots are requested by the app itself (ux0:data/dqiv/dqivn$SLOT.cfg: shot_frames=...) so file names are unique.

# (Re)install the app by unpacking the VPK (it is a zip).
rm -rf "$FS/ux0/app/$TITLEID"
mkdir -p "$FS/ux0/app/$TITLEID"
unzip -q -o "$VPK" -d "$FS/ux0/app/$TITLEID"

# kubridge must be loaded through taiHEN's config.txt or the loader aborts.
if ! grep -q "kubridge.skprx" "$FS/ux0/tai/config.txt" 2>/dev/null; then
  mkdir -p "$FS/ux0/tai"
  printf '*KERNEL\nux0:tai/kubridge.skprx\n' > "$FS/ux0/tai/config.txt"
fi
[ -f "$FS/ux0/tai/kubridge.skprx" ] || { echo "missing $FS/ux0/tai/kubridge.skprx"; exit 1; }

[ -f "$FS/ux0/data/dqiv/lib/libdq4core.so" ] || echo "WARNING: no libdq4core.so in ux0:data/dqiv/lib (run tools/setup_data.sh)"
rm -f "$FS/ux0/data/dqiv/dqivn$SLOT.log"

SHOTDIR="$V3K/portable/screenshots/$APPTITLE"
rm -f "$SHOTDIR"/dqivn${SLOT}_* 2>/dev/null     # only this slot's old shots
# frames at which the app asks the emulator for a screenshot (see game/GameLoop.cpp); SHOT_FRAMES env or 2nd argument (comma list of FRAME NUMBERS)
rm -f "$SHOTDIR"/dqivn${SLOT}_* 2>/dev/null
printf 'shot_frames=%s\n' "$SHOTS" > "$FS/ux0/data/dqiv/dqivn$SLOT.cfg"
# extra cfg lines (e.g. CFG_EXTRA='audiotest=1\nconfirm=circle') and an input script (INPUT_SCRIPT=path; see platform/vita_input.cpp) for headless runs
[ -n "${CFG_EXTRA:-}" ] && printf '%b\n' "$CFG_EXTRA" >> "$FS/ux0/data/dqiv/dqivn$SLOT.cfg"
rm -f "$FS/ux0/data/dqiv/trace$SLOT.on" "$FS/ux0/data/dqiv/replay$SLOT.on"
# TRACE=<frames>: record packets of this run (ux0:data/dqiv/trace<slot>/, copied to build/trace/; check with tools/pktcheck.py).  REPLAY=1: replay the trace of the previous TRACE run (no GameUpdate)
[ -n "${TRACE:-}" ] && { rm -rf "$FS/ux0/data/dqiv/trace$SLOT"; echo "$TRACE" > "$FS/ux0/data/dqiv/trace$SLOT.on"; }
[ -n "${REPLAY:-}" ] && printf 'quit=1\n' > "$FS/ux0/data/dqiv/replay$SLOT.on"
rm -f "$FS/ux0/data/dqiv/input_script$SLOT.txt" "$FS/ux0/data/dqiv/charatest$SLOT.on" "$FS/ux0/data/dqiv/fxtest$SLOT.on"
# CHARATEST="<first CharaTex idx> <count>": boot into the billboard sprite test scene (game/CharaTest.cpp) instead of the title
[ -n "${CHARATEST:-}" ] && echo "$CHARATEST" > "$FS/ux0/data/dqiv/charatest$SLOT.on"
# FXTEST="<battle|darklamp|morning|riremito|tabi|tabirev|spot|torch|sepia|buttons|endroll|slime> [start frame] [arg: endroll index 0-5]": boot into the screen-effect test scene (game/FxTest.cpp)
[ -n "${FXTEST:-}" ] && echo "$FXTEST" > "$FS/ux0/data/dqiv/fxtest$SLOT.on"
[ -n "${INPUT_SCRIPT:-}" ] && cp "$INPUT_SCRIPT" "$FS/ux0/data/dqiv/input_script$SLOT.txt"
# ONE emulator on the whole Mac (16 GB RAM; swap shares the nearly full disk): serialize across all agents and all games.
LOCK=/tmp/v3k-machine.lock
FREE_MB=$(df -m /System/Volumes/Data | awk 'NR==2{print $4}')
[ "$FREE_MB" -lt 2048 ] && { echo "only $FREE_MB MB free on disk: not starting the emulator"; exit 1; }
until mkdir "$LOCK" 2>/dev/null; do
  OWNER=$(cat "$LOCK/pid" 2>/dev/null)
  if [ -n "$OWNER" ] && ! kill -0 "$OWNER" 2>/dev/null; then rm -rf "$LOCK"; continue; fi
  sleep 5
done
echo $$ > "$LOCK/pid"
trap 'rm -rf "$LOCK"' EXIT
while pgrep -f '^v3k-' >/dev/null || pgrep -x Vita3K >/dev/null; do sleep 5; done
START_TS=$(date +%s)

cd "$V3K"
( VITA3K_MUTE=${VITA3K_MUTE:-1} VITA3K_JIT_CACHE_MB=${VITA3K_JIT_CACHE_MB:-16} exec -a v3k-dqivn$SLOT "$BIN" -w -r "$TITLEID" ) > "$HERE/build/vita3k-stdout.log" 2>&1 &
PID=$!
# watchdog: kill OUR process after DUR seconds (also ends early if it exits by itself)
( sleep "$DUR"; kill -9 "$PID" 2>/dev/null ) &
WD=$!
wait "$PID" 2>/dev/null
kill "$WD" 2>/dev/null
echo "emulator exited (pid $PID)"

grep -v "^\[mvk" "$HERE/build/vita3k-stdout.log" > "$HERE/build/vita3k-run.log" 2>/dev/null
cp "$FS/ux0/data/dqiv/dqivn$SLOT.log" "$HERE/build/dqivn.log" 2>/dev/null
rm -rf "$HERE/build/shots"; mkdir -p "$HERE/build/shots"
cp "$SHOTDIR"/dqivn${SLOT}_* "$HERE/build/shots/" 2>/dev/null
if [ -n "${TRACE:-}" ] && [ -d "$FS/ux0/data/dqiv/trace$SLOT" ]; then rm -rf "$HERE/build/trace"; cp -R "$FS/ux0/data/dqiv/trace$SLOT" "$HERE/build/trace"; echo "trace: $HERE/build/trace ($(ls "$HERE/build/trace" | wc -l) frames)"; fi
echo "vita3k log: $HERE/build/vita3k-run.log ($(wc -l < "$HERE/build/vita3k-run.log" 2>/dev/null) lines)"
echo "our log:    $HERE/build/dqivn.log ($(wc -l < "$HERE/build/dqivn.log" 2>/dev/null) lines)"
echo "shots:"; ls -1 "$HERE/build/shots"
