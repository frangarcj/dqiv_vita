# DQ4 bring-up: what is disabled, unresolved or different from the DQ5 port

(Foundation package, 2026-10-30. Update this file when an item is done.)

## Disabled DQ5-only features (each logs a message at start-up)
| item | why | what to do |
|---|---|---|
| `field_patch` (`native/field_patch.cpp`) | DQ5 offsets of `fld::FieldData::drawBlock` / `FieldSet`; libdq4core exports neither (older codebase) | re-derive the DQ4 world-map tile block with `tools/re/decomp.sh`, then re-enable. `g_fieldPatched` stays false: 25 columns. |
| `DevCheats`, `WarpTest`, `BattleTest` | **Done (harness branch)**: re-derived from `DebugSetValue` / `DebugSetMapName` / `Global::start*` / `Status::setEventParty` of libdq4core (event table `param::Event`: `DebugSetValue(1, id, female)`; battle `DebugSetValue(8, ...)`; `PartyStatus::setNoDamageEnable`; `DebugSetValue(10, 0, 2)` for no encounters), verified in Vita3K, see README "Warp harness and Coverage" and DEVELOPERS "Battle" / "Tester cheats". `NativeCall::kDevHooksVerified` is now true. | not reachable: monster arena, shops, bank, vehicles (script driven), `loadGame` (no exported equivalent; use `chapter N`) |
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
