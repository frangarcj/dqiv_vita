# 07 - Landscape (960x544) mode: feasibility, DS heritage, designs, prototype

Status 2026-10-27. Prototype: branch `land` of `native/` (worktree `/Users/frangar/Fun/dqv/native-land`, commits 002b195..18abd40), build with `-DDQVN_LANDSCAPE=ON` (`cmake -DDQVN_SLOT=6 -DDQVN_LANDSCAPE=ON`), screenshots in `re/arch/land_shots/`.
Verdict: **feasible, cheap for field/town/title/menus (prototype already plays the New Game flow in landscape); battle, world map and a few effects need targeted work.** Supersedes the "rejected" note in 06 §1 (which assumed the UI had to be re-laid out; it does not).

## 1. Where layout is decided (evidence)

Native side is **resolution agnostic**: `DevInit(obb, savedir, lang, devInfo)` gets only language + a PowerVR bit (`DevInit` @0xe79a4: `devInfo&0xf -> bit0`, `&0xf0 -> bit1`); no screen size, aspect or viewport crosses the packet interface. The only layout-dependent values native receives are the MenuMessage handshake flag `SendInts(texId, fmt, 512, 512, aspect > 0.74 ? 1 : 0)` (MenuMessage.cs:123 - "tablet" text-canvas layout) and `UnityInitTouchFlag`. Native sends camera *parameters* (pos/target/angles/near/far/fov2/yOffset/lookDownAdjust via ops 11/12), never matrices. So the whole layout lives in the C# we already ported:

| C# | what | landscape consequence |
|---|---|---|
| `SceneCamera.Update` (SceneCamera.cs:210) | `Perspective(s_fov*4/3, 1/aspect)` with [0,0]<->[1,1] swapped => **horizontal FOV fixed (= DS horizontal), vertical grows with the screen** (portrait 9:16 = 256 x 455 units, DS bottom camera continued upward) ; `identity[1,3] = s_yOffset` | in landscape the same code shows 256 x 145 units: 2.35x zoom, cropped (see `land_naive_title_f900.jpg`) |
| `SceneCamera.SetViewAndCenterY` (:313) | battle camera (`Global.CameraInitParams[1]`: view y 0.275, h 0.42, `bBGClear`) is a rect scaled by `aspect/0.5625` so the viewport is always **4:3 = the DS screen**, black clear plane above/below | height becomes 1.32 (>1) in landscape: broken |
| `SceneCamera.SystemInit` :184 | DSSA ortho camera `size = 128.53125/aspect` (257 units wide, DS pixels) | needs the design aspect |
| `TouchPad.AdjustCameraOrtho`, `Boot.cs:109,166`, `eIndent.Init` (:33), `InputName` :716-740, `MenuPortItemTable` :2687, `FldObject.cs:1493` (`-10.24*(aspect-0.5625)`), `Capture.cs:180`, `Spotlight.cs:172`, `Field.cs:107` (`128/aspect`) | all derive from `Camera.aspect` (UI: `225/aspect` half height; UI width is the fixed **450 units**) | |
| `MenuMessage.cs:154` `canvasYDif*450/Screen.width`, `MenuMessage.cs:123` | | use panel width, tablet flag |
| `SlimeTouchPort.cs:32` | `if (2/3 < dssaAspect) y -= 192 - size`: aligns the **DS 256x384 stack** to the screen bottom | works unchanged with design aspect 0.75 |

Native visibility culling (checked): `ardq::DisplayCharacter/BillboardCharacter::setNearFlag`, `args::DSSAObjectWithCamera::execNear` (DQIV `src/main/object/DSSAObjectWithCamera.cpp`) are **depth tricks** (pull sprites toward the camera, scale /16), not screen culling; `setSleepCharacter` = lying-down pose. There is no per-object screen test in the C#: `FldObject.cs` only has `obj_visible(m_obj_tbl[i] != -1)` (the DS `isMapObjInTownView` distance/frustum cull of DQIV `FLDObject.cpp:2304` did **not** survive into the Unity port; Unity frustum-culled instead, our engine culls by layer only). Prototype evidence: throne room and bedroom at 960x544 show the whole widened scene with NPCs/walls beyond the old 256-wide window (`land_overlay_throne_f1800.jpg`, `land_overlay_bedroom_f2900.jpg`): no holes, no missing NPCs.
Hard native limit found: world map `fld::FieldData::drawBlock` (Ghidra 0x172e04, file 0x162e04) **always emits a fixed 25 x 38 tile block** (columns -4..+20, rows -5..+32 from the base tile; corrected in 8) = 400 x 608 units, host mesh is 25 x 40 quads (`Field.cs` `c_quadNum 1000`, `FieldSet` writes `Vec2[4000]`). Portrait needs 256 x 455; a 16:9 view at the portrait vertical extent needs 602 x 341 -> **holes**. Fix without patching native: limit the field camera to <= ~300 units across (done in the prototype: `Field::Init`, `min(128/0.75, 150/aspect)`; verified only by calculation, the world map was not reached in a run). Patching the 4 loop constants + enlarging the host mesh/`FieldSet` buffer is the "real" fix (later).
What breaks if the world camera is simply 960x544 (`land_naive=1`): x2.35 zoom, UI stretched to 960 wide with 254 units of height (menus anchored to a 600-unit-high canvas are cut), battle rect > 1, Capture RTs fine (just bigger), fade quads fine (clip space), `BackColor`/clear plane fine (clip space).

## 2. UI

* MenuParts/MenuPort windows are placed **by eIndent anchors on the 450-unit canvas** (`CalcScreenPosition`: x = -225/0/+225, z = +-`SCREEN_HALF_HEIGHT` = 225/aspect); nothing in the native side knows the canvas. Message window = `MenuMessage.canvas_data` (indent + canvasYDif), command menus, battle menus, name entry, shop: all MenuPortItemTable builders use these anchors. => The UI can live unchanged in **any rect with aspect >= 0.5625**; the game's own tablet layout (0.75 = 408x544) is ideal.
* Proven in the prototype: `MenuCamera.rect` = 408x544 panel at x=552 (aspect becomes 0.75 by itself through the rect-aware `Camera.aspect`), `eIndent`/`MenuMessage`/`InputName` read `Layout::Aspect()`. Start menu and the **name-entry soft keyboard work with touches** (`land_overlay_startmenu_f1100.jpg`, `land_overlay_nameentry_f1450.jpg`: script taps at x+552 typed "abc"). TouchPad ray hit tests use `ScreenPointToRay(MenuCamera)` over `pixelRect` so taps map correctly with no extra code.
* `Fade` planes are children of the MenuCamera (clip-space quads) -> with a panel-sized camera they would only fade the panel; the prototype gives them two full-surface cameras (BG fade depth 0.9 behind the UI, front fade depth 5). Same applies to anything else that must cover the whole screen from layer 8: `SceneCamera.s_clearPlane` (battle black bars), `Capture` quad, Sepia/Spotlight overlays (not yet checked).
* Screens that assume portrait *height* (600 units): all of them, but all fit the 0.75 panel exactly like the Android tablet branch.
* Virtual pad (`MPad` meshes) is not built in the port; physical buttons per 06 §3.
* Side effects to handle: tap slop uses panel width (`DQV_UI_W`); `Screen.width` consumers (MenuMessage.cs:154) use the panel width; `DemoOverlay` (dev aid) paints a dark panel + copyright text across the surface (visible in split mode, delete it).

## 3. DS heritage: what survives in libdq5core

Evidence (DQIV-DECOMP + Ghidra on libdq5core):
* DS display model = **one virtual 256 x 384 space**: DQIV `getOamBufferAtY(0)` (main/top) and `getOamBufferAtY(192)` (sub/bottom), `getTextBgAtY(odd*192)`, `ScreenPosition`; `DssDisplay` plugins `STOP/CAPTURE/SINGLE3D/DOUBLE3D/TEXTONLY`. Towns/dungeons use **DOUBLE3D**: the 3D engine renders the *bottom* screen on odd frames and the *top* screen on even frames with `DualCamera` (camera0/camera1, `m_cameraNo = frame&1`, same projection, different centre), the other screen shown from a VRAM capture. The picture is a 256x384 continuous scene with the hinge in the middle.
* libdq5core still has the 2-screen coordinate model in the 2D/minigame code: `MG_SetUpScreen(bool)`/`MG_IsUpScreen()` (exports, Ghidra 0x199e28/0x199e3c, flag set around every `slime_draw`/`sideInfo_draw`/`ssa_eff_update`), `SetupSoftwareSpriteCamera` (0x19c660): `if (!MG_IsUpScreen()) MG_G3_Translate(0, 0xc0000 /*192.0 fx32*/, 0)` = **bottom screen = +192 px**; `bg_manage` clears a `0x100 x 0x200` (256x512) texture area; `Dssa_G3_SetDualParts/SetDualTexOffset` (dual texturing). DSSA sprites/quads (title, battle monsters, effects, minigames) are therefore in DS pixel units, `s_DSSAOrthoCamera` (257 wide) is the "DS pixel camera".
* The DS **menu/window system** (`ardq::MenuManager`, `MenuDisplays.message_/frames_/...`, OAM/BG windows, `window::*Control`) is **not** what the mobile build uses: the mobile UI is `MenuPort` (host-drawn parts, `gMI_*` tables, `MenuItem_Init_*`, `NameSetMenu`) and the message text goes to a native-rendered 512x512 A8 text canvas (`TextCanvas`). There are no top/bottom assignments left in the UI protocol. => a "bottom screen UI / top screen world" split exists only as DS history; the mobile port already **fuses** them: the 9:16 phone picture = both DS screens joined vertically (256 wide, extra height), the tablet 0.75 branch = a bit less; the battle uses only the 4:3 "top screen" viewport band (DS_ASPECT 1.3333 in SceneCamera.cs, `m_aspect = 0x1555` in DQIV `Camera`).
* Consequence: DS-style side-by-side (world "top" left, UI "bottom" right) is natural **as a layout** (design B below) but native does not require it; the vertical fusion is what the Unity code implements and what DQIV-Recompiled also does.

## 3b. DQIV-Recompiled (PC native DQ4 DS, same engine) -> DQ5 equivalents

(`refs/DQIV-Recompiled`: both DS screens in one 16:9 picture, scene widens at the sides.)

| DQIV-Recompiled | what it does | DQ5 mobile equivalent (where) |
|---|---|---|
| `DualScreen.cpp extendProjection()` (called from `dss/Camera.cpp:102`): `x' = x*256/canvasWidth`, `y' = (y-w)/2 + centre*w/2` on the bottom camera's projection | continues the bottom camera over the top screen and widens horizontally | **`SceneCamera.Update` matrix** (SceneCamera.cs:210). The Unity port already did the vertical continuation (fixed horizontal FOV); landscape = change M00 (prototype: `M11 = f*0.75/bandH`, `M00 = M11/realAspect`). `unifiedCentre` (party centred) <-> `s_yOffset` (`identity[1,3]`, native op 12 flag 16) |
| `TownCamera.cpp:35` zoom "narrows or widens the lens rather than moving the camera: fog/culling/sprite sizes stay" | camera zoom 50-200 % | scale M00/M11 (and `s_fov`) in `SceneCamera.Update` after native pos/target; never touch `s_position` (native reads `s_camera_pos` back each frame: op 11 echo) -> an easy later feature |
| `FLDObject.cpp:1466` `pc::unifiedFrame() \|\| isMapObjInTownView` (distance cull tuned for one screen) | disable cull | **none needed**: Unity port has no such cull (`FldObject.cs` obj_visible only), our engine has no frustum cull either (verified in screenshots). If a perf cull is added later, base it on the widened frustum |
| `FLDObject.cpp:1317` `StretchScope(backdropStretch)` for map `ev01` (opening sky made for DS width) | stretch backdrop geometry horizontally | **`FldObject.cpp`** (our port) for the sky/backdrop map objects of the opening/title (`map/ev/ev17` is the title backdrop; it looked fine at 960: castle + gradient + clouds fill the width, but opening camera pans (ev events) are untested -> test; fix = X scale of the root/sky objects) |
| `Camera.cpp` `pc::wideBattleFrame()` + `BattleMonsterNamePlate/BattleMonsterMask`, `battleZoom`, `snapBattlePoint` | 16:9 battle: scene zoomed so monsters fill the room between windows/commands, plates follow monsters | **`Global.CameraInitParams[1]` + `SceneCamera.SetViewAndCenterY`** (battle band; prototype keeps the 4:3 band height `l_sceneH`), name plates/damage numbers: `MenuPortItemTable_Battle.cpp:648` `MainLoop::m_camera->WorldToScreenPoint -> TouchPad::m_camera->ScreenToWorldPoint` already tracks world positions across cameras, so plates follow any viewport; DSSA enemy sprites: `s_DSSAOrthoCamera` (size/rect) |
| `wideFieldFrame`, `fieldExtendX/Y`, `zoomFieldRect` | world map drawn over the whole canvas | native `FieldData::drawBlock` fixed 21x33 block (see 1) + `Field.cs` 25x40 mesh: needs either camera limit (done) or patch+bigger mesh |
| `panelLayout` (logos/casino/book/log menus: both screens side by side) | scenes without a world | not applicable: those are MenuPort portrait UIs; they sit in the panel |
| `Settings` F8 16:9 vs DS width, `canvasWidth` | user setting | cfg `land_*` (prototype), later `layout=` in `dqvn.cfg` |

## 4. Designs

All use one logical surface 960x544 (no FBO change; the engine already supports `Camera.rect` with scissor and rect-aware `aspect`).

**A. Overlay (prototype default `land_world=full`)**: world cameras full surface, vertical extent = tablet (341 units), horizontal 602 units; UI camera = 408x544 panel at the right (or left) over the world. Pros: whole 960x544 used, nothing letterboxed, DS-like "scene widens at the sides" (like DQIV-Recompiled), message windows still sit at the bottom of the panel; UI panels already have opaque frames. Cons: the hero/title composition is centred at x=480 so the panel covers the right 43 % of the picture -> shift world centre left (projection x offset, trivial: add `identity[0,3]`) or keep the party left of the panel; world map needs a tighter zoom (block limit); the panel has no backdrop (empty gaps show the world). Touch: unchanged (panel only reacts inside its rect; taps on the world area = `PAD_ANY` -> A, same as before - decide whether to ignore them). Effort: **done for field/title/menus; 2-4 days to finish** (battle band, world map zoom/camera shift, Capture/Sepia/Spotlight/clear-plane cameras, DemoOverlay removal, panel backdrop art). Risk: low-medium.

**B. Split (`land_world=split`)**: world left 552x544 (aspect 1.015 -> 346 x 341 units, fits the 336-wide world-map block almost exactly), UI panel 408x544 right = DS "top screen + bottom screen" side by side. Pros: nothing hidden, world map works with the portrait framing, easiest battle (band 552 x 414 = 4:3 + a bar), natural touch separation (touch only on the panel, left = optional pointer), best fidelity to the DS idea. Cons: world less wide (and 43 % of screen is UI even when no menu is open - fill with art/map/status like the DS bottom screen); fades need the full-surface cameras (done). Effort: same code as A (a cfg flag) + panel artwork: **0.5 day on top of A**. Risk: low. Prototype screens: `land_split_*`.

**C. Overlay with auto-hide panel**: A, but the UI camera is only enabled/visible while a MenuPort window is open and the panel docks to the bottom (messages) or right (menus) -> world gets the full picture most of the time (DQIV-Recompiled "windows float over the world"). Pros: best looking, widest view. Cons: per-screen anchoring/size decisions (message window as a bottom strip 960 wide would need new `canvas_data` / text-canvas widths 512 -> natively laid out text lines, i.e. re-layout of ~70 screens = the rejected option) - only attempt with the 408 panel sliding in/out. Effort 1-2 weeks, risk medium.

**D (not recommended): rotated "tate" 540x960** (06 §1) stays as the alternative for hardware without landscape need.

**Recommendation:** ship **B as the default and A as option** (same code, `layout=split|overlay`), then iterate on C (sliding panel) only for polish. Touch mapping for B/A: logical surface == display (VIEW_X0=0), `lx = sx`, `ly = 543 - sy`; the UI hit tests use the MenuCamera `pixelRect`; keep world-area taps as `PAD_ANY` only in A/C.

## 5. Prototype (branch `land`)

* `source/game/Layout.hpp` (new): `UiRect()`, `WorldRect()`, `Aspect(real)` (= 0.75 design aspect in landscape, identity in portrait), cfg `land_ui=right|left`, `land_world=full|split`, `land_naive=1`.
* Edits (all identity in the portrait build, which still compiles): `config.h` (960x544, `DQV_UI_W`), `SceneCamera.cpp` (projection keeps vertical extent, battle band `l_sceneH`, DSSA camera), `Levels.cpp` (rects), `eIndent`, `MenuMessage`, `InputName`, `MenuPortItemTable_Maps`, `FldObject`, `Capture`, `Spotlight`, `Field` (rect + width limit), `Fade` (two full-surface cameras), `vita_input` (tap slop).
* Input script `tools/land_newgame_script.txt` (the New Game flow of the other worktrees, touches shifted by +552).
* Screenshots (`re/arch/land_shots/`): `port_title_f700/f900.jpg` (portrait baseline), `land_title_overlay_f700.jpg` (960 title, castle fully visible, same pixel scale as portrait), `land_naive_title_f900.jpg` (naive: 2.35x zoom), `land_overlay_startmenu_f1100.jpg`, `land_overlay_nameentry_f1450.jpg` (UI panel works with touch), `land_overlay_throne_f1800.jpg`/`f2300`, `land_overlay_walk_f2700`, `land_overlay_bedroom_f2900` (throne room / castle interior at 960x544: widened scene, no culling holes, only the black map border at the top), `land_split_startmenu_f1100.jpg`, `land_split_throne_f1800.jpg` (split: world 552 wide, panel right; dark panel + text = DemoOverlay dev aid).
* Not verified: message window (MenuMessage) and battle menus in the panel, battle camera band, world map, Capture/Sepia/Spotlight, opening cutscene camera pans, real touch on hardware.

## 6. Estimates

| item | effort |
|---|---|
| finish A/B for field+town+menus (cfg `layout=`, remove DemoOverlay, panel backdrop, party offset) | 1-2 days |
| battle (band in world rect, black clear plane camera, DSSA monsters, name plates) | 2-3 days |
| world map (camera limit verified; optional native patch + mesh) | 0.5 day (limit) / 2 days (patch) |
| effects (Capture/Sepia/Spotlight/Tabi RT sizes, clear planes on full-surface camera) | 1-2 days |
| opening/sky stretch, endroll, slime race (DSSA stack with 256x384) | 1-2 days |
| zoom (lens) like DQIV-Recompiled, centre-the-party option | 0.5 day each |
| Total to a polished landscape mode | ~1.5 weeks |

## 7. Implementation status (merged into `native` main, 2026-10-27)

Design A is the default, B (`layout=split`) and the portrait layout (`layout=portrait`) are selectable at runtime in `ux0:data/dqv/config.txt` (`platform/cfg`), `ui_side=left` mirrors the panel. All layout code is behind `game/Layout.hpp/.cpp`; `Layout::Aspect()` replaces every `Camera.aspect` read of the ported C#. CMake `-DDQVN_LANDSCAPE=OFF` = kill switch.

* Done and verified in Vita3K (screens in `land_shots/A_*.jpg`): title, start menu, name entry, New Game, opening throne room, walking, command/status/spell menus and message path in the panel (touch maps through the panel `pixelRect`; input scripts use panel-relative coordinates so the old scripts work), fades/clear plane/Capture effects on full-surface cameras (`Fade::FullCamera`; Capture/Sepia/Spotlight effects themselves not exercised), `DemoOverlay` skipped, world slides left by half a panel while a menu is shown (`Layout::NotePanel/ShiftNdc` hooks in `MenuPortItem`/`MenuMessage`, applied in the `SceneCamera` projection (identity[0,3]), the DSSA camera position and the `Field` camera position), world-map framing limited to the 21x33 tile block (calculation only).
* Battle (plains `btl_pl_d` and burr-baby `btlca1` fights, `land_shots/battle_*.jpg`): battle monsters are 3D billboards of the MainCamera (not DSSA), so widening the projection also widens the stage; the sky dome and ground cover the wider and taller view (stage edge visible only on the castle stage at the far left at zoom 1.75). Hopping numbers/target cursors follow because they project world positions through `MainLoop::m_camera` into the UI camera. Layouts `large|panorama|panel|band` + `battle_zoom`, `battle_speed` (see README). Not done: the DQIV `large` arrangement with a full-width command menu below small status windows (needs a second battle-only UI canvas; the MenuPort screens are laid out on the 450-unit canvas), spell effect/flash overlays at full width (not exercised), `BattleMonsterMask`-style name-plate clamping (plates are MenuPort parts in the panel).
* Open: opening/title sky edges when shifted, endroll/slime race (`SlimeTouchPort` stack alignment), world map in a real run, split-mode Sepia blit, hardware touch.

## 8. World map: native block patch (done, landscape only)

`fld::FieldData::drawBlock` emits columns `-4..+20` x rows `-5..+32` around the base tile (not 21x33: 25 x 38 tiles = 400 x 608 units; the host grid of `Field.cs` is 25 x 40) and `FieldSet` lays the UVs out with a row stride of 25 quads. `source/native/field_patch.cpp` (called from `NativeCall::Bind` when the layout is landscape/split, cfg `field_patch=0` disables) rewrites four ARM immediates after the modules are loaded (checked against the expected words first, so another libdq5core build is left alone):

| vaddr | original | patched |
|---|---|---|
| 0x162e44 | `sub r5,r5,#4` first column | `#18` |
| 0x162fb8 | `cmp r2,#20` last column | `#40` |
| 0x162fd0 | `suble r5,r3,#4` column reset per row | `#18` |
| 0x0e5924 (FieldSet) | `add r5,r5,#25` row stride | `#59` |

Result: 59 columns x 38 rows = 944 x 608 units, the host mesh (`Field::Init`) is 59 x 40 quads (`NativeCall::g_fieldPatched`). 59 = 18 back + 40 forward + 1: the widest view is the dragon flight (`setOffset` zoom <= 1 + 32/128 = 1.25 -> 376 units half-width) plus the half-panel slide of the world while a menu is open (+128 units to the right). The log line `Field::SetIndexList: ... width 59 rows 38` confirms native and host agree; the old camera limit stays only for the unpatched case. Verified in Vita3K with `warp`: ocean tiles fill 960x544 at the normal 170-unit half-height with no void (chapter 4 on foot, chapter 5 dragon flight: before widening to 59 columns a 10 px black strip showed at the left in flight). Not verified on land (the warp harness could not place the player on land: `fieldpos` has no effect and the player never reached land), so tile order/stride was validated only by the width/rows log and the seamless ocean.

## 9. Other scenes in landscape (warp harness, `land_shots/warp_*`)

| scene | result |
|---|---|
| slime race (`slimerace`) | 3D track fills the world; betting UI in the panel (`warp_slimerace_f550.jpg`) |
| casino poker (`casino`) | table + paytable + message window; the table is cut at the left because the world is slid left by the open panel (acceptable) |
| staff roll (`staffroll`) | text centred on the full surface, petals; no panel involvement |
| monster book (`book`) | parchment spans the surface, list buttons in the panel |
| slime touch (`slimetouch`) | DSSA art keeps its 256x384 portrait composition: a 408 px column with black sides (not stretched); the camera is `SlimeTouchPort`'s DSSA camera at the design aspect; widening it would need new art for the sides |
| title / opening (`opening`) | sky, clouds and castle fill 960; a small cloud edge step at the bottom right during the cloud animation; the DQIV `ev01` sky stretch is not needed for the title map (`ev17`); the story opening pans (ev maps) are not reachable by the harness, untested |

## 10. UI cameras (landscape): dialogue, boot flow, battle status

`game/UiCameras.{hpp,cpp}`: besides the panel-clipped MenuCamera (layer 8, right panel) there are three more UI cameras with the same unit scale (600 units over the surface height, 0.907 px/unit): **Dialogue** (layer 21; MenuMessage window + Yes/No, flush with the bottom edge, centred; `Update` aligns the frame's nominal bottom (`MenuMessage::s_bottomZ`, reserving the Yes/No height for type 3) to the bottom of the surface; eases to the centre of the world area while a panel menu is open so they never overlap; the world never moves), **Boot** (layer 22, rect = centred 408 px panel; ports TitleTouch, StartMenu*, StartNameEntry, SaveOverwrite, ViewVolume, Network*, License, PortalButton, plus a Yes/No or the message window while one of them exists; `InputName`'s camera and free-standing parts such as the title copyright line follow), **Status** (layer 20, reserved; BattleStatusLayout no longer relocates the status row). `MenuPort::m_uiLayer` selects the camera; `MenuPortSystem` passes each port the touch ray of its own camera (`TouchPad::GetPressRayFor`), `PadFocus` projects its marker through the camera of the focused button; scripted touches are panel-relative (`ScriptPanelX0`: centred during the start flow). Layers: 14 PadFocus marker, 18 front fade, 19 BG fade/clear plane/Capture, 20-22 above.
World shift: off for dialogues and menus (`world_shift=1` brings the old behaviour back); battles use a fixed slide for the whole battle (`battle_shift`). Battle party status: scaled x1.5 (face icons x1.7) by its own camera and centred over the world area; world map: see §8.

## 11. Battle party status: faces only (open decision)

The status boxes / labels are back in the panel at their portrait size and place; `BattleStatusLayout` only enlarges the face icons (`battle_face_scale`, default 100 = off). The icon (28.8 units = 26 px) already starts at the top edge of the screen, so a bigger face grows upward and is **clipped** (`land_shots/battle_face170_2members.jpg`, 170 %); growing downward would cover the name box. Collision list, 960x544, `battle_layout=large`, plains fight, 2 members (4 members = the same boxes continued at the 101 px pitch, the 4th ends at x 954, still inside the panel; a 4-member party could not be forced: any `party` debug override crashes libdq5core before a game is loaded and chapter warps give hero + Harry only):

| element | rect x,y,w,h | collides with |
|---|---|---|
| face icon 0 / 1 | ~590,0,42,26 / ~694,0,34,26 | grows beyond the screen top when scaled |
| status box 0 / 1 (name, HP, MP, LV) | 558,28,93,100 / 659,28,93,100 | nothing (sky) |
| action labels / `Follow Orders` | ~560-760,130-170 | nothing |
| 3rd/4th box (4 members) | 760,28,93,100 / 861,28,93,100 | nothing (right end 954) |
| command panel (Fight, Tactics, Party) | 553,345,407,140 | monsters only if the slide is removed (`battle_shift=0`) |
| Flee | 823,487,137,57 | - |
| target list (while choosing) | ~555,232,295,88 | right-most monster at zoom 1.75 (x <= 500) no |
| monsters (large, 3 groups) | x 50-500, y 258-420 | status boxes: no (y < 128) |
| message window (bottom-centre, dialogue camera) | 276,400,408,125 | command panel x 553-684 if both open (the window eases left to x 72-480 while a panel menu is open) |

Options for bigger faces: (a) move the whole status row down ~20 px (free: nothing is there, monsters start at y 258); (b) let faces overlap the top of the name box; (c) keep 100 %. `BattleTest`: `party` lines are now ignored with a warning (they fault libdq5core before a game is loaded); `forceparty` restores the old behaviour.

## 12. DQIV-style battle arrangement (merged)

* **Status row** (right, panel): moved down `battle_status_y` px (45), faces x`battle_face_scale` (2.5) about their own centre keeping their bottom edge on the box; the action labels get a dark game-window plate (`MenuParts::CreateBuildVariableWindow`, sized to 1-3 lines) because in landscape they sit over the bright scene instead of black.
* **`large` auto zoom** (`SceneCamera::FitBattleZoom`, after DQIV `battleZoomToFit`): extents of the monsters' meshes are measured over frames 8..90, normalised to zoom 1 about the anchor (x = centre of the area left of the panel, y = feet line at 75 % of the height); the zoom keeps them inside x 10..540 (the status block and the command panel are on the right), y 46 (name-plate band) .. 530, clamped 0.7..2.0, eased 0.02/frame.
* **Monster name plates / number marks** (`Make_MonsterName`, `MonsterNamePlate`): placed with the full-surface Status camera (`UiCameras::PlateCamera`) from the monsters' real projected positions, clamped to the world area beside the panel (x 72..484 px for plate centres instead of +-142 units), the original overlap logic kept (+-83 / 166 units), rows below the feet and the number marks (y 468 px; the middle plates of 3+ groups 36 px lower instead of 58 units higher).
* Tall monster / boss: not shown. Monster ids other than the ones already used (1, 30, 70, 120) crashed libdq5core in the debug hook (160, 200, 240 tried); there is no monster table to validate against, so no boss screenshot.
* Collisions: none seen (plains: status y 73-215, command panel x >= 553 y >= 345, monsters x 25-535; plates y 445-520 under the monsters, left of the panel). The castle stage (burr babies) shows its black edge at the far left at zoom 2.0.

## 13. Name plates on one row, active-member arrow

* Landscape plates (`Make_MonsterName`): one row at y 468 px, each centred on its group's projected centre (`MonsterNamePlate::m_center.x`); neighbours are pushed apart by the minimum (drawn plate = 160 units incl. borders, separation 166), kept within the world area; a second row (36 px lower, alternating) is used only if one row cannot fit. The log prints `MonsterName plate N: group centre X px -> plate centre Y px` (plains fight: 87->84, 232->235, 418->418; burr babies 274->274).
* The yellow active-member arrow (`BattleUpCursor`, a child of the status root) is drawn by the Status camera (it can leave the panel) and offset left by `14.4 * (faceScale - 1) * 1.35` units so it sits beside the enlarged face; `StatusPlacer` re-applies the offset on top of the native position each frame.

## 14. Verification round (Yes/No, 4 members, boss, castle stage)

* **Yes/No window**: the Yes/No box is a singleton port opened once at start-up (before any boot port), so it kept the dialogue camera while the message window switched to the boot camera in the start flow: the box was drawn 116 px too low (off screen) under "Delete this adventure log...?". `UiCameras::Update` now moves the Yes/No port to the same camera as the message window every frame. Verified at the SaveOverwrite prompt of the start flow (`yesno_boot_portrait_f1250.jpg` = portrait reference, `yesno_boot_landscape_broken_f1250.jpg` = before, `yesno_boot_landscape_fixed_f1250.jpg` = after, centred and directly under the window, PadFocus marker on Yes). An in-game (dialogue-camera) Yes/No and a real shop menu were NOT reached: the warp harness gets into church/inn/shop maps but the NPC to talk to needs blind walking.
* **4-member party**: `party` overrides fault libdq5core unless a game exists. Start a New Game first (`tools/newgame_script.txt`), then let the warp script run late: `delay 1850`, `party 1 2 3 4`, `wait 20`, `battle btl_pl_d 30x1 70x1 120x1` (player ids 1..4 = hero (abc), Pankraz, Bianca (child), Bianca). Screenshot `party4_members_status.jpg`: the 4 boxes span x 558-945 inside the panel, faces 2.5x above their boxes, labels with plates, arrow beside the active member's face, name plates on one row; nothing overlaps.
* **Tall boss**: monster ids <= 155 work in the debug hook from the title (160, 200, 240 crash; the table apparently ends between 155 and 160). Id 155 (large winged demon, `boss_tall_id155.jpg`) fits at ~1.4x: top at y 190, feet 430, clear of the status block and the panel. No change needed.
* **Castle stage "edge"** (burr babies, `btlca1`): the black area at the left of the tower is the stage's night sky (a huge background triangle covers it: the measured left coverage was -5500 units), not a missing-geometry edge; clamping the zoom/slide would not remove it, so nothing was changed.
