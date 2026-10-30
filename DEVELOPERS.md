# Dragon Quest IV - native PS Vita port: developer documentation (end-user README: README.md)

Android "Dragon Quest IV" (Unity 4.7 + Mono front end, `libdq4core.so` game logic) rebuilt as a **native Vita app**:

* `libdq4core.so` (NEON build, all game logic) and `libsdlib_android.so` (audio, OpenSL ES) are loaded with the so-loader and driven
  through their narrow C interface (`GameUpdate(pad)` + int/float packet arrays + the MenuPort mailbox).
* Everything Unity/Mono did - ~19.5k lines of C# that turn those packets into scenes, UI and effects - is ported line by line to C++20 on top of a
  small **mini-Unity** engine (`include/miniunity*`) that renders with vitaGL.
* Assets come from the converted pack (`tools/convert`), not from Unity files.

Plan and decisions live in `/Users/frangar/Fun/dqiv/work/re/arch/` (`README.md` indexes 01..06); this repo is phase A of that plan ("header-first").
This repo is independent of the Unity+Mono experiment in `../port/` (TITLEID `DQIVV00001`); they only share the data directory.

## Status (phase A, 2026-10-27)

Works in Vita3K (`tools/run_vita3k.sh`):

* so-loader: libdq4core + libsdlib_android load, link (0 unresolved imports), run constructors and `JNI_OnLoad` (`IsLoadNativeCode() = 1`).
* boot sequence of the C# `Boot`/`MainLoop`: `LoadFileList` (fake asset manager, `assets/filelist.bin`), `DevInit(obb, saves/, lang 1, devInfo 1) = true`,
  `ParticleSystemInit`, `MenuPortInit`, `GameInit` with the static packet arrays, then **`GameUpdate` every frame at 30 Hz** (vitaGL swap interval 2) followed by
  `UnityRenderEvent(1)` (exactly the C# order). No crash over 1000+ frames.
* `NativeManager::Update` parses the 41 native->host commands with the complete operand consumption of the C# (the effects are WP5/WP6 stubs); per-frame packet log + binary recorder.
* audio: `SdSoundSystem_Create("")` returns 0 with vitasdk OpenSLES at 32 kHz, `FileReadAllBytes("sound/snd_vol.dat")` works, the `AAssetManager_open(NULL, "<obb>/sound/BGM01.akb")` shim returns the right window.
* mini-Unity slice: object model, coroutines, cameras, renderer, 408x544 logical-surface FBO presented centred on 960x544, `Resources.Load` over the pack (textures, text assets).
  The Boot logos (`boot/sqex`, `boot/arte_logo720_240`, Picture shader + alpha fades) and a demo overlay (`boot/retro5_copyright` over a dark-blue clear) render through that path.
* SceCtrl/touch -> pad word -> `TouchPad.m_press/m_toggle` -> `GameUpdate`; touch -> logical coordinates + Unity touch phases (not testable in Vita3K headless; no crash).

What the native side emits per frame after `GameInit` (observed in Vita3K, no input): frame 0 `fade` (op10, sets `Global.Flag` bit 1), frame 1 `mesh_load`x2 (op22), `mesh_ortho`x2 (op28),
`back_color` (op9), `fade`; frame 2 `camera` (op11); from frame 3 `mesh` (op23) x2 *every* frame (DSSA quads of the title scene, 36-43 ints / 86-106 floats); ~250 frames later one more `fade`;
then idle (`flag=0x1` stays set because the `Fade` class is a stub and never reports the end). During the first ~85 frames of the logo phase MenuPort messages appear on port 0
(`[0,5,127]` BGM volume, `[0,4,127]` SE volume, `[p,1,category,type]` port-open for ports 1..11) - they are logged, not yet consumed.

> **End users**: see `README.md` (`tools/make_data.py` builds `ux0:data/dqiv` from an APK+OBB; Release builds have no developer tools, `-DDQIV_DEV_TOOLS=ON` / Debug enables test scenes and scripted UI).

## Build / run

```sh
export VITASDK=/usr/local/vitasdk PATH=$VITASDK/bin:$PATH
git submodule update --init           # lib/vitaGL (pinned at 76f43e6, same as port/)
mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make -j10     # -> build/dqivn.vpk, eboot.bin
cd .. && tools/setup_data.sh          # once: libs/filelist/obb/pack into Vita3K's ux0:data/dqiv (symlinks; --copy for hardware staging)
tools/run_vita3k.sh 40 "300,600"      # install VPK, run 40 s, screenshots at frames 300 and 600
```

`run_vita3k.sh` leaves `build/dqivn.log` (our log), `build/vita3k-run.log` (emulator log), `build/vita3k-stdout.log`, `build/shots/dqivn_fNNNNN.jpg`. **Always read both logs.**
Several agents share one Vita3K install: the script never `pkill`s; it starts the emulator with `exec -a v3k-dqivn` so another agent's `pkill -f MacOS/Vita3K` does not hit it,
and screenshots are requested by the app itself at the given *frame numbers* (`ux0:data/dqiv/dqivn.cfg`, `VITA3K_SCREENSHOT` printf) so file names are unique.

* Debug builds log everything (`l_debug/info/warn/success`, file reopened per line so a crash loses nothing); Release only errors. `-DDQIV_FAST_BOOT=OFF` restores the real logo timing (1 s / 3 s / 1 s).
* Packet trace: create `ux0:data/dqiv/trace.on`, run, then `tools/pktdump.py <fs>/ux0/data/dqiv/trace` (per-frame summary, flags desynchronised streams).
* `tools/check_imports.py` checks `source/loader/dynlib_table.inc` against `refs/imports_*.txt`. `tools/convert/` = pack converter (Python, venv in `../tools/venv`), format in `tools/convert/FORMAT.md`; `web/` = TypeScript port of the same converter that runs in the browser (end-user setup page, see `web/README.md`; it must stay byte-identical to `tools/convert`: after changing either, run `node web/test/convert_node.mjs` and compare), `make -C tools/convert test PACK=...` validates a pack with the runtime loader.
* Data layout (`ux0:data/dqiv/`, shared with `port/`): `lib/*.so`, `main.obb`, `assets/filelist.bin`, `pack/` (199 MB), `saves/` (DevInit save dir), `dqivn.log`, optional `trace.on`, `trace/`, `dqivn.cfg`.
* Constants (paths, load addresses, 408x544 logical size, language, device info) are in `source/config.h`.

### Screen layout (landscape, 07_LANDSCAPE.md)

Runtime setting `layout=landscape|split|portrait` in `ux0:data/dqiv/config.txt` (read through `platform/cfg`, a per-slot `dqivn<slot>.cfg` overrides it; `ui_side=left` puts the panel on the left). Default **landscape**: the 960x544 surface is the world (cameras keep the vertical extent of the
game's 0.75 "tablet" layout and widen the horizontal one), the UI is the unchanged 408x544 tablet layout in a rect-clipped `MenuCamera` panel on the right; while a menu/message window is shown the world slides left by half a panel (`Layout::ShiftNdc`) so the party stays visible.
`split` = world in the 552 px left of the panel (nothing overlaps), `portrait` = the old 408x544 pillar-boxed surface. CMake `-DDQIVN_LANDSCAPE=OFF` is a kill switch (portrait only). All layout code goes through `game/Layout.hpp` (`Layout::Aspect(cameraAspect)` replaces every
`Camera.aspect` read of the C#; `UiRect/WorldRect`); **never read `Camera.aspect`/`Screen.width` directly in ported UI/camera code**. Things that must cover the whole surface although the MenuCamera is only the panel (fades, battle clear plane, Capture effects) live on the full-surface cameras of `Fade::FullCamera`.
Input scripts: touch coordinates are UI-panel pixels (`tools/newgame_script.txt` works in every layout).

## Layout and file ownership

Work packages are the ones of `re/arch/05_PORTING_MAP.md` §3. **One owner per directory/file; headers are contracts** (change them with a note at the top of the header and tell the owner).
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
| `source/game/{SceneCamera,BackColor,Fade,BillboardObject,Chara,Riseup,MeshChara,ParticleSystem}.*` | **WP5a** sprites/camera | |
| `source/game/{FldObject,FbxObject}.*` (+ ChunkFile) | **WP5b** map models | |
| `source/game/Field.*` (+ FieldSymbol, Volcano, BlinkMapSymbol) | **WP5c** world map | |
| `source/game/{Capture,Spotlight,Sepia,NetConnectionStub}.*` (+ Endroll*, CgController, SlimeTouch*) | **WP6** effects | |
| `source/game/DemoOverlay.*` | WP2 (dev aid) | copyright quad + colour clear; delete when the real UI draws |
| `source/loader/`, `source/compat/`, `source/jni/`, `source/utils/`, `source/main.cpp`, `source/config.h`, `lib/` (except vitaGL) | **WP3** platform | so-loader, bionic<->newlib shims, FalsoJNI tables, logger |
| `source/pack/dqiv_pack.{h,c}` | converter owner | runtime pack loader (C99), identical to `tools/convert` output format |
| `tools/convert/` | converter owner | pack writer + docs (copied from `../tools/convert`, the original stays there) |

Header-first rule: a package's public API (`game/X.hpp`, `include/miniunity/*`) is committed with stub bodies before anyone depends on it; stub bodies call `DQIV_STUB()`
(`game/stub.hpp`), which logs `STUB <signature>` once - so the dqivn.log of any run lists the unimplemented API that the native side actually used.
`NativeManager.cpp` already calls every WP5/WP6 entry point listed in 05 §3 "interfaces (2)".

### Frame loop (game/GameLoop.cpp)

`vita_input::Poll` -> `Engine::BeginFrame` -> `RunUpdate` (Start queue, `TouchPad.Update` (pad word), `MainLoop.Update` (`NativeManager.Update` = `GameUpdate`), then coroutines (Boot logos, `MainLoop.Start`))
-> `RunLateUpdate` (`MenuPortSystem.LateUpdate` = `UnityRenderEvent(1)` first) -> `Render` (cameras by depth into the logical FBO) -> `Present` (centred blit, `vglSwapBuffers`, interval 2) -> `EndFrame` (deferred Destroy, deferred `LoadLevel`).
Level builders (`game/Levels.cpp`) stand in for scene loading ("mainData": Main Camera + Boot; "dq4sp"/level0: MenuCamera + TouchPad, MainCamera + MainLoop; MPad meshes are not built).

## Conventions (keep ports diffable against the C#)

* Same class, member and method names as the C# (`NativeManager::cmd_fld_load`, `Chara::SetPattern`, `m_fld`, `s_camera_pos`...), same static/instance split, same control flow; the C# file is the spec and is named in the header comment of every ported file
  (`re/cs_out/Assembly-CSharp/<Class>.cs`). **Never improve game logic**: the native core expects the exact C# reaction (frame counts, flag bits, message formats).
* C# -> C++: properties are public fields (`localPosition`, `layer`, `enabled`, `color`) except where Unity computes (`position()/setPosition()`, `rotation()`, `eulerAngles()`);
  `null` -> `nullptr`; `List<T>` -> `std::vector`; `IEnumerator` -> `Coroutine` (`yield return 0` -> `co_yield 0`, `WaitForSeconds` -> `co_yield WaitForSeconds{s}`);
  `Debug.Log` -> `Debug::Log(fmt, ...)`; `Object.Destroy` is deferred to the end of the frame.
  Full table in the header comment of `include/miniunity/object.hpp`; Unity 4.7 semantics that the game relies on: 03_MINI_UNITY.md §2.
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

`game/FldObject` (line port of FldObject.cs + `ChunkFile.hpp`): `Load("map/c/ca/out/caout1")` instantiates `<name>.fld` (prefab: one child per model, grandchildren = vertex-animation shapes) and parses `<name>.map` (FMAP/FMDL/FETC/FSEQ/FSQD/FPMV/FEMV/FPAM/FPLP/FEAN/FCAN/FSRA chunks); shaders are chosen per material from the texture name / `Texture2D::origFormat` (`set_shader`, 04_SHADERS 3.2). The title screen background is map `map/ev/ev17/out/ev17` (event camera 1). `game/FbxObject` = `fbx/cNNNN` skinned models + `Animation` ("Take 001"...).
Viewer without the game: create `ux0:data/dqiv/fldtest<slot>.on` with one line per map `"<path> [dispScale] [frames] [mapCamera, 0 = free orbit] [elevation]"` (or `fbx/c0003 1 100 <animNo>`) and run `tools/run_vita3k.sh 40 "60,200,340"`; `tools/hosttest/run.sh <pack> --fld <map>...` loads/animates/sweeps maps under ASan on the host.

### World map (WP5c)

`game/Field` (Field.cs): `Field::Init` (op 29) builds the 25 x 40 quad grid + an orthographic `FieldCamera` (layer 9, mask 512, disables MainLoop's camera); `SetIndexList` (op 30) calls the native `FieldSet` (needs `DevInit`: PowerVR half-texel bias comes from deviceInfo) to fill the tile UVs; `FieldSymbol` = the 42 atlas icons (picid 19 = volcano: plain quad until WP2's `game/Volcano.hpp` exists, picked up automatically via `__has_include`); `BlinkMapSymbol` is the red blinking dots of the city map (MenuPortItemTable ICityMap code 3).
Viewer: `ux0:data/dqiv/fieldtest<slot>.on` with lines `"<col0> <row0> [scale] [frames]"` (atlas block shown in the grid, all 42 symbols beside it) boots normally but holds NativeManager paused; `tools/run_vita3k.sh 40 "150,270,390"`.

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

### Battle (forced through the native debug hooks)

`ux0:data/dqiv/battletest<slot>.on` (`game/BattleTest`): the game boots normally (title), then after `delay` frames the driver calls libdq4core's own debug entry points, as the original debug menu did: `DebugSetMapName("B-<short battle map>")`, optional `DebugSetValue(12, id, 0)`... `(12,-1,0)` (party; the default debug party is the hero LV14 + Harry), `DebugSetValue(8, g, monster<<16 | count)` for groups 0..3, then `DebugSetValue(8, 4, 0)` = `setTileId` + `Encount::brew` + `Global::startBattle()` (game part 5). File lines: `map btlca1` (the SHORT native name; `unity_load_fld` copies it into a 16-byte stack buffer and builds `map/btl/btlca/stage/btlca1`, so a full path smashes the stack), `mon <id> <count>` (up to 4), `party <id>`, `delay N`, `repeat N`.
Then drive the existing battle screens with `INPUT_SCRIPT` touches (logical coords, origin bottom-left; Fight (224,165), Attack (104,165), Magic (297,165), target list rows (104,166)/(104,120), spell Cast button (204,97)); pad buttons do not move the MenuPort cursor yet. Verified: map `btlca1` + burr babies (fight to victory, return to the title), `btl_pl_d` + Healslime x2/Gourdzilla/Samigina (attack, spell Woosh with its wind effect, damage numbers, aura effect, party HP/MP updates).
Battle layout: `Global.CameraInitParams[1]` view rect (0,0.275,1,0.42) scaled by aspect: the 3D view is the middle band, `SceneCamera.s_clearPlane` blacks the rest.

### Config, language, saves (WP4)

* **Config file**: `ux0:data/dqiv/config.txt` (`key=value`, `#` comments; read first by `platform/cfg.cpp`, then the per-slot `dqivn<slot>.cfg` overrides). Keys: `lang=en|ko|zh-tw|zh-cn` (or 1..4; default `en`; Japanese text is not in the DQ4 OBB) -> `GetLangages::Init()` (Boot) -> `DevInit(lang)`, the quit dialog text, MenuParts font (`Menu/SE-GARRIR`, `GMaruGoPro-M`, `FOTK-YDHopeL`, `FZHTK_TL`, `FZHTK_SL`) and every `m_langagesNumber` branch of the screens. Verified in Vita3K: start menu in EN, JA, KO, zh-TW, zh-CN (fonts + native canvas text); the EN atlas' rotated ("flipped") glyphs (star, down-triangle, N) render upright.
* **Saves**: written by libdq4core itself (`Backup_Write` -> `ux0:data/dqiv/saves<slot>/data1..5.dat` = 15360 bytes each (the 5 adventure logs / autosave), `data9.dat` = 512 bytes (system/options)); the host only provides the directory. Verified end to end: New Game ("Tester") in one boot -> next boot "Continue your adventure" shows `1: Tester LV 1 000:00 Prologue`. "Continue a suspended adventure" is enabled by the native suspend data; the C# never triggers it (`OnApplicationPause` only pauses sound, `ApplicationQuit` only shows the quit dialog): the native writes it from its own menu and then asks the host to quit with port-0 cmd 999 (`ApplicationQuit::Create`), so nothing extra is needed on the Vita beyond that dialog and the PS-button pause (`Sound::SystemPause`).
* **Cloud Save / Google Drive**: `game/NetConnectionStub` = category-4 port in the C#'s "NotReachable" behaviour (status bit1 off, cloud ops answer 408, SQMK auth silently fails, present codes 801); the native shows "Connection failed. Please check your signal and try again." and returns to the menu (verified, no hang).

## Soak test (long-session memory / stability, Vita3K)

Tools (dev builds, `-DDQIV_DEV_TOOLS=ON`): `game/SoakStats` logs one `SOAK ...` line every 300 frames (newlib heap used/peak/arena, vitaGL pool free/total, live GameObjects/components/materials/meshes/textures (+CPU/GL bytes), open fds, open MenuPorts, frame time avg/max);
`ui_soak=300:0` (UiSelfTest) builds/fills/closes every screen of the MenuPortItemTable in an endless loop; `tools/soak/make_soak.py N <ready_frame> --input input.txt > warp<slot>.on` writes N iterations of
*warp into a town/dungeon (480 f) -> forced battle on another stage fought by scripted touches (1320 f) -> world map (480 f)* (needs the WP1/WP5 `WarpTest` harness + `input_script=1`); `tools/soak/analyze.py dqivn.log --every 8 --window 2280 --offset 240` makes the tables below.
Run: `INPUT_SCRIPT=input.txt CFG_EXTRA='input_script=1\nlayout=landscape\nui_soak=300:0' tools/run_vita3k.sh 1500 ""` (muted, one emulator at a time).

Result (2026-10-27, Vita3K, loaded Mac so ~15-20 fps of real time; 19 200 frames = 823 s of game time, 8 battles on 6 stages + 8 town/dungeon FLD loads + 48 complete passes over the 98 UI screens = ~4 700 screen open/close cycles):

Sampled curve (landscape; heap/peak/arena in MB, `mesh` count, `tex` = live Texture2D objects, `res` = textures with a GL copy):

| frame | ms_avg | ms_max | heap | peak | arena | go | comp | mat | mesh | meshMB | tex | res | texMB | live | fds | ports |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 300 | 65 | 849 | 7.9 | 7.9 | 8 | 438 | 1377 | 477 | 153 | 0.24 | 113 | 72 | 1.5 | 2594 | 4 | 11 |
| 2700 | 45.6 | 359 | 7.6 | 8.3 | 10.2 | 159 | 540 | 119 | 94 | 0.07 | 153 | 31 | 4.3 | 1104 | 4 | 11 |
| 5100 | 64.7 | 811 | 8.3 | 8.4 | 12.1 | 125 | 438 | 98 | 82 | 0.17 | 167 | 25 | 4.6 | 949 | 4 | 11 |
| 7500 | 41.9 | 471 | 7.6 | 8.5 | 11.5 | 167 | 564 | 141 | 110 | 0.09 | 180 | 40 | 4.9 | 1201 | 4 | 11 |
| 9900 | 38.7 | 210 | 7.7 | 8.5 | 11.5 | 146 | 515 | 95 | 110 | 0.1 | 186 | 15 | 4.9 | 1091 | 4 | 12 |
| 12300 | 44.6 | 650 | 7.8 | 8.5 | 11.5 | 209 | 738 | 137 | 171 | 0.13 | 195 | 18 | 5.1 | 1489 | 4 | 15 |
| 14700 | 47.1 | 500 | 8.2 | 8.5 | 13.1 | 236 | 853 | 150 | 214 | 0.21 | 241 | 26 | 5.1 | 1733 | 4 | 18 |
| 17100 | 45.4 | 382 | 8 | 8.5 | 12.7 | 227 | 826 | 142 | 211 | 0.2 | 246 | 21 | 5.5 | 1691 | 4 | 18 |
| 19200 | 39.9 | 173 | 8.1 | 8.5 | 12.9 | 137 | 478 | 111 | 114 | 0.17 | 247 | 20 | 7.2 | 1126 | 4 | 11 |

Per-iteration floor..ceiling (a leak would be a rising floor; landscape, then portrait (5 iterations, 11 400 frames)):

| iter | heap | arena | go | mat | mesh | tex | live | ms_avg |
|---|---|---|---|---|---|---|---|---|
| 0 | 7.7..8.2 | 8..11.7 | 136..438 | 86..486 | 76..168 | 113..138 | 949..2633 | 39.5..169.6 |
| 1 | 7.6..8.1 | 10.2..11.7 | 119..227 | 91..134 | 83..192 | 153..157 | 909..1574 | 39.7..47 |
| 2 | 7.8..8.4 | 11.7..12.1 | 110..225 | 77..118 | 82..186 | 156..170 | 860..1558 | 37.4..64.7 |
| 3 | 7.6..8.1 | 11.5..11.7 | 136..227 | 92..143 | 87..190 | 180..183 | 1011..1594 | 40.8..49.2 |
| 4 | 7.7..8.1 | 11.5..11.7 | 136..225 | 95..154 | 97..194 | 184..187 | 1034..1592 | 38.6..46.9 |
| 5 | 7.7..8.1 | 11.5..13.1 | 136..248 | 106..171 | 105..222 | 194..196 | 1056..1764 | 39.5..50.9 |
| 6 | 8.1..8.2 | 13.1..13.1 | 136..301 | 114..308 | 112..215 | 241..242 | 1117..2013 | 45.6..64.2 |
| 7 | 7.8..8.2 | 12.7..13.1 | 130..227 | 105..151 | 107..221 | 242..247 | 1093..1710 | 41.9..51.8 |
| 8 | 8.1..8.1 | 12.9..12.9 | 136..137 | 109..116 | 113..116 | 247..247 | 1118..1128 | 39.9..41.1 |

portrait:

| iter | heap | arena | go | mat | mesh | tex | live | ms_avg |
|---|---|---|---|---|---|---|---|---|
| 0 | 7.7..8.2 | 8..11.8 | 136..438 | 87..487 | 77..169 | 114..138 | 952..2636 | 38.7..58.9 |
| 1 | 7.6..8.1 | 11.6..11.8 | 118..227 | 92..135 | 84..193 | 153..157 | 910..1577 | 42.9..59.5 |
| 2 | 7.8..8.4 | 11.8..12.2 | 109..225 | 78..119 | 83..187 | 156..170 | 860..1561 | 43.3..52.1 |
| 3 | 7.6..8.1 | 11.6..11.8 | 136..227 | 93..144 | 88..191 | 180..183 | 1014..1597 | 40.7..54.5 |
| 4 | 7.7..8.1 | 11.6..11.8 | 136..225 | 96..155 | 98..195 | 184..187 | 1037..1595 | 41.7..46.8 |


Verdict:
* **Flat**: heap in use 7.6-8.5 MB, peak 8.5 MB from frame 5700 on, GameObjects/components return to the same town baseline (go 136, comp 474) at the end as at the start, open fds constant (4), open ports back to 11 after every UI pass, frame time avg 39-50 ms in Vita3K (no trend; the slope of the second half is +-3 ms / 10 000 frames, spikes to 0.5-1.5 s are first-load hitches of new maps/battle stages).
* **Bounded growth, not leaks**: the newlib *arena* (high-water mark) crept 10.2 -> 13.1 MB while the in-use heap did not (fragmentation; stops after iteration 5); the number of live `Texture2D`/`Material`/`Mesh` *objects* at the town baseline rises with the number of distinct maps/stages visited (tex 113 -> 247, mat 86 -> 110, mesh 76 -> 113) and plateaus once the stage list repeats (iterations 6-8): they are the lazy pack caches (GL copies of textures are evicted by the sweep, `res` stays 11-40; CPU mesh data 0.1-0.2 MB).
* Portrait behaves identically to landscape for the 5 iterations run (11 400 frames; the second run was cut short by the one-emulator-at-a-time rule).
* UI: `UI-MEM` counters (see "Free of leaks" in the UI section) stay flat over 10 open/close cycles of any screen; the 4 700 cycles of the soak confirm it.
* Not measurable in Vita3K: vitaGL pool free sizes (`vglMemFree` uses `sceClibMspaceMallocStats`, which the emulator stubs: all pools report the same bogus 2039 MB) - the columns are meaningful on real hardware only; native-side (libdq4core) allocations are inside the newlib heap numbers (the bionic shim mallocs); GXM textures/meshes are tracked through `tex`/`res`/`texMB`.
* Notes for owners: 2 of the 6 battle stages (`btl_is_d`, `btl_dk1`) did not start through `battle <name>` (no `map_name` line, probably not valid native short names) - harness only; world-map (`field`) phases add no FLD (no map_name line), so the field-map loader is exercised by the towns/battle stages only.

## Tester cheats and pad-only scripts (WP3)
* `game/DevCheats.{hpp,cpp}`: cfg keys `nodamage`, `noencount`, `gold`, `party_hp`, `party_mp`, `warp` (+`warp_chapter`, `warp_delay`), `heropos`; documented in README "Debug options". `noencount` overwrites the first two words of
  `btl::Encount::brew` (ARM, `mov r0,#0; bx lr`) with kuKernelCpuUnrestrictedMemcpy; the callers (`execField`, `execDungeon`) only start a battle when it returns non-zero. `warp` reuses `WarpTest` (`WarpTest::CreateFromCfg`).
* Input scripts (`INPUT_SCRIPT`, `platform/vita_input.cpp`): besides buttons / touch / sticks there is `<F> sync <map substring> [tap <btn> <period>]` (wait for a map, tapping meanwhile; later events are relative to that moment).
  `tools/padonly_to_cabin.txt` (boot -> New Game -> name -> cabin) and `tools/coverage/cave_walk.txt` (random walk + Cross taps, use with `warp=dab1 nodamage=1`) need no touch at all.
* `PadFocus` (`game/PadFocus.cpp`): physical-button navigation of every MenuPortItem screen (see its header); `InputName` has its own pad path.
* `tools/link_pad.ld`: pads the text segment so `vita-elf-create` always has room before the 64 KB aligned data segment.
* Saves: `re/saves/README.txt` (restore a snapshot into `saves<slot>/`).

## Branches and decisions

- `main`: the native (Unity-free) port.
- `unity-mono`: the parked Unity 4.7 + Mono 2.6 experiment (imported from the old `port/` repo, last commit 0333070).
  It boots libunity/libmono through the so-loader (emulated signals for the Boehm GC, RWX heap via kubridge,
  EGL/GLES2 over vitaGL with attribute compaction) and reaches the title screen and start menu in Vita3K at ~30 fps.
  Kept as a reference and as a generic route for other Unity 4 Android games; not developed further.
- Landscape: design A (world full width, menus in the right panel, dialogues bottom-centre) is the default.
  The sliding-panel variant (design C in re/arch/07_LANDSCAPE.md) is dropped.

## Open problems / next steps

* WP3: real `Sound` (banks, BGM/SE lists, `Sound.SystemUpdate`), `ApplicationQuit` dialog (sceMsgDialog), pointer cursor (Select) + the ePadState/ray machinery for MenuPort buttons, `--replay` of `.pkt` traces, config file (confirm=circle, layout), Global.Flag gating tests with real fades.
* WP1: prefab `Instantiate` (pack prefab -> GameObject tree incl. `scene/level0`), Material/Mesh/Font/AnimClip loading, Animation + skinning, colliders, `Resources.UnloadUnusedAssets`.
* WP2: CG programs for the shaders that are not FFP (fld_*, Sepia, Spotlight, ButtonGray/Reverse, ImdShade), TextMesh/Font, A8 texture `GL_ALPHA` behaviour in vitaGL, `Camera.Render`/RenderTexture/OnRenderImage, depth-sorting details; vitaGL FBO texture orientation is currently correct (verified by the upright copyright text).
* `vglInitExtended` returns GL_TRUE only when the resolution had to be reduced (it is *not* a success flag) - do not check it.
* Audio threads: vitasdk OpenSLES callback stack (0x10000) vs Vorbis decoding in `CoreAudioOutCallback` is untested; fallback = option B of REPORT_RUNTIME (own audio thread).
* The memory budget is untested beyond the boot: newlib heap 128 MiB (`DQIV_HEAP_MB`), vitaGL takes the rest minus 6 MiB; textures are uploaded eagerly (no unload yet).
* Real hardware untested (kubridge + libshacccg not needed so far: only fixed-function GL is used).
