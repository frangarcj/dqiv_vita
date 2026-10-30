# 01 - Frame loop, lifecycle, boot flow (Unity-cut port)

Sources: `cs_out/Assembly-CSharp/{Boot,MainLoop,MainLoopEx,NativeManager,TouchPad,RawPad(s),MenuPortSystem,Capture,Cleaner,Fade,BackColor,ParticleSystem}.cs`,
Ghidra of `libdq5core.so` (`arch/decomp/native_a.c`), Unity data dump (below). Ghidra addr = file offset + 0x10000.
Everything here is **verified** unless marked (unverified).

## 1. What actually runs per frame (NEON path, `Boot.s_useFixedUpdate == false`)

Unity runs, in this order, for ONE rendered frame (one "tick"; vSyncCount=2 on a 60 Hz panel => **30 Hz logic**, see 1.3):

```
[input poll]                                   Input.touches / Input.GetKeyDown(Escape)
Update()    (all enabled MonoBehaviours; order between scripts is unspecified in Unity, effective order below)
   TouchPad.Update  -> factUpdate()            computes TouchPad.m_press / m_toggle / m_externalToggle (pad bits)
   MainLoop.Update  -> NativeManager.Update()  the ONE call into libdq5core per frame (see 1.2)
   Fade.Update  (FadeCTRL), BackColor.Update, TapToStart.Update, HoppingNumber, Volcano, Endroll, ...
   (coroutines: yield return 0 resumes here, after Update of the frame in which they yielded)
LateUpdate()
   MenuPortSystem.LateUpdate -> internalUpdate():
        NativeCall.UnityRenderEvent(1)         dq5core TextCanvasTransfer -> glTexSubImage2D into the message canvases
        process native->unity MenuPort messages (port 0 commands, then ports 1..31: NoticeReceive + Update(pushState, ray))
   Capture.LateUpdate -> Caputer()             camera.Render() into RenderTextures + screen-effect coroutine step
[render]    cameras sorted by depth ascending; each: clear -> cull by layer mask -> sort -> draw
[end of frame]  Destroy() queue flushed; Time.frameCount++
```
Facts: `MainLoop.Update` only calls `NativeManager.Update()` when `(Global.Flag & 0x30000) == 0` (not paused / not cleaning).
`MenuPortSystem.internalUpdate` ALWAYS calls `UnityRenderEvent(1)` first (even when paused), then returns early if `(Global.Flag & 0x30000) != 0`
(it still tracks `s_pushState` = 4 if finger is down, so a press that began during a pause is ignored until released).

The `FixedUpdate` variants (MainLoopEx, FadeEx, MenuPortSystemEx, HoppingNumberEx, ...) are used only when `/proc/cpuinfo` has no "neon" => **not ported** (always NEON; libdq5core.so, never libdq5core_n.so).

### 1.1 Boot / scene flow (what must be reproduced)

```
Unity start -> scene "mainData" (level 0 = Boot scene): Main Camera (ortho 400, clear=Color black? see 6) + TextMesh "New Text" (empty, unused)
   Boot.Start (coroutine):
      GetLangages.Init()                          language -> 1 (ENGLISH) (see REPORT_JNI_DATA §4); Screen.fullScreen=true
      (dataPath ends with ".obb" => skip ObbDownloadRes entirely: NOT ported)
      ApplicationQuit.SystemInit(quitmsg)         quit dialog texts (Resources "boot/quitmsg")
      Logo(): for each of m_logoTexture[2] = {boot/sqex, boot/arte_logo720_240} (Boot MonoBehaviour fields, mainData obj 31):
              Quad 450x150 at (0,0,150) under camera, material Custom/Picture-like (m_logoShader), alpha 0->1 over 1 s,
              hold 3 s (touch = skip), 1 s fade out. Touch() = rising edge of Input.touchCount>0.
              Between logo steps `while (Quiting()) yield` handles Escape -> quit dialog (see 7).
      s_useFixedUpdate = IsNotNeonSupport()  => false; skip SetNoNeonMode / targetFrameRate=30
      wait NativeCall.IsLoadNativeCode()!=0       (set by dq5core JNI_OnLoad; in the port: already true after JNI_OnLoad)
      JNI loadFilelistFromAssets()                => in the port: call dq5core LoadFileList once (loader owns this; see REPORT_JNI_DATA §3)
      deviceInfo |= 1  (PowerVR/SGX/Vivante)      => PASS 1 (Vita = SGX543). (Adreno 320 would be |=16: never)
      Application.LoadLevel("dq5sp")              => scene "level0" (all non-DontDestroyOnLoad objects destroyed)
level0 (10 GameObjects, dumped in 6): MainCamera [+ MainLoop], MenuCamera [+ TouchPad], MPad hierarchy (virtual-pad meshes), CurLight/DirLight... (no Light components!)
   MainLoop.Start (coroutine, ONE yield):
      m_camera = camera; DontDestroyOnLoad(go)
      NativeCall.DeviceInit()            -> DevInit(assetsPath, savePath, lang, deviceInfo)   [assetsPath = OBB path; savePath = persistentDataPath + "/"]
      BackColor.SystemInit(); Fade.SystemInit(); Capture.SystemInit(); BillboardObject.SystemInit();
      Chara.SystemInit(); Riseup.SystemInit(); MeshChara.SystemInit(); ParticleSystem.SystemInit() (-> native ParticleSystemInit);
      SceneCamera.SystemInit() (creates DSSA ortho camera); TouchPad.AdjustCameraOrtho();
      MenuParts.SystemInit(); MenuPortSystem.SystemInit() (-> native MenuPortInit); Sound.SystemInit();
      NativeManager.Init() (-> native GameInit);
      Global.InitCamera(0); SceneCamera.SetAngleDeg/SetTarget/Update();
      yield return 0;
      Global.Flag &= ~0x10000   (FLAG_PAUSE cleared: from now on NativeManager.Update runs each frame)
   TouchPad.Awake (DontDestroyOnLoad), Start waits for FLAG_PAUSE cleared, then builds the on-screen buttons.
```
Initial `Global.Flag = 65536` (paused). Native `GameInit` ends by calling `Global::startTitle`-path setup (`Global::SystemInit/initialize`, `MenuAPI::SystemInit`, DSSAObject::Init...). The first `GameUpdate` runs the title (`GameUpdate`: `if (flag at +0x20 != 0 && (frame&3)==0) -> Global::startTitle/startTown`); the title screen itself is drawn by native commands + MenuPort items (TapToStart is a C# MenuParts spawned by a menu item).

### 1.2 `NativeManager.Update()` (the per-frame exchange) - exact algorithm

```c
// C# globals: int m_input[1024]; int m_output[8192]; float m_output_float[8192]; IntPtr m_string[32];
if (m_camera_flag > 0) {              // set to 1 by cmd_camera (op 11) last frame
    m_camera_flag = 0;
    SetInt(0); SetInt((int)(SceneCamera.s_camera_pos.x *  4096f));
               SetInt((int)(SceneCamera.s_camera_pos.y *  4096f));
               SetInt((int)(SceneCamera.s_camera_pos.z * -4096f));   // input op 0 = actual camera world pos (fx32, z negated)
}
if (m_fld_load) for each id in m_anim_list: if animation finished -> SetInt(1); SetInt(id);   // input op 1 = anim-end notice
                                                                                                //  id<0x1000: FLD common anim obj id; else fbx model (id&0xFF) | 0x1000
SetInt(-1);                            // terminator
m_output[0] = Global.Flag;             // C# -> native flag word (bit set below)
NativeCall.GameUpdate(TouchPad.m_press, TouchPad.m_toggle);   // native writes m_output/m_output_float/m_string
m_output_offset = m_float_offset = m_input_offset = 0;
Global.Flag = GetInt();                // native -> C# flag word (m_output[0])
for i in 0..23: if (m_dssa[i]) m_dssa[i].SetVisible(false);   // all DSSA meshes hidden; cmd_mesh re-enables the ones redrawn this frame
loop: op = GetInt(); if (op < 0) break; m_cmd_tbl[op]();        // 41 commands, see 02_PACKET_PROTOCOL.md
if (m_camera_flag > 0) { SceneCamera.Update(); Chara.UpdateBillboardShadows(); for all chara/rise: SetBillboard(); }
else                     for all chara/rise with m_billboard_request: SetBillboard();
for i in 0..23: if (m_dssa[i]) m_dssa[i].SetBillboard();
if (m_fld_load) { m_fld.Update(); /* + special-case m_fld_etc==0: attach m_chara[0] to map object 38 */ }
if (Sound.s_bgmState != 0) Sound.SystemUpdate();
```
Native side of GameUpdate (decompiled): reads `UnityGlobalFlag = out[0]`, sets output cursor = out+1, float cursor = start, resets;
processes `m_input` ops until a negative int (op0 -> camera pos Fix32 x3 stored in `g_camera_pos`; op1 -> `FLDObject::SetCommonAnimationEnd(id)` or `ModelObject::setMotionEnd(id&0xFF)`);
then `UnityMenuLoop()` (dispatch MenuPort messages from C#, `MenuPort::Defrag`, `ardq::MenuManager::execute`), stores pad (`toggle==0x100 -> 0x200`), runs `GamePartManager::Exec()` (the whole DS game frame),
`status::GameStatus::addPlayTime(2)`, `++g_FrameCount`, finally writes `out[0]=UnityGlobalFlag` and the **terminator -1** at the output cursor.
=> **One GameUpdate = one logic tick.** Native expects ~30 ticks/s (play time += 2 per tick in 1/60 s units).

### 1.3 Frame rate / time
* mainData QualitySettings: single level "Fastest", `vSyncCount = 2` => 30 fps on 60 Hz displays. TimeManager fixed timestep 10 (unused, FixedUpdate not used). `targetFrameRate=30` only on the no-NEON path.
* Port: `vglSwapInterval`-style swap interval 2 (or fixed 33.3 ms pacing); **render and logic at 30 Hz, 1 GameUpdate per rendered frame** (REPORT_RUNTIME lists this as unverified; the playtime +2/tick evidence supports 30 Hz).
* `Time.deltaTime` users: Boot.Logo fades (1 s/3 s/1 s), TouchPad pad-blink; everything else counts frames (`Fade` m_count, `TapToStart` uses `Time.frameCount`, Capture counts frames). Implement `Time.deltaTime` = measured frame time clamped to 1/10 s; `Time.frameCount` increments per rendered frame. `Time.timeScale` (quit dialog sets 0): only the Boot/ApplicationQuit paths touch it - model it as a global pause flag (`Global.Flag |= 0x10000` is what actually freezes the game logic).
* `OnApplicationPause(pause)`: Sound.SystemPause; on resume: `NativeManager.ResetNativePacket(); MenuPortSystem.ResetNativePacket(); ParticleSystem.ResetNativePacket()` (re-register pinned arrays after GC). In the port arrays are static and never move: **skip** (but still call Sound.SystemPause on PS button suspend).

### 1.4 `Global.Flag` word (shared C#/native, round-tripped every frame in `m_output[0]`)
| bit | name | meaning |
|---|---|---|
| 0x1 | FLAG_FADE | a Fade plane is animating (set/cleared by C# `Fade`; also set by native `UnitySetFade`) |
| 0x2 | FLAG_SCREEN_EFFECT | Capture screen-effect coroutine running (C# sets in `Capture.ScreenEffect`, clears when coroutine ends) |
| 0x4 | FLAG_PLAYER_LOCK | TouchPad ignores input (`IsEnable` mask 4|0x40|0x80) |
| 0x8 | FLAG_TOUCH_DIR | native wants touch-direction input (virtual pad disabled; `UnityInitTouchFlag(1)`) |
| 0x10 | FLAG_TOUCH_ROT | camera-rotate bar usable |
| 0x20 | FLAG_TOUCH_MAP | map button usable (`UnityInitTouchFlag(2)` = 0x28) |
| 0x40 | FLAG_GUI_LOCK | cleared by MenuMessage shake end |
| 0x80 | FLAG_SYSTEM_GUI_LOCK | |
| 0x10000 | FLAG_PAUSE | initial value 65536; C# clears after MainLoop.Start; quit dialog sets it |
| 0x20000 | FLAG_CLEANER | `Cleaner.Cleaning()` (after every field/stage load): blocks NativeManager.Update + MenuPort processing for ~5 frames while assets unload |

Port of `Cleaner`: keep the frame counting (3 Update ticks, then 2 more yields) so native sees the same latency, but `Resources.UnloadUnusedAssets` => our texture/mesh refcount sweep, `GC.Collect` => no-op, `ResetNativePacket` x3 => no-op.

## 2. Pad / touch -> GameUpdate (summary; details + Vita mapping in 06_INPUT.md)

`GameUpdate(int pad_press, int pad_toggle)` (native takes the low 16 bits: `short press, ushort toggle`; stored in `ar::g_Pad`, held u16 +0, toggle u16 +2):
`PAD_U=1 D=2 L=4 R=8` (field movement and menu cursor), `0x10` camera-rotate left (BL), `0x20` camera rotate right (BR), `0x40/0x80` FL/FR (flick rotate: toggle `0xB0`/`0x70` = flick left/right: 0x80|0x30 and 0x40|0x30),
`0x100` ANY (tap anywhere; **native maps toggle==0x100 to 0x200**), `0x200` A/confirm, `0x400` B/cancel/talk, `0x800` X (command menu), `0x1000` Y (area map), `0x2000` START, `0x4000` SELECT, `0x8000` DEBUG (unused).
`m_toggle` = rising-edge bits `(prev ^ now) & now`, OR `m_externalToggle` (set by MenuPortItem when a touch button with `m_gamepad` code is released: `TouchPad.m_externalToggle |= button.m_gamepad`).
Virtual-pad and the 4 on-screen buttons (Map = 0x1000, Command = 0x800, Talk = 0x400, PadPos = port message) are pure C# UI in `TouchPad` (MenuParts + colliders).

## 3. What the port keeps / drops from the frame loop

| item | decision |
|---|---|
| Update/LateUpdate/render split | keep exactly (needed: Fade/pad in Update before GameUpdate? Actually TouchPad.Update vs MainLoop.Update order is *unspecified* in Unity; we choose TouchPad first so the pad of this frame reaches GameUpdate) |
| FixedUpdate variants, `Boot.IsNotNeonSupport`, NoNeon | drop |
| ObbDownloadRes / ExpansionDownloader / GoogleDrive / UnityCloud / UnitySQMK / AndroidSQMK | drop; `NetConnection` port (catego 4) is replaced by a stub port (see 05 §NetConnection) |
| `Application.LoadLevel` | no scene system: `Engine::LoadLevel("dq5sp")` = destroy all objects not marked DontDestroyOnLoad, then run the level0 builder in C++ (`Level0::Build()` below) |
| Quit dialog | `ApplicationQuit` -> native Vita dialog (sceMsgDialog or own MenuParts popup); `GetQuitDialogResult` 1=OK, 2=cancel, 0=pending |
| Coroutines | C++20 coroutines (`gnu++20` + GCC 15 is already used by port/CMakeLists; verified `-fcoroutines` compiles with arm-vita-eabi-g++). `yield return 0` => `co_yield 0` (resume next frame, after Update); `yield return new WaitForSeconds(s)` => `co_yield Wait(s)` |

## 4. Native entry-point call sequence from the port (replaces Mono P/Invoke)

All through `so_symbol()` pointers into libdq5core.so (all int/pointer args => no softfp issue). Called on the **game thread** (same thread that owns the GL context, because `UnityRenderEvent` issues GL calls):
```
DevInit(const char* obbPath, const char* saveDir /*ends with '/'*/, int lang=1, int devInfo=1) -> bool
GameInit(int* in /*1024*/, int* out /*8192*/, float* outF /*8192*/, const char** str /*32*/)      // pointers retained by native: static storage
ParticleSystemInit(Vec3* posScale /*2*/, int* idx /*1176*/, Vec3* vtx /*784*/, Vec2* uv /*784*/, Color32* col /*784*/)   // retained
MenuPortInit(short nPorts=32, short* unityBuf /*128*/, short unitySize=128, short* nativeBuf /*2048*/, short nativeSize=2048)  // retained
per frame:  GameUpdate(press, toggle);  UnityRenderEvent(1);  [FieldSet(...) on demand]
FileReadAllBytes(const char* relPath) -> malloc'd buffer (sound/*.akb, sound/snd_vol.dat); FreeFileMemory(ptr)
```
`UnitySetGraphicsDevice` is a no-op (0xe8464); `UnityRenderEvent(id!=1)` is a no-op. `IsLoadNativeCode`, `Debug*`, `DebugCollSetMesh`, `GetMessageCanvas` are not used by the shipping C# (GetMessageCanvas is not even imported by Neon.cs; it memcpy's the TextCanvas buffer).
**There are no `coll_*` calls from C#.** The whole collision/pathing layer (`coll_init`, `coll_SearchFloorPoly`, ...) is native-internal; the only debug hook is `DebugCollSetMesh` (DebugDispColl.cs, drop).

## 5. Memory/threads
Single game thread (big stack, >= 1 MiB). libsdlib has its own update thread + audio callback (REPORT_RUNTIME §D). GL only on the game thread. dq5core's `glBindTexture/glTexSubImage2D` hit our vitaGL through the loader (dynlib) - GL state discipline: **TextCanvasTransfer leaves GL_TEXTURE_2D bound to the canvas texture and changes the error state; the engine must re-bind its own textures before every draw** (do not cache "currently bound texture" across `UnityRenderEvent`), and call it when the active texture unit is 0.

## 6. Level-0 scene, exact data (UnityPy dump `/tmp`-free; reproduced from `extract/unity/{level0,mainData}`)

GameObjects (all layer 8 except MainCamera): `CurLight`, `cdDebug`, `Dirs`, `DirNormal`, **`MainCamera`** (layer 0, tag MainCamera), **`MenuCamera`** (layer 8), `CurNomal`, `MPad`, `DirLight`, `CurBS`.
* **MainCamera**: pos (0, 28.925, -26.973) rot quat (0.39875,0,0,0.91706) [= euler X 47°]; perspective, fov 20 (overridden at runtime by `SceneCamera.Update`), near 5, far 100 (overridden), `m_ClearFlags = 3` (**Depth only**, Unity 4: 1 Skybox, 2 SolidColor, 3 Depth, 4 Nothing), bg (0.192,0.302,0.475,0.0196), rect (0,0,1,1), `m_Depth = -1`, `cullingMask = 1` (layer 0 only), no target texture, HDR off, GUILayer present. Script: **MainLoop** (no serialized fields).
* **MenuCamera**: pos (0,100,0) rot X +90° (looks down -Y; UI lives in the XZ plane: x right, **z up** on screen), **orthographic**, size 400 (runtime: `TouchPad.AdjustCameraOrtho` sets `225 / aspect`), near 1, far 200, `clearFlags = 3` (Depth), `depth = 1`, `cullingMask = 256` (layer 8 only). Script **TouchPad** with fields: m_siColDebug=MeshCollider(cdDebug), m_siColMove=SphereCollider(MPad r=95), m_siMovePad=Transform(CurNomal), m_siMoveCurNormal/ m_siMoveCurLight=MeshRenderers, m_siMoveCurBS=Transform(CurBS), m_siMoveDirs=Transform(Dirs), m_siMoveDirLight/DirNormal=MeshRenderers.
* MPad subtree (virtual joystick; only needed if the touch pad is kept): MPad pos (-120,-265.5,100) scale .63; children Dirs(z -1)->DirNormal/DirLight; CurNomal -> CurLight(z=1), CurBS(z=2). Meshes in sharedassets1 (file 1) / OBB (cdDebug mesh, MeshCollider).
* RenderSettings: ambient (0.2,0.2,0.2), fog off. No Light components anywhere (=> no lighting; Alpha-Diffuse materials get ambient only; `ImdShadeChara` uses its own `_LDir`/`_LColor`).
* mainData: `Main Camera` (ortho 400, clearFlags 2 SolidColor black, mask all, depth -1) + `New Text` TextMesh (empty) + Boot script (fields: m_logoTexture[2] = boot/sqex, boot/arte_logo720_240; m_logoShader; m_obbMenuParts, m_obbFont[], m_obbMat[], m_obbmsg, m_quitmsg).
* Other cameras are created at runtime: `SceneCamera.s_DSSAOrthoCamera` (ortho, size 128.53125/aspect, pos (0,0,100), near -1024, far 1024, depth 0.5, clear Depth, mask 2048 [layer 11 "DSSA_OTHOR"]), `Field.m_camera` ("FieldCamera", ortho size 128/aspect, rot X 90, near 1, far 500, depth 0, clear Depth, mask 512 [layer 9], created in `Field.Init`, and **MainCamera is disabled while Field is active**), `InputName` camera (ortho 225/aspect, depth 2, mask 1024 [layer 10], pixelRect-clipped), Endroll/SlimeTouch re-configure existing cameras (push/pop depth, clearFlags, mask, bgColor).
* Layers (TagManager): 0 Default, 8 Menu, 9 Field, 10 INPUT_NAME, 11 DSSA_OTHOR.
* Scenes' MonoBehaviours: only MainLoop, TouchPad, Boot (3 in total).
