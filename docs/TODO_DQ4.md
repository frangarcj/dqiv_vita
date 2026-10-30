# DQ4 bring-up: what is disabled, unresolved or different from the DQ5 port

(Foundation package, 2026-10-30. Update this file when an item is done.)

## Disabled DQ5-only features (each logs a message at start-up)
| item | why | what to do |
|---|---|---|
| `field_patch` (`native/field_patch.cpp`) | **DONE** (see "World map" below): `FieldData::drawBlock` of libdq4core re-derived, 3 ARM immediates, 59 columns | - |
| `DevCheats` (`nodamage`, `noencount`, `gold`, `party_hp`, `party_mp`, `warp`) | `_ZN3btl7Encount4brewEv`, `_ZN6status18PartyStatusUtility17setNoDamageEnableEb` do not exist in libdq4core; `DebugSetValue` ids are DQ5's | flag `NativeCall::kDevHooksVerified` (native_bridge.hpp) gates it, `WarpTest::Create` and `BattleTest::Create` |
| `WarpTest` | uses `Global::startTown(int)`, `loadGame`, `startSlimeRace/Touch/OpeningStaff/Staffroll/Fin`, `getMapNameById`, `FieldPlayerManager` which are missing in libdq4core (checked with readelf: only `startField/startTitle/startCasino/startDebugTown/startFirstTown/startSurechigai/startBook/startTown(char const*)` exist) | re-derive, then flip `kDevHooksVerified` |
| `BattleTest` | `DebugSetValue(8/12, ...)` semantics unverified for DQ4 | same |
| `ResetNativePacket`, `MenuPortResetNativePacket`, `ParticleSystemInit/ResetNativePacket`, `FieldSet`, `LoadFileList_JNI` | exports absent in libdq4core (DQ5-only) | bound as optional (null); all callers guarded. DQ4 `Field.cs` fills the tile UVs itself: Field port must do it. |
| `ParticleSystem` class | no such class in DQ4 | files deleted; ops 40 (`particle`) gone |
| `Endroll*`, `SlimeTouch*` | no such classes in DQ4 | files deleted (FxTest keeps only the effects that exist) |

## Still in the tree but dead for DQ4 (remove when you touch them)
`Spotlight`, `Sepia` (screen effect class; the DQ4 FLD sepia *shaders* exist), `Volcano` (used by `FieldSymbol`), `CgController` (was in SlimeTouch, gone), `Capture::ScreenEffect` still has the DQ5 spotlight sub-commands (reads `NativeManager::GetFloatEx`), `MenuPortItemTable::Make_SlimeTouch_*`, `PadFocus`/`Layout` hooks that name DQ5 screens.

## Packet / opcode differences found (NativeManager.cs, 38 vs 41 commands)
DQ5 ops removed in DQ4: 8 `fld_etc`, 28 `mesh_ortho`, 40 `particle`. DQ4 numbering (every later op shifts down): 0 fld_load 1 fld_color 2 fld_alpha 3 fld_pos 4 fld_rot 5 fld_common_anim 6 fld_event_anim 7 fld_on_off
8 back_color 9 fade 10 camera 11 camera_sub 12 chara_load 13 chara 14 chara_on_off 15 chara_shadow 16 chara_sleep_pose 17 chara_sleep_near 18 chara_pose 19 rise_icon_load 20 rise_icon 21 mesh_load
22 mesh 23 mesh_on_off 24 mesh_shader 25 mesh_color 26 mesh_tex_offset 27 field_load 28 field 29 field_symbol 30 field_scale 31 fbx_load 32 fbx 33 map_color_rate 34 screen_effect 35 se_play 36 se_stop 37 bgm.
Operand changes: `fbx` bit 1024 = force-play flag of `PlayAnimation(seq, bForce)`; `chara_load` id masked `& 0xFFF`; `fld_common_anim` always queues the anim for the end notification (DQ5 only for `num>>16 >= 2`);
`fld_load` resets the map camera before `Term()`; no `Chara.SetPriorityShader()`; `m_dssa` is 24 slots (DQ5 40); no `ReleaseStage`/`GetIntEx`/`GetFloatEx` in the C# (the C++ keeps `GetIntEx/GetFloatEx` for `Capture`).
MenuPort buffers: host->native 129 shorts, native->host 1089 (65 + 1024) instead of 128 / 2048; port-0 command 7 (`MenuParts.HIRO_NAME_SCALE = p2/p3`) added, command 9 (`SetInputMode`) removed; category 3 = `StaffrollPort(port)` (DQ5 `EndrollPort(port,type)`), category 5 gone.
`tools/pktcheck.py` was updated to the DQ4 table (it translates to the DQ5 operand walker; slots: mesh 24).

## Boot-flow deltas already applied
Boot (Resources `Title/` instead of `Boot/`, `quitmsg`, no `loadFilelistFromAssets`, no `deviceInfo`; `s_deviceModel`/`s_gpuName` fixed to "" / "POWERVR SGX543MP4+"), MainLoop (no ParticleSystem, `Field.SystemInit`, FLAG_CLEANER 0.5 s after boot, no ResetNativePacket on resume),
Global (`CameraInitParams[1]`: far 500, last arg 0.04; no `FLAG_SYSTEM_GUI_LOCK`), Cleaner (`Make(end)`/`Task_Cleaning`), Fade (Start coroutine waits for FLAG_PAUSE), SceneCamera (no DSSA ortho camera, projection matrix * `s_scaleW`, `s_bFixAdrendo` flag only; the DQ4 non-Adreno
clear-plane *camera* path is NOT ported, the DQ5 mesh plane is used), RawPads (array, `Count`, idx), TouchPad (`m_upToggle`; the virtual pad internals stay replaced by the physical-button word), TapToStart (waits 720/690, copyright fades out after 2508), FbxObject `PlayAnimation(seq, bForce)`.
Not ported (not needed for the title): `MainLoopEx`/`FadeEx`/`MenuPortSystemEx` (non-NEON path), `MenuPortSystem.OnGUI` (debug), `TouchPad` virtual-pad code (`SetPadPos`, `CreateMenuButton`, `AdjustMenuZPos`), `Field.UpdatePattern` (stub), `Field.SystemInit` material, `Sound` (DQ5 state machine still in use: `s_state/s_bgmNext` of DQ4 not ported), `StaffrollPort` (UNPORTED, logged).

## Converter differences (Unity 4.6.5f1)
Worked with UnityPy 1.25.4 unchanged except the MonoBehaviour layouts in `dqivconv/prefab.py` (`MB_SCHEMAS`): `Boot` order is `m_logoTexture[], m_logoShader, m_quitmsg, m_obbMenuParts, m_obbFont[], m_obbMat[], m_obbmsg`;
`TouchPad` has an extra serialized int `l_state`. Remaining warning: `DebugViewer` has no schema (debug script, irrelevant). The APK has no `assets/filelist.bin`: DQ4's libdq4core parses the OBB zip itself (`InitFileLoader`), libsdlib's `AAssetManager_open` table is built from the OBB zip directory at runtime
(`native/asset_manager.c`). `tools/make_data.py` was only patched for the OBB number/md5 (main.106) and the Unity file count; its `extract_libs_and_assets` still copies `filelist.bin`/`ct.inf` from the APK and `PACK_SHA256` is still DQ5's: TODO before shipping.
Resources paths: 17 761 unique (DQ5 21 609), 2 791 common. `Boot/*` -> `Title/*` (`Title/sqex`, `Title/arte_logo720_240`, `Title/TapToStart`, `Title/Retro4_copyright`, `Title/potalbutton`), `Boot/quitmsg` -> `quitmsg`, `Menu/Boot/*` for the OBB download UI,
world maps `Menu/map/DSDQ4WMap01(m)` + `DSDQ4LMap0_1024` instead of `Menu/map/DQ4DS_*` ... full lists in `work/re/resources_paths.txt` (not committed).

## Gotchas learned
* Vita3K's `sceClibVsnprintf` crashes on `%g` (strlen of garbage): use `%f`/`%d` in `l_info`.


## World map (done, DQ4 re-derivation)
`FieldData::drawBlock()` (`_ZN9FieldData9drawBlockEv`, .so 0x14178c) is the DQ5 `fld::FieldData::drawBlock` of the older codebase: it writes packet op 28 (`field`: startX, startY, width = tiles of the first row, count, tile ids) for tile offsets -4..+20 x -5..+32 around the view's base tile (25 x 38 tiles). There is **no native `FieldSet`**: DQ4 `Field.cs` fills the tile UVs itself (`SetIndexList`, row stride 25 quads) and animates sea/river tiles in `UpdatePattern` (`m_patternInfo/m_patternCheck`), so the host owns the mesh and UV layout (`Field.cpp` ports both; the stride is the mesh width).
`field_patch.cpp` patches three words, each checked before it is written: `+0x1417ec sub r4,r4,#4 -> #18` (first column / packet startX), `+0x1418c4 cmp r2,#20 -> #40` (last column), `+0x1418e0 sub r4,r3,#4 -> #18` (column reset per row): 59 columns x 38 rows = 944 x 608 units, the host mesh is 59 x 40 quads (`NativeCall::FieldCols`). Same numbers as DQ5 (back 18 / forward 40). `Field/field` is the 1024 x 1024 atlas (`c_oneGridSize` 0.0156 = 16 px / 1024).

## Pad word (checked against libdq4core)
`window::NormalControl::checkCamera` (0x1791a0 in Ghidra) reads `Global+0xd4 & 0x30` (the `press` argument of `GameUpdate(press, toggle)`): 0x20 -> `TownPlayerManager::setCameraRotType(1)` (rotate R), 0x10 -> type 2 (rotate L), 0x30 -> `setCameraRotToNorth`. These are exactly the host bits of `vita_input` (L = 0x10, R = 0x20, L+R / R3 = 0x30; the touch ROT_OR_FLICK `0x70/0xB0` flick words also contain 0x30). The DQ4 `TouchPad.cs` constants are the DQ5 ones (A 0x200, B 0x400, X 0x800, Y 0x1000, ANY 0x100); there is no field rotation in DQ4 (`FieldPlayerManager` has none), rotation only exists in towns/dungeons.

## Shaders (DQ4 pack: 45 shader objects)
Not in DQ4: Sepia, Pre/Post-Spotlight, Particle, FieldVolcano, DssaNormal, Bssa*, ImdShadeChara(2), Field_iOS (table entries and the `dqiv_sepia_scr` CG program removed). New in DQ4: `Custom/CharaSleep`, `Custom/CharaSleepEx` (Chara lying pose): vertex `clip.xy = (mvp*v).xy`, `clip.zw = (VP * _sleepMatrix * v).zw`; implemented as `OP_SLEEP` with the CG programs `source/shaders/dqiv_sleep.{v,f}.cg` (blob `extras/cg/dqiv_sleep.bin`, same mechanism as gray / sepia_fld; fixed-function MODULATE fallback if the blob is missing and libshacccg is absent).
