# 05 - Class-by-class porting map, work packages, order of work

Target language: **C++20 (gnu++20, already in `port/CMakeLists.txt`) on top of the mini-Unity engine** (`arch/include/miniunity.hpp`), vitaGL for rendering.
Translation rule: *mechanical, line-for-line*. Same class/member names, same static/instance split, same control flow; only Unity/BCL calls change per the conventions in the header comment. Never "improve" game logic: the native core expects the exact C# reaction (frame counts, flag bits, message formats).
Counts below are C# lines (from `wc -l`); C++ will be ~1.2x. Difficulty 1 (trivial) .. 5 (hard).

## 0. Proposed source layout (inside `/Users/frangar/Fun/dqv/port/`; the loader files are untouched)
```
source/engine/      mini-Unity (WP1, WP2): object.cpp transform.cpp component.cpp gameobject.cpp resources.cpp pack_glue.cpp animation.cpp skin.cpp
                    camera.cpp renderer.cpp backend_vitagl.cpp shaders.cpp (ShaderDesc table) textmesh.cpp font.cpp collider.cpp input.cpp time.cpp coroutine.cpp engine.cpp
source/game/        translated classes (WP3-6), 1 file per C# class, same names: NativeManager.cpp, Chara.cpp, FldObject.cpp, MenuParts.cpp ... + Global.hpp
source/native/      native_bridge.cpp: so_symbol() lookups + typed wrappers of the dq5core exports; packet_recorder.cpp (trace/replay)
source/platform/    vita_input.cpp (touch/pad -> Input), vita_present.cpp (logical-surface blit), dialog.cpp
shaders/            *.cg  (precompiled to .gxp at build, psp2cgc)
tools/convert/      (other agent) pack writer  ->  ux0:data/dqv/pack/{index.bin,meta.pak,mesh.pak,tex.pak,blob.pak}
```
Entry: `unity_boot.c` is replaced by `dqv_game_main()` (boot thread, >= 1 MiB stack): `vglInit...; Engine::Init(); Boot::Start()` etc. then `while(!quit){ Engine::BeginFrame(); Engine::RunUpdate(); Engine::RunLateUpdate(); Engine::Render(); Engine::EndFrame(); }` with swap interval 2.
Everything runs on ONE thread; libdq5core/libsdlib are called from it (their own threads: sdlib update + audio only).

## 1. Dropped classes and the result to fake where the game still calls them
| C# class | lines | decision | what the game expects / fake |
|---|---|---|---|
| `GoogleDrive`, `GoogleDriveDriver`, `UnityCloud` | 1073+355+58 | drop | called only by `NetConnection` |
| `UnitySQMK`, `AndroidSQMK`, `UnitySharedObject`, `SQMKOperation`, `ExpansionOperation` | ~220 | drop | only by `NetConnection` |
| **`NetConnection`** (MenuPort category 4, port opened by native `gMI_Network*` screens) | 684 | **replace by `NetConnectionStub`**: `class NetConnection : MenuPort` with `NoticeReceive`: `[0]`->`Send(0,0)` (status: not authenticated), `[1]`->`Send(1,801)` (no code list), `[2]`->`Send(1,408)`, `[10],[11],[13]`->`Send(1,408)` (408 = network error; this is what the C# sends when `internetReachability == NotReachable`), `[12],[80]`-> ignore. `Application.internetReachability` = NotReachable | save-file sync / campaign codes are unreachable offline - accepted |
| `ObbDownloadRes`, `ExpansionDownloader` | 509+131 | drop | `Boot` skips them since `dataPath` ends with ".obb" (always true in the port) |
| `UnityWebRequest`, `UnityWebResponse` (Midworld) | ~400 | drop | |
| `Test`, `MenuPortItemDebug`, `DebugDispColl`, `CONST.DEBUG` paths | 118+142+104 | drop | keep `CONST.Log/Assert` as `l_debug` |
| `NoNeon`, `MainLoopEx`, `FadeEx`, `HoppingNumberEx`, `EndrollEx`, `MenuPortSystemEx`, `ShakeFrameEx`, `Boot.IsNotNeonSupport` | ~150 | drop | NEON path only |
| `GetLangages` | 114 | tiny: `m_langagesNumber` = config (default 1 English), `Init()` no-op, `IsCanOpenScheme()` false | (Locale mapping: REPORT_JNI_DATA §4) |
| `AndroidJavaClass` users (`Boot`, `ApplicationQuit`) | | `loadFilelistFromAssets` -> done by loader; `UnityAndroidDialog.ShowDialog/m_result` -> `platform/dialog.cpp` (sceMsgDialog): `m_result` 0 pending, 1 OK, 2 cancel | |
| `LicensePolicy` | 45 | `OpenURL` = no-op (or show text) | |

## 2. Class map (kept classes)

Legend: WP = work package (section 3). "uses" = main engine features beyond Transform/GameObject.

| C# file | lines | -> module | WP | diff | uses / notes |
|---|---|---|---|---|---|
| `NativeCall`, `Neon` (firstpass) | 106+59 | `native/native_bridge.cpp` | 3 | 1 | function pointers via `so_symbol`; types per 01 §4 |
| `MainLoop` | 70 | `game/MainLoop.cpp` | 3 | 1 | Component with `StartCo` coroutine; order of SystemInits matters (01 §1.1) |
| `Boot` | 211 | `game/Boot.cpp` | 3 | 2 | logo coroutine; quit state machine; no OBB branch; calls `Engine::LoadLevel("dq5sp")` |
| `Global`, `CONST`, `Utility` | 39+66+45 | `game/Global.hpp` | 3 | 1 | `CameraInitParams[2]` table: (fov 9.5/48, angleX -47/0, dist 39.55, near 5/0.25, far 100/850, scaleW 0.0625, targetCamera true/false, view (0,.275,1,.42), bBGClear) |
| `NativeManager` | 894 | `game/NativeManager.cpp` | 3 | 3 | the 41 commands (02 §C); depends on every scene-object class API; implement commands against the public C# signatures |
| `Cleaner` | 42 | `game/Cleaner.cpp` | 3 | 1 | |
| `TouchPad`, `RawPad`, `RawPads` | 820+82+49 | `game/TouchPad.cpp` + `platform/vita_input.cpp` | 3 (+MenuParts) | 3 | pad word from buttons, keep ray/pointer logic and `ePadState` machine for touch (06) |
| `ApplicationQuit` | 102 | `game/ApplicationQuit.cpp` | 3 | 1 | dialog |
| `Sound` | 417 | `game/Sound.cpp` | 3 | 2 | `SdSoundSystem_*` via `so_symbol` on libsdlib_android; `FileReadAllBytes/FreeFileMemory` of dq5core; `s_streamPath` unused (AddStreamDataFromPath never called); `snd_vol.dat` 1179 floats; **int-only/pointer API except `SoundCtrl_SetVolume(int,float,int)` -> softfp wrapper** |
| `LinkList`, `TaskStack`, `UnityOBJList`, `ShakeFrame` | 56+57+33+21 | `game/util/*` | 4 | 1 | |
| `MenuPort`, `MenuPortSystem` | 146+205 | `game/MenuPort*.cpp` | 4 | 2 | verbatim mailbox (02 §D) |
| `eIndent` | 119 | `game/eIndent.cpp` | 4 | 1 | `SCREEN_HALF_HEIGHT = 225/Camera.main.aspect` |
| `MeshMaker`, `MeshList` | 239+130 | `game/MeshMaker.cpp` | 4 | 2 | quad accumulator -> Mesh; `MeshList::Load` BinaryReader format |
| `MenuParts` (+`FRAME_ENUM` 732 consts) | 1315+732 | `game/MenuParts.cpp` | 4 | 4 | biggest UI class: PartsInfo/IBase/IButton/IText/IFrame/IBar... TextMesh, colliders (Box/Mesh), 5 shaders; `Menu/frameData` TextAsset (MeshList format), `Menu/*` textures |
| `MenuMessage` | 292 | `game/MenuMessage.cpp` | 4 | 3 | creates `Texture2D(512,512,Alpha8)`; sends GL id to native; frame shake; speaker TextMesh (EN) |
| `MenuPortItem` | 346 | `game/MenuPortItem.cpp` | 4 | 3 | |
| `MenuPortItemTable` | 4066 | `game/MenuPortItemTable.cpp` (split in 4 files by `makeFuncList` entries) | 4b | 3 (volume) | 68 screen builders, tables of geometry; contains CPU-drawn minimaps (`Texture2D.SetPixels32`: world map 256x256 ARGB32 @2465, local map 1024x1024 RGB24 @2562) and `WorldToScreenPoint/ScreenToWorldPoint` uses (@168, 735); mostly mechanical |
| `HoppingNumber`, `BattleUpCursor`, `TapToStart`, `LicenseNobu` | 145+26+135+84 | `game/*.cpp` | 4 | 2 | |
| `InputName` | 787 | `game/InputName.cpp` | 4b | 4 | own camera (layer 10, `pixelRect` clipping), name-entry keyboard, many MenuParts |
| `SceneCamera` (+`InitParam`) | 338 | `game/SceneCamera.cpp` | 5 | 3 | projectionMatrix override, `rect`, LookAt/eulerAngles tweaks (01 §6, 03 §2.3), creates DSSA ortho camera + clear plane |
| `BackColor`, `Fade` | 97+209 | `game/BackColor.cpp` `Fade.cpp` | 5 | 1 | clip-space quads with `Camera` parents; frame counters |
| `BillboardObject`, `Chara`, `Riseup`, `MeshChara` | 31+461+116+108 | `game/*.cpp` | 5 | 3 | shared quad meshes, per-instance Material, `renderQueue`; MeshChara can be drawn directly from packet arrays (02 §E) |
| `ParticleSystem` | 106 | `game/ParticleSystem.cpp` | 5 | 2 | dynamic mesh from native arrays |
| `ChunkFile`, `FldObject` | 184+2607 | `game/FldObject.cpp` | 5b | 5 | `.fld` prefab + `.map` chunk file parse, own keyframe interpolation (`fld_motion_update`, `seq_*`, `pmv_*`, `emv_*`, `sra_*`, `plp` palette texture swaps), sepia/shader swaps, priorities, `rotateXZY`, RGB rate. Mostly numeric code - translate carefully (float semantics!). Cross-check against the DS original in `refs/DQIV-DECOMP/src/main/fld/FLDObject.cpp` (+ `include/main/fld/`) |
| `FbxObject` | 105 | `game/FbxObject.cpp` | 5b | 2 | needs `Animation` + skinning from engine |
| `Field`, `FieldSymbol`, `Volcano`, `BlinkMapSymbol` | 277+149+108+71 | `game/Field*.cpp` | 5c | 2 | world-map grid 25x40 quads; calls native `FieldSet` |
| `Capture` | 1115 | `game/Capture.cpp` | 6 | 4 | RenderTexture, `Camera.Render`, 5 coroutine effects, dynamic mesh grid. Coroutines -> C++20 |
| `Spotlight`, `Sepia` | 220+102 | `game/Spotlight.cpp` `Sepia.cpp` | 6 | 2 | `OnRenderImage`/`Graphics.Blit` |
| `Endroll`, `EndrollPort` | 281+98 | `game/Endroll*.cpp` | 6 | 3 | camera push/pop, textures `endroll/*`, `.bin` TextAsset |
| `CgController`, `SlimeTouchPort`, `SlimeTouchBehaviour` | 173+67+91 | `game/*.cpp` | 6 | 3 | Animation, box colliders, `DSSAOrtho` camera rays |
| `NetConnectionStub` | ~40 | `game/NetConnection.cpp` | 6 | 1 | see 1 |

Totals (kept): ~18.5k C# lines (4.1k of them are `MenuPortItemTable` + `FRAME_ENUM` tables).

## 3. Work packages (6) and the interfaces between them

Rule for parallelism: **header-first**. Day 0 deliverable of every WP = compiling `.hpp` files + stub `.cpp` (empty/logging bodies) for its public API, committed before anyone depends on it. Engine stubs (null renderer) must link on a PC host so non-Vita testing is possible.

| WP | scope | owner of files | depends on (API only) | first milestone |
|---|---|---|---|---|
| **WP1 Engine core** | `Object/GameObject/Component/Transform` (TRS, parenting, world matrices), `Instantiate`, active/layers, lifecycle (Awake/Start/Update/LateUpdate/OnDestroy, deferred Destroy, DontDestroyOnLoad, LoadLevel), coroutines runner, `Time/Screen/Application/Debug`, `Resources` + **pack loader** (wrap `dqv_pack.c`: textures->`Texture2D` blobs, meshes, materials, prefabs->GameObject trees, fonts, TextAssets, anim clips), `Mesh/Material/Texture` data classes, `Animation` evaluation + CPU skinning, colliders (raycast), math types | `engine/` core | `dqv_pack.h` (converter, other agent) | `Resources.Load("boot/sqex")` -> Texture2D; `Instantiate(scene/level0)` builds the 10 level0 objects; headless unit tests on host |
| **WP2 Renderer** | `Camera` (rect/depth/clear/cullingMask/projection/matrices, ScreenPointToRay etc.), render loop (gather, sort by queue/depth, state), vitaGL backend, logical-surface FBO + present (layouts, 06), `ShaderDesc` table + FFP state setup + 7 CG programs (04), texture upload/free (A8/565/4444/8888, wrap/filter), dynamic mesh submission, `RenderTexture`/`Camera.Render`/`Graphics.Blit`/`OnRenderImage`, **TextMesh + Font** (bitmap, 03 §5), GL-state hygiene vs dq5core calls, frame pacing (swap interval 2) | `engine/` render, `shaders/` | WP1 (components, Mesh/Material) | draw a textured quad + TextMesh text on the Menu layer at 408x544; `UnityRenderEvent(1)` canvas update visible |
| **WP3 Bridge, frame loop, platform** | `native/` (so_symbol bindings, DevInit, buffers, `packet_recorder` trace/replay), `MainLoop`, `Boot`, `Global`, `NativeManager` (+ all 41 cmds calling WP5 APIs), `Cleaner`, `Sound`, `TouchPad/RawPad(s)` + `platform/vita_input.cpp` (touch/pad synthesis, 06), `ApplicationQuit`+dialog, integration into `port/source/main.c` (replace `unity_boot`), CMake, config, logging, packaging of the pack path | `game/` glue, `native/`, `platform/` | WP1/WP2 stubs; WP4/WP5 headers | with stub engine: `GameInit/GameUpdate` loop runs on Vita3K, trace of first 300 frames recorded; then Boot logos |
| **WP4 UI / MenuPort** (4a framework, 4b screens) | 4a: `MenuPort`, `MenuPortSystem`, `MenuPortItem`, `MenuParts`+`FRAME_ENUM`, `MeshMaker/MeshList`, `eIndent`, `MenuMessage` (text canvas handshake), `TapToStart`, `HoppingNumber`, `BattleUpCursor`, `LicenseNobu`, utilities. 4b: `MenuPortItemTable` (68 screens), `InputName` | `game/` UI | WP1 (MeshFilter...), WP2 (TextMesh, shaders), WP3 (Global.Flag, Sound, TouchPad ray) | title screen: gMI_TitleTouch + start menu; then message window with text from native |
| **WP5 Scene content** (5a sprites, 5b map models, 5c world map) | 5a: `SceneCamera`, `BackColor`, `Fade`, `BillboardObject`, `Chara`, `Riseup`, `MeshChara`, `ParticleSystem`; 5b: `ChunkFile`, `FldObject`, `FbxObject`; 5c: `Field`, `FieldSymbol`, `Volcano`, `BlinkMapSymbol` | `game/` | WP1, WP2 | town: map models + billboard NPCs + camera; world map; DSSA meshes |
| **WP6 Effects & extras** | `Capture` (+ screen effects), `Spotlight`, `Sepia`, `Endroll/EndrollPort`, `CgController`, `SlimeTouch*`, `NetConnectionStub` | `game/` | WP2 (RT, OnRenderImage), WP4 (ports), WP5 | battle transition, spotlight in dungeons, ending credits |

Interfaces (exact):
1. **Engine ABI** = `miniunity.hpp` (WP1 owns it; WP2 may add backend-only headers). Changes via PR notes in the header.
2. **Scene-object APIs consumed by `NativeManager`** (owner WP5): same public statics/methods as the C#: `Chara(int)`, `SetPosition/SetColor/SetScale/SetPattern/SetTexture/SetVisible/SetFieldPosition/...`, `Chara::SetSleepNear/SetPriorityShader/UpdateBillboardShadows/CalcLieBillboard`, `SceneCamera::{SetPosition,SetTarget,SetAngleDeg,SetNear,SetFar,SetFOV2,SetYOffset,SetLookDownAdjust,SetTargetCamera,Update,s_camera_pos,s_billboardRot}`, `Fade::{SetLayer,SetFadeOut*,SetFadeIn,IsEnd}`, `BackColor::SetColor`, `MeshChara`, `Riseup`, `FbxObject`, `FldObject m_fld` (`Load/Term/Update/SetMapObj*/SetCommonAnimation/...`), `Field::*`, `ParticleSystem::s_funcs[]`, `Capture::ScreenEffect`.
3. **UI <-> bridge** (WP3/WP4): `MenuPortSystem::{SystemInit,ResetNativePacket,Send,Receive}`; `TouchPad::{m_press,m_toggle,m_externalToggle,GetPressRay,SetPosPadSize,SetInputMode,SetPadBlink,m_bToggleESC}`; `Sound::*`; `Global::Flag`.
4. **Native bridge** (WP3): `NativeCall::*` function pointers with the signatures of `Neon.cs`; arrays are file-static (never freed).
5. **Pack contract** (WP1 <-> converter agent): see section 5.
6. **Test harness** (WP3, used by all): `packet_recorder` writes `ux0:data/dqv/trace/NNNN.pkt` (flag, out ints until -1, out floats, strings, menu buffers both directions, pad word) for every frame while `DQV_TRACE=1`; `--replay` feeds recorded frames into `NativeManager::Update` without libdq5core (and a PC host build with a software/SDL GL backend can render them) => UI/scene packages can be developed and regression-tested without the Vita or the native lib.

## 4. Order of work (critical path to "title screen" then "walk on the first field")

```
M0  (all, day 0-2)  headers/stubs, build integration, pack path config, packet recorder skeleton.   
    WP1: GameObject/Transform/Resources(Load texture+mesh+material).   WP2: backend init (vglInit, FBO, present), one textured quad, FFP states.
M1  Boot logos + native alive           WP3: DevInit/GameInit/GameUpdate loop, Global.Flag, Boot coroutine.   WP2: Picture shader quad + alpha fade;  Time/Fade.
    -> Vita3K shows SQEX + Arte logos, then black; native log shows GameUpdate ticking; Sound.SystemInit OK.
M2  TITLE SCREEN                        WP4a: MenuPort mailbox, MenuParts(+MenuPartsInfo from Menu/frameData), MeshMaker, MenuPortItem + the screens gMI_TitleTouch / gMI_StartMenu*/YesNo/Config (subset of the table), TapToStart, MenuMessage minimal.
                                         WP2: TextMesh+FontAlpha8; MenuCamera ortho; touch->ray.  WP5a: BackColor, Fade, SceneCamera (title uses a FLD/DSSA background? -> may need WP5b minimal).  WP3: TouchPad pad bits, Sound.PlayBGM.
    -> title: logo, "TOUCH to START", menu "Continue/New Game", BGM.
M3  NEW GAME -> TEXT                     WP4a: MenuMessage + canvas texture handshake (native TextCanvas -> glTexSubImage2D) ; InputName (WP4b) for the hero's name.
M4  FIRST FIELD (walk)                  WP5a+5b: Chara/billboards, FldObject Load + models + shaders fld_*, SceneCamera real cameras, camera ops, ChunkFile; WP3: NativeManager all fld/chara/camera ops; pad movement.
    -> DQ5 begins on the ship deck (town/FLD "map/..."): walk with d-pad, talk (B), command menu (X), BGM/SE.
M5  Menus (status/items/shops/save)     WP4b complete table.  M6 Battle: DSSA ortho + Capture transition + battle menus (WP5a MeshChara/WP6).  M7 world map (WP5c), effects (spotlight, sepia, tabi), ending, minigames (WP6), polish (TATE, pointer cursor, performance).
```
Everything before M4 can be developed in parallel because of header-first + trace replay. The riskiest items (schedule early): (1) `UnityRenderEvent` GL-state interplay and A8 texture `GL_ALPHA` in vitaGL (WP2+WP4a, M2/M3), (2) FFP vs CG shader coverage + vitaGL FBO behaviour (WP2, M1), (3) FldObject numeric fidelity (WP5b), (4) Texture memory (RGBA8888 conversions of RGB24/RGBA32: 4+ bytes/pixel; converter should prefer RGB565/RGBA4444 where the original was 24-bit/low colour - already does for some), (5) `Camera.aspect`/rect semantics, (6) `GetComponentsInChildren` inactive semantics in FldObject.

## 5. Contract with the converter (`/Users/frangar/Fun/dqv/tools/convert/dqv_pack.h`, read, not edited) - what the runtime needs and what to verify
Already provided (header inspected): index with lower-case Resources paths -> kinds (texture/mesh/material/shader/prefab/anim/text/font), prefab node trees (TRS, layer, active, tag), MeshFilter/MeshRenderer/Skinned/Animation/Camera components, generic property lists for MonoBehaviour/TextMesh/colliders (`DQV_P_LOCAL` references for TouchPad's fields), meshes with u16 indices + optional skin, materials with shader name/queue/tex envs/colors/floats, shader state (blend/zwrite/ztest/cull/alphatest), fonts with `CharacterRects`, anim clips with tangents, scenes (`scene/level0`, `scene/maindata`).
Runtime requirements to confirm with the converter owner:
1. `Texture2D.name` = original asset name (the `.tim`/`.tm2` suffix is used by FldObject logic) and `orig_format` = Unity `TextureFormat` id (RGBA32 vs RGB24 decides `fld_atest` vs `fld_normal`). Both fields exist (`DqvObject.name_off`, `DqvTexture.orig_format`).
2. `Material.name` preserved; `Material.shader_name` must be the declared name (`"Custom/fld_trans"`...) or `builtin:<file>:<id>` - runtime maps `builtin:unity_builtin_extra:7`, `Alpha-Diffuse`, etc. to `MUL` default descs (04 §3.2: FLD swaps them anyway).
3. Font objects for the 5 bitmap fonts (`menu/se-garrir` etc.: font + atlas texture + material) with all `CharacterRects` (JA font: 9911 entries); skip `mplus-2c-medium_16` TTF (1.38 MB) unless cheap.
4. TextAssets raw: `*.map` (FLD map chunk files), `menu/framedata`, `menu/map/*`, `endroll/*.bin`, `boot/quitmsg`, `menu/license_list_*`.
5. Texture wrap/filter flags (`wrap_mode`, `filter_mode`) as in Unity (Repeat is needed for the UI atlases - negative V).
6. Prefab `Animation` component: default clip + clip list; clip names are "Take 001".. (FbxObject builds `"Take " + (1+n).ToString("D3")`) - the clip **name** must be preserved (Object name of AnimationClip) - the header's `DqvAnimationData.clips` are gids; their `DqvObject.name_off` provides the name (verify non-empty).
7. Skinned meshes: bone node indices, bind poses (column-major) and weights present (`DQV_MF_SKIN`), root bone.
8. Optional but useful: an `index.txt` dump (exists) and a `--only <prefix>` mode to build small packs for tests.
9. Memory-aware loading: `dqv_read_part` for textures (stream pixel data straight to the GL upload buffer), `tex.pak` kept open (no per-asset fopen on the SD card more than needed).
