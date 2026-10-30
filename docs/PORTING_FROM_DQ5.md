# Porting guide: from the Dragon Quest V Vita port to Dragon Quest IV

This repo started as a mechanical rename of the DQ5 port (`/Users/frangar/Fun/dqv/native`, branch `main`, commit e879852). DQ4, DQ5 and DQ6 on Android are the same engine family: a Unity + Mono front end over a native core (`libdq4core.so`) and `libsdlib_android.so`. So this port is **the DQ5 port plus the delta between the DQ5 C# and this game's C#**, not a new reverse-engineering effort.

## Goal
A native, Unity-free Vita port with the same adaptations as DQ5:
- so-loader running `libdq4core.so` and `libsdlib_android.so` unchanged, and the C# front end ported line by line to C++ over mini-Unity and vitaGL.
- Landscape 960x544 **design A** as default: full-width world, UI in a 408x544 panel on the right, dialogues bottom-centre, boot/title flow centred, large battle layout. `layout=split|portrait` kept as options. No sliding panel.
- Full physical-button play with no touch needed (`PadFocus`, `InputName` pad path), touch still working.
- World-map widening (`field_patch`), battle options (`battle_layout`, `battle_zoom`, `battle_speed`, ...), `config.txt`, all shipped languages, original save format.
- Converter (`tools/make_data.py`) and browser converter (`web/`, byte-identical output), precompiled Cg blobs, no `libshacccg` needed, heap sizing, texture budget.
- Dev tooling: warp harness, `DevCheats`, coverage scenarios, soak tools, input scripts.
- End-user README, DEVELOPERS.md, LiveArea art generated from this game's icon, CI workflows.

## Layout
| path | content |
|---|---|
| repo root `/Users/frangar/Fun/dqiv` | the port (git repo, branch `main`) |
| `work/` (gitignored) | `extract/apk`, `extract/cache` (OBB), `extract/pack`, `re/bin/*.so`, `re/cs_out/{Assembly-CSharp,Assembly-CSharp-firstpass}` (ILSpy output), Ghidra project, worktrees |
| `/Users/frangar/Fun/dqv/` | **read-only reference**: `native/` (DQ5 port), `re/cs_out/Assembly-CSharp/` (DQ5 C#), `re/arch/` (design docs, also in `docs/arch-dq5/`), `re/bin/`, `re/scripts/` |
| `/Users/frangar/Fun/vita3k` | emulator, see the `vita3k-test` skill |

The rename also turned DQ5 literals into wrong-but-plausible ones here: anything game-specific (md5, sha256, OBB number, offsets, ids, chapter/map names, comments about DQ5 content) is unverified until you check it against this game.

## Method
1. For a class, `diff /Users/frangar/Fun/dqv/re/cs_out/Assembly-CSharp/X.cs work/re/cs_out/Assembly-CSharp/X.cs`.
2. Apply that same delta to `source/game/X.*`. Same names, same control flow, never improve game logic (see Conventions in DEVELOPERS.md). Classes missing here are removed or disabled; new classes are ported line by line.
3. The native core is **not** the same binary. Exports, packet formats, MenuPort ids, map names and offsets can differ. Every hard-coded address or mangled name (`field_patch.cpp`, `DevCheats.cpp`, `WarpTest.cpp`, `dynlib_table.inc`, `native_bridge.cpp`) must be re-derived from `libdq4core.so` with Ghidra. Check the word before patching, as `field_patch.cpp` does.
4. Reuse DQ5's solved problems. Before debugging anything, read the matching section of DEVELOPERS.md and `docs/arch-dq5/`.

Ghidra headless: `/opt/homebrew/Cellar/ghidra/12.2/libexec/support/analyzeHeadless`, scripts in `tools/re/`; model `/Users/frangar/Fun/dqv/re/decomp.sh`. Create the project once in `work/re/proj_game` and do not copy it per agent. Image base is +0x10000 vs the .so offsets. ILSpy: `DOTNET_ROOT=/opt/homebrew/opt/dotnet/libexec DOTNET_ROLL_FORWARD=Major ~/.dotnet/tools/ilspycmd`. Python for the converter: `/Users/frangar/Fun/dqv/tools/venv/bin/python -I` (do not create another venv unless you test `make_data.py` bootstrap).

## Rules (all agents)
- **Vita3K only; no real console.** `tools/run_vita3k.sh` always runs muted with `VITA3K_JIT_CACHE_MB=16`.
- **One emulator on the whole Mac.** `tools/run_vita3k.sh` takes the machine-wide lock `/tmp/v3k-machine.lock` and waits for it. Never launch Vita3K any other way, never `pkill` an emulator, kill only your own PID. Keep runs short, prefer warp/save starts over replaying intros. A run may queue for minutes: use a Bash timeout of 600000 ms or run it in the background.
- **Disk is nearly full** (about 8 GB free, and macOS swap uses the same disk). Run `df -h /System/Volumes/Data` before heavy work. Below 3 GB free: stop, delete only your own temporary output, and report. No per-agent copies of pack or Ghidra projects. `tools/setup_data.sh` symlinks; keep it that way.
- Do not write in `/Users/frangar/Fun/dqv`, in the other game's repo, or in `/Users/frangar/Fun/vita3k`. Do not touch anything in `~/Downloads`.
- Commit each verified milestone on your branch with a descriptive message ending in `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Never commit game binaries, packs, OBB, APK, `work/` or build dirs. Do not push or create remotes.
- Always read both logs after a run (`build/vita3k-run.log`, `build/dqivn.log`) and look at the screenshots with the Read tool. Report what you actually saw; say plainly what failed or was not tested.
- Work in the worktree and files you were assigned; `DQ?N_SLOT` gives each worktree its own title id.

## Game-specific starting point
Measured on 2026-10-30, against DQ5:
- Android v1.0.5, package `com.square_enix.android_googleplay.dq4`, OBB `main.106.com.square_enix.android_googleplay.dq4.obb` (20 882 files) at `work/extract/cache/com.square_enix.android_googleplay.dq4/`.
- **Unity 4.6.5f1**, not 4.7.2: the converter may meet different serialized layouts.
- `libdq4core.so` is the **older codebase** of the family: 7 896 exports vs 13 088, and many DQ5 C exports are absent (including `FieldSet`, `g_deviceInfo`, `camera_mtx`, the `g_Character*` set), with `g_BattleExec*`, `g_*Part` and others instead. Expect the largest native differences here. All its imports also exist in DQ5's.
- C# 18 358 lines vs 20 578. Not present in DQ4: `CgController`, `Endroll`, `EndrollEx`, `EndrollPort`, `ParticleSystem`, `Sepia`, `SlimeTouchBehaviour`, `SlimeTouchPort`, `Spotlight`, `Volcano`. New in DQ4: `Staffroll`, `StaffrollPort`, `FontSizeDumper`, `Test2`.
- Largest diffs (changed lines): MenuPortItemTable 1047, TouchPad 590, Chara 321, Field 249, MenuParts 203, FldObject 178, InputName 161, FRAME_ENUM 152, Sound 138, NativeManager 136, FieldSymbol 115, RawPads 82, Boot 71, SceneCamera 57, Capture 53, NetConnection 50, Cleaner 43, MenuPortSystem 38.
- `/Users/frangar/Fun/dqv/refs/DQIV-DECOMP` is the NDS DQ4 decompilation and `/Users/frangar/Fun/dqv/refs/DQIV-Recompiled` the PC version that joins both DS screens into 16:9: useful references for this game.
