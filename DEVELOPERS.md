# Dragon Quest IV - native PS Vita port: developer documentation (end-user README: README.md)

Android "Dragon Quest IV" v1.0.5 (Unity 4.6.5f1 + Mono front end, `libdq4core.so` game logic) rebuilt as a **native Vita app**:

* `libdq4core.so` (all game logic) and `libsdlib_android.so` (audio, OpenSL ES) are loaded with the so-loader and driven
  through their narrow C interface (`GameUpdate(pad)` + int/float packet arrays + the MenuPort mailbox).
* Everything Unity/Mono did - ~18.4k lines of C# that turn those packets into scenes, UI and effects - is ported line by line to C++20 on top of a
  small **mini-Unity** engine (`include/miniunity*`) that renders with vitaGL.
* Assets come from the converted pack (`tools/convert`, or the browser converter in `web/`), not from Unity files.

This repo started as a mechanical rename of the Dragon Quest V port; DQ4, DQ5 and DQ6 are the same engine family, so the DQ4 port is the DQ5 port plus the delta
between the two C# code bases and the two native cores. How to apply that delta: [docs/PORTING_FROM_DQ5.md](docs/PORTING_FROM_DQ5.md). What is disabled / different / open for DQ4:
[docs/TODO_DQ4.md](docs/TODO_DQ4.md) (authoritative). The design documents of the DQ5 port (frame loop, packet protocol, mini-Unity contract, shaders, porting map, input and
layout, landscape) are kept as the **design reference** in [docs/arch-dq5/](docs/arch-dq5/) (`README.md` indexes 01..07); they describe DQ5 numbers (opcode table, buffer sizes, ids) where they differ from DQ4.
Where a section below still quotes DQ5 measurements or ids it says so.

## Status (2026-10-30)

"Works" below means seen in Vita3K (`tools/run_vita3k.sh`, headless); the real console was never tried.

* so-loader: libdq4core + libsdlib_android load, link (0 unresolved imports), run their constructors and `JNI_OnLoad` (`IsLoadNativeCode() = 1`).
* boot sequence of the C# `Boot`/`MainLoop` as in DQ4: the asset table is built from the OBB zip directory (`native/asset_manager.c`; DQ4 has no `filelist.bin`), `DevInit` with the 3-argument DQ4 signature,
  `MenuPortInit`, `GameInit` with the static packet arrays, then **`GameUpdate` every frame at 30 Hz** followed by `UnityRenderEvent(1)`; the DQ5-only exports (`ResetNativePacket`, `FieldSet`, `ParticleSystem*`, `LoadFileList`) are optional bindings.
* `NativeManager::Update` parses the 38 native->host commands of DQ4 (table in docs/TODO_DQ4.md: ops 0..37, `fld_load` ... `bgm`); `tools/pktcheck.py` knows the DQ4 table.
* world-map tile block widened to 59 columns by `native/field_patch.cpp` (three ARM immediates of `FieldData::drawBlock`, each checked before writing, logged at start-up).
* title scene: Boot logos (`Title/sqex`, `Title/arte_logo720_240`), TapToStart, backdrop, start menu ("Create a new adventure log", Licence, Cloud Save), new-game name entry; reachable with buttons only
  (`tools/padonly_dq4.txt`); saves are written by the native (`saves<slot>/data1..5.dat` 15 360 bytes, `data9.dat` 512 bytes seen in Vita3K). Audio runs (OpenSLES, no underruns in a 3600-frame title run).
* UI: the 81-entry DQ4 `MenuPortItemTable` (ids 0..80) is ported; ids 1..80 except 55 and 74 were built with sample data and screenshotted (checklist below).
* Delta ports done (build and title only): Chara, Field, FldObject, FieldSymbol, Riseup, FbxObject, MeshChara, Capture, Sound (partly), Staffroll + StaffrollPort, CharaSleep shaders.
* **Not verified for DQ4**: any real play (towns, dungeons, world map, battles, events, staff roll), Korean/Chinese text, `layout=split|portrait`, DQ5-derived battle/dialogue options, long-session stability (the soak numbers below are DQ5's), the real console.
* Disabled until re-derived from `libdq4core.so` (`NativeCall::kDevHooksVerified`): `DevCheats`, `WarpTest`, `BattleTest`.

> **End users**: see `README.md` (`tools/make_data.py` or the page in `web/` builds `ux0:data/dqiv` from an APK+OBB; Release builds have no developer tools, `-DDQIV_DEV_TOOLS=ON` / Debug enables test scenes and scripted UI).

## Build / run

```sh
export VITASDK=/usr/local/vitasdk PATH=$VITASDK/bin:$PATH
git submodule update --init           # lib/vitaGL (pinned at 76f43e6)
mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make -j10     # -> build/dqivn.vpk, eboot.bin
cd .. && tools/setup_data.sh          # once: libs/obb/pack/cg into Vita3K's ux0:data/dqiv (symlinks to work/; --copy for hardware staging)
tools/run_vita3k.sh 40 "300,600"      # install VPK, run 40 s, screenshots at frames 300 and 600
```

`run_vita3k.sh` leaves `build/dqivn.log` (our log), `build/vita3k-run.log` (emulator log), `build/vita3k-stdout.log`, `build/shots/dqivn_fNNNNN.jpg`. **Always read both logs.**
Several agents share one Vita3K install: the script never `pkill`s; it starts the emulator with `exec -a v3k-dqivn` so another agent's `pkill -f MacOS/Vita3K` does not hit it,
and screenshots are requested by the app itself at the given *frame numbers* (`ux0:data/dqiv/dqivn.cfg`, `VITA3K_SCREENSHOT` printf) so file names are unique.

* Debug builds log everything (`l_debug/info/warn/success`, file reopened per line so a crash loses nothing); Release only errors. `-DDQIV_FAST_BOOT=OFF` restores the real logo timing (1 s / 3 s / 1 s).
* Packet trace: create `ux0:data/dqiv/trace.on`, run, then `tools/pktdump.py <fs>/ux0/data/dqiv/trace` (per-frame summary, flags desynchronised streams).
* `tools/check_imports.py` checks `source/loader/dynlib_table.inc` against `refs/imports_*.txt`. `tools/convert/` = pack converter (Python; developers use `/Users/frangar/Fun/dqv/tools/venv/bin/python -I`, `tools/convert/stage_unity.py` stages `work/extract/unity` from the extracted APK/OBB), format in `tools/convert/FORMAT.md`; `web/` = TypeScript port of the same converter that runs in the browser (end-user setup page, see `web/README.md`; it must stay byte-identical to `tools/convert`: after changing either, run `node web/test/convert_node.mjs` and compare), `make -C tools/convert test PACK=...` validates a pack with the runtime loader.
* Data layout on the Vita (`ux0:data/dqiv/`): `lib/*.so`, `main.obb`, `assets/ct.inf` (only file of the APK's `assets/`; DQ4 has no `filelist.bin`), `pack/` (~190 MB: `index.bin`, `meta.pak`, `mesh.pak`, `tex.pak`, `blob.pak`), `cg/*.bin`, `config.txt`, `saves<slot>/` (DevInit save dir), `dqivn<slot>.log`, `dqivn<slot>.cfg`, optional `trace<slot>.on`, `trace<slot>/`.
* Layout of the development tree (`work/` is gitignored): `work/extract/apk` (extracted APK), `work/extract/cache/com.square_enix.android_googleplay.dq4/main.106....obb`, `work/extract/unity` (staged converter input), `work/extract/pack` (the reference pack, SHA-256 in `tools/make_data.py`), `work/re/bin` (the `.so` files), `work/re/cs_out/{Assembly-CSharp,Assembly-CSharp-firstpass}` (ILSpy output), `work/wt-*` (worktrees). `tools/setup_data.sh` symlinks these into Vita3K.
* Constants (paths, load addresses, 408x544 logical size, language, device info) are in `source/config.h`.

### Screen layout (landscape, docs/arch-dq5/07_LANDSCAPE.md)

Runtime setting `layout=landscape|split|portrait` in `ux0:data/dqiv/config.txt` (read through `platform/cfg`, a per-slot `dqivn<slot>.cfg` overrides it; `ui_side=left` puts the panel on the left). Default **landscape**: the 960x544 surface is the world (cameras keep the vertical extent of the
game's 0.75 "tablet" layout and widen the horizontal one), the UI is the unchanged 408x544 tablet layout in a rect-clipped `MenuCamera` panel on the right; while a menu/message window is shown the world slides left by half a panel (`Layout::ShiftNdc`) so the party stays visible.
`split` = world in the 552 px left of the panel (nothing overlaps), `portrait` = the old 408x544 pillar-boxed surface. CMake `-DDQIVN_LANDSCAPE=OFF` is a kill switch (portrait only). All layout code goes through `game/Layout.hpp` (`Layout::Aspect(cameraAspect)` replaces every
`Camera.aspect` read of the C#; `UiRect/WorldRect`); **never read `Camera.aspect`/`Screen.width` directly in ported UI/camera code**. Things that must cover the whole surface although the MenuCamera is only the panel (fades, battle clear plane, Capture effects) live on the full-surface cameras of `Fade::FullCamera`.
Input scripts: touch coordinates are UI-panel pixels (`tools/newgame_script.txt` works in every layout).

## Layout and file ownership

Work packages are the ones of `docs/arch-dq5/05_PORTING_MAP.md` §3. **One owner per directory/file; headers are contracts** (change them with a note at the top of the header and tell the owner).
`CMakeLists.txt` globs `source/**.c|cpp` (`CONFIGURE_DEPENDS`): adding a file under your directory needs no CMake edit.

| path | owner | content |
|---|---|---|
| `include/miniunity.hpp`, `include/miniunity/{math,coroutine,object,assets,components,statics,engine}.hpp` | WP1 owns the file set; WP2 may extend `components.hpp`(Camera/Renderer/TextMesh) and `assets.hpp`(Shader/Font/RenderTexture) | the mini-Unity API (03_MINI_UNITY.md contract, split by owner) |
| `source/engine/math.cpp object.cpp coroutine.cpp statics.cpp engine_core.cpp pack_glue.cpp stubs.cpp engine_internal.hpp` | **WP1** engine core | Transform/GameObject/lifecycle/Destroy, coroutine runner, Time/Screen/Input/Debug, `Resources` over the pack, stubs for colliders/Animation/Instantiate (TODO) |
| `source/engine/renderer.cpp backend.hpp backend_vitagl.cpp shaders.cpp assets_gl.cpp` | **WP2** renderer | Camera/Renderer/Material/render loop, vitaGL backend (logical FBO, present, FFP draw), `ShaderDesc` table (FFP subset), Texture2D/RenderTexture GL glue |
| `source/shaders/` (empty) | WP2 | CG programs (`*.cg`, 04_SHADERS §4) |
| `source/native/` (`native_bridge`, `sdlib_bridge`, `packet_recorder`, `asset_manager`) | **WP3** | typed `so_symbol` bindings of libdq4core/libsdlib, trace recorder, NDK asset shim |
| `source/platform/vita_input.*` | **WP3** | SceCtrl/touch -> pad bits, Unity touches, logical coordinates (06) |
| `source/game/{GameMain,GameLoop,Global,Boot,MainLoop,NativeManager,TouchPad,Levels,Sound,Cleaner,ApplicationQuit}.*` | **WP3** | boot orchestration, frame loop, the C# glue classes (Sound and ApplicationQuit are mostly stubs) |
| `source/game/{MenuPortSystem,MenuParts,MenuMessage}.*` (+ future MenuPort*, MeshMaker, eIndent, TapToStart, InputName, MenuPortItemTable...) | **WP4** UI | MenuPort mailbox, UI framework, 81 screens (DQ4 table, ids 0..80) |
| `source/game/{SceneCamera,BackColor,Fade,BillboardObject,Chara,Riseup,MeshChara}.*` | **WP5a** sprites/camera | (DQ5 `ParticleSystem` does not exist in DQ4) |
| `source/game/{FldObject,FbxObject}.*` (+ ChunkFile) | **WP5b** map models | |
| `source/game/Field.*` (+ FieldSymbol, BlinkMapSymbol) | **WP5c** world map | `Volcano` is DQ5-only and slated for removal |
| `source/game/{Capture,NetConnectionStub,Staffroll,StaffrollPort}.*` | **WP6** effects | DQ5 `Endroll*`, `CgController`, `SlimeTouch*` are gone; `Spotlight`/`Sepia` are dead code (docs/TODO_DQ4.md) |
| `source/game/DemoOverlay.*` | WP2 (dev aid) | copyright quad + colour clear; delete when the real UI draws |
| `source/loader/`, `source/compat/`, `source/jni/`, `source/utils/`, `source/main.cpp`, `source/config.h`, `lib/` (except vitaGL) | **WP3** platform | so-loader, bionic<->newlib shims, FalsoJNI tables, logger |
| `source/pack/dqiv_pack.{h,c}` | converter owner | runtime pack loader (C99), identical to `tools/convert` output format |
| `tools/convert/`, `web/` | converter owner | pack writer + docs; `web/` is the byte-identical TypeScript port (`node web/test/convert_node.mjs` compares with the SHA-256 table) |

Header-first rule: a package's public API (`game/X.hpp`, `include/miniunity/*`) is committed with stub bodies before anyone depends on it; stub bodies call `DQIV_STUB()`
(`game/stub.hpp`), which logs `STUB <signature>` once - so the dqivn.log of any run lists the unimplemented API that the native side actually used.
`NativeManager.cpp` calls every WP5/WP6 entry point listed in `docs/arch-dq5/05_PORTING_MAP.md` §3 "interfaces (2)".

### Frame loop (game/GameLoop.cpp)

`vita_input::Poll` -> `Engine::BeginFrame` -> `RunUpdate` (Start queue, `TouchPad.Update` (pad word), `MainLoop.Update` (`NativeManager.Update` = `GameUpdate`), then coroutines (Boot logos, `MainLoop.Start`))
-> `RunLateUpdate` (`MenuPortSystem.LateUpdate` = `UnityRenderEvent(1)` first) -> `Render` (cameras by depth into the logical FBO) -> `Present` (centred blit, `vglSwapBuffers`, interval 2) -> `EndFrame` (deferred Destroy, deferred `LoadLevel`).
Level builders (`game/Levels.cpp`) stand in for scene loading ("mainData": Main Camera + Boot; "dq4sp"/level0: MenuCamera + TouchPad, MainCamera + MainLoop; MPad meshes are not built).

## Conventions (keep ports diffable against the C#)

* Same class, member and method names as the C# (`NativeManager::cmd_fld_load`, `Chara::SetPattern`, `m_fld`, `s_camera_pos`...), same static/instance split, same control flow; the C# file is the spec and is named in the header comment of every ported file
  (`work/re/cs_out/Assembly-CSharp/<Class>.cs`; the DQ5 originals are in `/Users/frangar/Fun/dqv/re/cs_out/Assembly-CSharp/`, read-only). **Never improve game logic**: the native core expects the exact C# reaction (frame counts, flag bits, message formats).
* C# -> C++: properties are public fields (`localPosition`, `layer`, `enabled`, `color`) except where Unity computes (`position()/setPosition()`, `rotation()`, `eulerAngles()`);
  `null` -> `nullptr`; `List<T>` -> `std::vector`; `IEnumerator` -> `Coroutine` (`yield return 0` -> `co_yield 0`, `WaitForSeconds` -> `co_yield WaitForSeconds{s}`);
  `Debug.Log` -> `Debug::Log(fmt, ...)`; `Object.Destroy` is deferred to the end of the frame.
  Full table in the header comment of `include/miniunity/object.hpp`; Unity 4.x semantics that the game relies on: `docs/arch-dq5/03_MINI_UNITY.md` §2.
* Header-first stubs never silently succeed on semantic APIs that return data; they return neutral values and log once.
* Everything runs on **one** thread (the game thread, 4 MiB stack); libsdlib's update thread and the OpenSL callback are the only others. GL only on the game thread.
  Do not cache GL state across `UnityRenderEvent` (it binds its own texture): the backend re-binds and sets full state in every draw.
* Native packet arrays (`NativeManager`, `MenuPortSystem`, `ParticleSystem`) are file-static and never freed: libdq4core keeps raw pointers.
* Floats crossing into libsdlib (`SdLib::SoundCtrl_SetVolume`...) go through softfp trampolines (`loader/softfp_wrappers.hpp`); libdq4core's entry points take only ints/pointers (called directly).
* Commit messages end with a blank line and the Co-Authored-By trailer; never commit game binaries, the pack or build dirs (`.gitignore`).

### UI framework (WP4a) and the WP4b recipe

`game/MenuPortSystem` (LateUpdate = `internalUpdate`: `UnityRenderEvent(1)` first, then port-0 commands, push-state machine, per-port `NoticeReceive`/`Update`) -> `MenuPort` (`Param` view of one
native->host message, `Send/SendInts/SendBytes` variadic like the C# `params`) -> `MenuPortItem` (category 1: parts list + id tables) / `MenuMessage` (category 2) / WP6 ports (weak factories in `ui_weak_stubs.cpp`).
`MenuParts` + `MeshMaker` + `MeshList` + `eIndent` + `FRAME_ENUM` are line ports of the C#; `IBase` changers are `std::shared_ptr`, delegates are `std::function`; `new GameObject().AddComponent<T>()` -> `(new GameObject())->AddComponent<T>()`.
**WP4b (DQ4)**: the DQ4 `makeFuncList` has 81 entries (ids 0..80) and every one is ported; open `game/MenuPortItemTable.hpp` for the file layout and translation rules. The DQ5 minigame screens (Sugoroku, Luida, MonsterGame, SlimeTouch, SlimeRace, ScopeBack) do not exist in DQ4 and were removed; ids 68..70 are `Chapter2Status`, `TorunekoMiseban`, `NENE_ItemView`. DQ4 differs from DQ5 in the pad bits of arrow/+- buttons (`MenuPortItem::PAD_D=1, PAD_U=2, PAD_R=4, PAD_L=8`; `PadFocus` L/R use them), in `FRAME_ENUM` (294 frames, no NAME_ENTRY_U2/U3), in the `MeshMaker.SetUV` v convention and `MenuParts.s_info` (14 parts, no sugoroku arrow), and has no `m_se`/`ITexture`/default-name keyboard.
Test without playing through the game: `CFG_EXTRA='ui_open=150:53\nui_press=450:16,520:16\nui_tap=640:13002' tools/run_vita3k.sh 100 "700,900"` (`game/UiSelfTest.hpp`: scripted pad presses, screens built on spare ports, simulated button taps).

### Map models (WP5b)

`game/FldObject` (line port of FldObject.cs + `ChunkFile.hpp`): `Load("map/c/ca/out/caout1")` instantiates `<name>.fld` (prefab: one child per model, grandchildren = vertex-animation shapes) and parses `<name>.map` (FMAP/FMDL/FETC/FSEQ/FSQD/FPMV/FEMV/FPAM/FPLP/FEAN/FCAN/FSRA chunks); shaders are chosen per material from the texture name / `Texture2D::origFormat` (`set_shader`, `docs/arch-dq5/04_SHADERS.md` 3.2). (In DQ5 the title backdrop was map `map/ev/ev17/out/ev17`; the DQ4 title map was not checked.) `game/FbxObject` = `fbx/cNNNN` skinned models + `Animation` ("Take 001"...).
Viewer without the game: create `ux0:data/dqiv/fldtest<slot>.on` with one line per map `"<path> [dispScale] [frames] [mapCamera, 0 = free orbit] [elevation]"` (or `fbx/c0003 1 100 <animNo>`) and run `tools/run_vita3k.sh 40 "60,200,340"`; `tools/hosttest/run.sh <pack> --fld <map>...` loads/animates/sweeps maps under ASan on the host.

### World map (WP5c)

`game/Field` (Field.cs): `Field::Init` builds the quad grid + an orthographic `FieldCamera` (layer 9, mask 512, disables MainLoop's camera). **DQ4 has no native `FieldSet`**: the DQ4 `Field.cs` fills the tile UVs itself (`SetIndexList`, row stride 25 quads) and animates the sea/river tiles in `UpdatePattern` (`m_patternInfo/m_patternCheck`), so `Field.cpp` does both; the host mesh is 59 x 40 quads (`NativeCall::FieldCols`) after the `field_patch` (see docs/TODO_DQ4.md "World map"). `FieldSymbol` = the atlas icons; `BlinkMapSymbol` is the red blinking dots of the city map. The DQ5 `Volcano` plane is not used by DQ4.
Viewer: `ux0:data/dqiv/fieldtest<slot>.on` with lines `"<col0> <row0> [scale] [frames]"` boots normally but holds NativeManager paused (dev builds); not re-run for DQ4.

**UI visual checklist (DQ4, Vita3K, `ui_tour`, 2026-10-30)** - every screen id 1..80 (except 55 name entry, which needs the native's message, and 74 PortalButton, an empty port) was built, filled by `MenuPortItem::DebugFill` (sample text, visibility codes) and screenshotted over the real DQ4 title background; result = layout/atlas/text sane unless noted.
Run (3 runs of ~27 ids, ~150 s each): `CFG_EXTRA='ui_tour=150:1,150:2,...' tools/run_vita3k.sh 150 ""` -> `build/shots/dqivn<slot>_ui<ID>.jpg`.
| family | ids | result |
|---|---|---|
| title / start | 52 53 54 56 (73 TitleTouch is invisible by design) | OK (buttons, grey/red back icon) |
| main menu | 1-11 13-23 27 | OK; atlas icons (items, arrows, status gems, back button, +/- triangles) render upright; windows without a frame code only show their text (the native sends the frame) |
| 12 Chapter | `Menu/map/hyodai1` exists in DQ4: banner shown on its black card | OK |
| battle | 29-43 | OK: faces (DQ4 `Create(2)` + AddIcon 0.3), tactics buttons with the 3 icon layers (id 30), spell/item lists, target frames; 38/42 partly empty until native fills codes |
| maps | 24 ShopMap (picture + frame), 25 CityMap, 26 WorldLargeMap, 28 Encyclopedia | 24/28 OK; 25/26 empty until native sends the map name / flags (not exercised with real data) |
| casino / coliseum | 44-51 | OK (+/- buttons with up/down triangles, frames) |
| shop / bank / church | 57-67, 68 Chapter2Status, 69 TorunekoMiseban, 70 NENE_ItemView | OK; 70 is a dynamic frame (-1) that stays empty until native sends the frame/text |
| network / license | 75-80 | OK (license text and scroll knob, Privacy Policy button) |
| not exercised | pad navigation on real native screens, 25/26 with data, 55 InputName, 74 | |

### Battle

Decoded from `DebugSetValue` / `DebugSetMapName` of libdq4core (not the DQ5 numbers): the game must have a state (the title has an empty party), so `DebugSetValue(1, event, female)` loads a story checkpoint first (`status::Status::setEventParty`: party, levels, gold, items, flags, chapter; table `param::Event`, 347 rows, `dumpevents` logs it). Then `DebugSetValue(8, g, monster << 16 | count)` fills monster group g = 0..3 (`Encount` +0x34/+0x44), and `DebugSetValue(8, 4, 0)` sets Encount type 0, `StageStatus::setBtlMapName("btl_pl_d")` and calls `Global::startBattle()` (`NonBattleActionManager::setAction(4)`, handled by the running town/field part). The harness then overwrites the stage name in the `StageStatus` instance (`g_Global - 0x1dcfa8 + 0x1dff70`, +0x28; the native reads it in `BattleStage::initialize` through `getBtlMapName`, which swaps the 7th/8th characters for the time-of-day letters `_d/_e/_n` when the time zone is enabled). `DebugSetMapName("<count>-<monster>")` is the native shortcut for the same fight on `btl_pl_d` (ignored while the map is the title `ev01`); `0-890` no encounters, `0-801` no damage, `0-802` level 99, `0-899` chapter 9 (staff roll), `1-701`/`1-702` shop/inn fights: the debug lists `BT/..`, `FS/..` at the end of `map/map_list` (`tools/coverage/maps.txt`) are these codes.
WarpTest line `battle MAP GROUPS...` (`battle btl_pl_d 34x2 38x1`) or `ux0:data/dqiv/battletest<slot>.on` (`game/BattleTest`): `map <stage>`, `mon <id> <count>` (up to 4), `event <id> [female]` (default 1; `chapter N` accepted), `delay N`, `repeat N`. Stage names are the files under `map/btl/*/stage` (`btl_pl_d`, `btl_mt_d`, `btl_wd_d`, `btl_sd_e`, `btlda1` (cave), `btlcb1_d`, ...). Monster ids are the ones of the debug list (34 38 42 51 59 69 89 91 97 107 115 119 124 133 145 162 170 ...; id 1 = Walking stick).
Drive the battle with the pad: an input script of Cross taps (`tools/coverage/battle_pad.txt`, `700-2500/24 tap cross`) goes through Fight, Attack, the first target and the messages (PadFocus) to victory and back to the map. Touch coordinates work too (`<F> touch x y`, UI-panel pixels).
Battle layout: `Global.CameraInitParams[1]` view rect (0,0.275,1,0.42) scaled by aspect: the 3D view is the middle band, `SceneCamera.s_clearPlane` blacks the rest.

### Config, language, saves (WP4)

* **Config file**: `ux0:data/dqiv/config.txt` (`key=value`, `#` comments; read first by `platform/cfg.cpp`, then the per-slot `dqivn<slot>.cfg` overrides). Keys: `lang=en|ko|zh-tw|zh-cn` (or 1..4; default `en`) -> `GetLangages::Init()` (Boot) -> `DevInit(lang)`, the quit dialog text, MenuParts font and every `m_langagesNumber` branch of the screens. **DQ4's OBB has no Japanese game text** (`assets/msg/{en,ko,sl,tl}` only, 200 files each), so `lang=ja` is refused with a log warning. Only `en` was run in DQ4 so far.
* **Saves**: written by libdq4core itself (`ux0:data/dqiv/saves<slot>/data1..5.dat` = 15 360 bytes each, `data9.dat` = 512 bytes, as created in Vita3K); the host only provides the directory. The DQ5 port verified a new-game / continue round trip; for DQ4 the files appear after the name entry, a continue was not re-checked. "Continue a suspended adventure" and the quit flow go through `ApplicationQuit` (port-0 cmd 999) as in DQ5.
* **Cloud Save / Google Drive**: `game/NetConnectionStub` = category-4 port in the C#'s "NotReachable" behaviour (status bit1 off, cloud ops answer 408, SQMK auth silently fails, present codes 801). The start menu shows the Cloud Save button; the resulting "connection failed" dialog was verified in the DQ5 port, not re-run for DQ4.

## Soak test (long-session memory / stability, Vita3K)

Tools (dev builds, `-DDQIV_DEV_TOOLS=ON`): `game/SoakStats` logs one `SOAK ...` line every 300 frames (newlib heap used/peak/arena, vitaGL pool free/total, live GameObjects/components/materials/meshes/textures (+CPU/GL bytes), open fds, open MenuPorts, frame time avg/max);
`ui_soak=300:0` (UiSelfTest) builds/fills/closes every screen of the MenuPortItemTable in an endless loop (not used in the DQ4 run below: the DQ4 table is still being ported); `tools/soak/make_soak.py N <ready_frame> --input input.txt > warp<slot>.on` (DQ4 version) writes N iterations of
*town/dungeon warp with the chapter-5 party (480 f) -> forced battle on another stage (`battle <stage> <monsters>`) fought by pad Cross taps (1320 f) -> world map (480 f)*; the party is the event-289 party with `nodamage`, `noencount`, 99999 gold (it needs the WarpTest harness + `input_script=1`);
`tools/soak/analyze.py dqivn.log --every 8 --window 2280 --offset 240` makes the tables below.
Run: `cp warp.on .../ux0/data/dqiv/warp<slot>.on; INPUT_SCRIPT=input.txt CFG_EXTRA='input_script=1' tools/run_vita3k.sh 720 ""` (muted, one emulator at a time; with other agents queued the wait can be 40+ minutes: do not wrap it in a short `timeout`).

Result (2026-10-31, Vita3K, chapter-5 party, `python3 tools/soak/make_soak.py 4 150`; the run was cut at frame 5143 (191 s of game time, 27 Hz) when the surrounding `timeout` of the queued script expired, so only 2.2 of the 4 planned iterations ran: towns `maout` -> `mfout` -> `dab1`, 2 forced battles on `btl_pl_d` (2 x Walking stick) and `btl_mt_d` (Restless armour + Bullfinch) won with Cross taps, 2 world-map phases):

| frame | ms_avg | ms_max | heap | peak | arena | go | comp | mat | mesh | tex | res | texMB | live | fds | ports |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 300 | 40.3 | 328 | 6.5 | 6.9 | 7.2 | 385 | 1218 | 431 | 109 | 118 | 95 | 1.7 | 2293 | 4 | 11 |
| 1200 | 37.6 | 223 | 6.3 | 6.9 | 7.2 | 167 | 588 | 88 | 113 | 121 | 9 | 4.2 | 1109 | 4 | 13 |
| 1800 | 38.7 | 550 | 6.8 | 6.9 | 7.2 | 391 | 1234 | 437 | 124 | 124 | 103 | 4.9 | 2342 | 4 | 11 |
| 2400 | 37.5 | 101 | 7.8 | 7.8 | 10.4 | 137 | 474 | 82 | 77 | 129 | 20 | 6.8 | 933 | 4 | 11 |
| 3000 | 37.4 | 99 | 6.8 | 7.8 | 9 | 351 | 1114 | 364 | 130 | 165 | 98 | 4.8 | 2158 | 4 | 11 |
| 3600 | 37.2 | 71 | 6.6 | 7.8 | 9 | 207 | 749 | 97 | 161 | 168 | 14 | 4.5 | 1416 | 4 | 16 |
| 4200 | 38.6 | 445 | 6.8 | 7.8 | 9 | 351 | 1114 | 366 | 140 | 169 | 101 | 5.1 | 2174 | 4 | 11 |
| 4800 | 37.1 | 94 | 7.8 | 7.8 | 10.2 | 137 | 474 | 87 | 87 | 170 | 23 | 7.1 | 989 | 4 | 11 |
| 5100 | 36.8 | 42 | 7.8 | 7.8 | 10.2 | 137 | 474 | 87 | 87 | 170 | 23 | 7.1 | 989 | 4 | 11 |

Verdict (short run, so only a first look): heap in use 6.3-7.8 MB (peak 7.8, arena 10.4 MB), flat; GameObjects fall back to the same world-map baseline (go 137, comp 474, mat 82-87) after every town + battle cycle (the first value, 385, is the town itself); open fds constant (4); open ports 11 (16 during battle menus); frame time 37-40 ms (27 Hz) without trend, spikes of 0.2-0.5 s only at map / battle-stage loads. Live `Texture2D` objects rise 118 -> 170 and texMB 1.7 -> 7.1 while new maps and stages are visited (lazy pack caches, the same plateau pattern as the DQ5 soak: not a leak, but only 3 towns + 2 stages were visited, so the plateau itself is not shown). No DABT, no ERR. Not covered: the multi-hour behaviour, the vitaGL pool counters (Vita3K reports bogus 2039 MB), real hardware.

## Tester cheats and pad-only scripts (WP3)
* `game/DevCheats.{hpp,cpp}`: cfg keys `nodamage`, `noencount`, `gold`, `party_level`, `party_hp`, `party_mp`, `warp` (+`warp_chapter`, `warp_delay`), `heropos`; documented in README "Debug options". DQ4 mechanisms (all decoded from libdq4core): `nodamage` = exported static
  `status::PartyStatus::setNoDamageEnable(bool)` (a plain global byte, re-armed every 30 frames); `noencount` = `DebugSetValue(10, 0, 2)` which clears the `Encount` "enabled" byte (`Encount::initialize` resets it on every event load, so it is re-armed every 60 frames; `DebugGetValue(1)` reads it back);
  `gold` = `DebugSetValue(3, 0, N)`; `party_level` = `DebugSetValue(11, N, 0)` (`PlayerStatus::levelup`); `party_hp` / `party_mp` = `DebugSetValue(0x11 / 0x12, player, N)` for every player id 1..25 (the native skips mask 0x104ec00) followed by `DebugSetValue(9, 0, 0)` (full heal), read back with `DebugGetValue(0x11)`;
  `warp` = `eventinit <chapter event>` + `Global::startTown(<map>)` (`WarpTest::CreateFromCfg`). Encount / StageStatus / StoryStatus are not exported: `DevHooks::Obj(vaddr)` addresses them relative to `g_Global` (offsets read from the GOT relocation of the `DebugSetValue` code).
* Input scripts (`INPUT_SCRIPT=file tools/run_vita3k.sh`, `platform/vita_input.cpp`, full syntax in the header comment of that file; one event per line, `<frame>` = rendered frames since the first Poll): `<F> tap cross [n=2]`, `<F>-<T>/<P> tap cross` (repeat), `<F> down|up <btn>`, `<F> touch x y`, `<F> stick`, `<F> log text`, `<F> quit`, plus the closed-loop ops below, which hold the script until their condition is met and make every later event relative to that moment (so a script can be written as "do this, then 30 frames later that"):
  * `<F> sync <map substring> [tap <btn> <period>]`: wait until the last map path loaded by the native contains the text (`NativeManager`, e.g. `sync ham1f1` or `sync worldmap`), tapping a button meanwhile.
  * `<F> walk <x> <z> [tol=0.5] [timeout=600] [map]`: hold the d-pad directions that bring the hero (position as sent by the native, the `CHARA0 pos` numbers of `heropos=1`; screen up = +z, right = +x) to (x, z) within `tol`, or stop at `timeout` frames, or when the scene no longer contains `[map]` (a door was taken). It logs `walk reached/timeout/left the map` with the final position in hundredths. The d-pad only, so walking through doors, NPC collisions and tile rules are the real ones. No diagonal planning: use waypoints around obstacles.
  * `<F> autotap <btn> <period>`: press `<btn>` for 2 frames every `<period>` frames from now on (0 = off), also while `walk`/`sync` hold the script: dismisses messages while walking. Do not leave it on while you want a menu to stay open.
  * `<F> roam <timeout> [period=40]`: walk about (down, right, up, left bursts of `period` frames) until a random battle starts, tap Cross through it (every 20 frames), end 120 frames after the battle stage went away or at the timeout.
  Debug logging for these: cfg `heropos=1` logs `CHARA0 pos` (hero, as sent by the native) whenever it changes, `heropos=2` also every other character slot (`CHARA<n> pos`, NPCs and party followers, plus `CHARA_LOAD slot id` lines): that is how the NPC / counter positions for `walk` are found without any reverse engineering.
* **TURBO** (`TURBO=1 tools/run_vita3k.sh ...`, `TURBO=1 tools/coverage_run.sh <name>`): sets cfg `turbo=1` (no 30 Hz limit in `vita_platform.cpp`) and `VITA3K_VBLANK_HZ=240`; the run script shortens DUR to `DUR/3 + 10` wall-clock seconds. About 4.3x faster than real time. The game is frame-locked, so frame numbers, input scripts, `sync`/`walk` and screenshot frames keep their meaning; **sound is not in step**, so anything that waits on audio (jingles, BGM-driven scene steps) takes a different number of frames: steer with closed-loop ops (`sync`, `walk`), not fixed timings, there. Never judge pacing, frame time or frame rate under TURBO (the soak is run without it).
  `tools/padonly_dq4.txt` (boot -> TapToStart -> New Game -> name entry -> intro, buttons only) needs no touch at all; `tools/newgame_script.txt` is the touch version. `tools/coverage/*.warp` are the DQ5 port's coverage scripts (map names and flags of DQ5) and do not apply to DQ4.
* `PadFocus` (`game/PadFocus.cpp`): physical-button navigation of every MenuPortItem screen (see its header); `InputName` has its own pad path.
* `tools/link_pad.ld`: pads the text segment so `vita-elf-create` always has room before the 64 KB aligned data segment.

## Branches and decisions

- `main`: the native (Unity-free) DQ4 port. Feature work happens on branches in worktrees under `work/` (`git worktree add work/wt-NAME -b NAME`) and is merged into `main` by the integrator.
- Landscape: design A (world full width, menus in the right panel, dialogues bottom-centre) is the default.
  The sliding-panel variant (design C in `docs/arch-dq5/07_LANDSCAPE.md`) is dropped.
- Data and packet differences from the DQ5 port: docs/TODO_DQ4.md. Packet table summary (NativeManager.cs, 38 commands instead of 41): DQ5 ops 8 `fld_etc`, 28 `mesh_ortho`, 40 `particle` are gone and every later op shifts down (DQ4: 0 `fld_load` ... 27 `field_load`, 28 `field`, 29 `field_symbol`, ... 37 `bgm`); MenuPort buffers are 129 shorts host->native and 1089 (65 + 1024) native->host; port-0 command 7 (`HIRO_NAME_SCALE`) was added and 9 (`SetInputMode`) removed; category 3 is `StaffrollPort`, category 5 is gone.

## Open problems / next steps

The authoritative list is [docs/TODO_DQ4.md](docs/TODO_DQ4.md). In short:

* Verify DQ4 in play (field, world map, battle, events, staff roll); everything beyond the title/menu is only compiled and partly seen in screenshots.
* Re-derive `DevCheats`, `WarpTest`, `BattleTest` from `libdq4core.so` (Ghidra scripts in `tools/re/`), then `kDevHooksVerified`.
* `Sound`: the DQ4 state machine (`s_state/s_bgmNext`) is not ported; the DQ5 one is in use. `MainLoopEx/FadeEx/MenuPortSystemEx`, `MenuPortSystem.OnGUI` and the virtual-pad code of `TouchPad` are not ported (physical buttons replace the virtual pad).
* Remove the dead DQ5-only code (`Spotlight`, `Sepia`, `Volcano`, `CgController`, DQ5 `Capture::ScreenEffect` sub-commands, `Make_SlimeTouch_*`, DQ5 names in `PadFocus`/`Layout`).
* `vglInitExtended` returns GL_TRUE only when the resolution had to be reduced (it is *not* a success flag) - do not check it.
* Audio threads: vitasdk OpenSLES callback stack (0x10000) vs Vorbis decoding in `CoreAudioOutCallback`: runs in Vita3K without underruns, untested on hardware; fallback = an own audio thread.
* Memory budget: newlib heap 96 MB (`DQIV_HEAP_MB`), vitaGL takes the rest minus 6 MiB; textures are uploaded eagerly (no unload yet); only the title run was measured.
* Real hardware untested. `libshacccg` is not needed to start (`cg/dqiv_gray.bin` and `cg/dqiv_sepia_fld.bin` are shipped), but `dqiv_sleep` (CharaSleep) has no shipped blob and falls back to fixed-function without it.
