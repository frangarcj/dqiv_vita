# 03 - "mini-Unity": API subset, Unity 4.7 semantics, rendering model

Header sketch (the contract for the engine package): `arch/include/miniunity.hpp`. Census = grep of the C# (excluding GoogleDrive/SQMK/Net/OBB/Test/Debug classes, 22.9k lines total, ~19.5k relevant);
numbers = occurrences. Everything below is what the port needs - **nothing else must be implemented**.

## 1. API census (verified by grep) and what to build

| Unity API | uses | needed members (exact) |
|---|---|---|
| `GameObject` | 114 | `new GameObject()`, `new GameObject(name)`, `AddComponent<T>`, `GetComponent<T>`, `GetComponentsInChildren<Renderer|MeshFilter>()`, `.transform`, `.layer` (28), `.name`, `.active` (get/set, 14), `SetActive` (5), `SetActiveRecursively` (50), `Find`? no, `CreatePrimitive(Quad)` (Boot logo only) |
| `Transform` | 74 (+85 `.parent`) | `parent` (set 85x), `position`, `rotation`, `localPosition` (55), `localScale` (50), `localRotation`, `eulerAngles`/`localEulerAngles` (10), `Rotate(x,y,z)` (6), `Translate` (4), `LookAt(target, up)` (3), `GetChild(i)`, `childCount`/`GetChildCount`, enumeration `foreach (Transform t in transform)` |
| `Component`/`MonoBehaviour` | 84 `AddComponent` | `Awake Start Update LateUpdate OnDestroy OnRenderImage OnApplicationPause OnApplicationQuit`; `enabled`; `StartCoroutine` (1) + `IEnumerator Start()` (6 classes). NOT used: FixedUpdate (NEON path), OnGUI (debug skin only), SendMessage, Invoke |
| `Material` | 160 | `new Material(Shader)`, `new Material(Material)`, `.color` (get/set), `.mainTexture` (50), `.shader`, `.renderQueue` (6), `SetTextureOffset("_MainTex",v)`, `SetTextureScale("_MainTex",v)`, `SetColor("_Color"/"_OrgColor"...)` (14), `Instantiate(material)`; `renderer.material` (clone-on-access), `.materials`, `.sharedMaterial` |
| `Shader` | 63 | `Shader.Find(name)` (names list in 04), `.name` (FldObject.cs:2591 re-finds by `material.shader.name`) |
| `Mesh` | 60 | `vertices`(45 set), `uv`(41), `triangles`(16), `SetTriangles(arr,sub)`(20), `subMeshCount`, `colors`/`colors32`(11), `Clear()`, `bounds`/`RecalculateBounds`, `MarkDynamic/Optimize` (no-ops), `mesh` getter on MeshFilter is **instancing** (copy on first access), `sharedMesh` |
| `MeshFilter` / `MeshRenderer` | 78 / 39 | `mesh`, `sharedMesh`, `enabled` (95 `parent/enabled`), `material(s)`, `sharedMaterial` |
| `Texture2D` / `Texture` | 39 | `width`,`height`,`format`,`filterMode`(Point/Bilinear),`wrapMode`(Clamp),`name` (`mainTexture.name.IndexOf("tz_")` FldObject:1431 !), `GetNativeTextureID()`, `new Texture2D(w,h,fmt,false)`, `SetPixels32`, `Apply` |
| `TextMesh` | 43 | `text`, `font`, `characterSize`, `lineSpacing`, `anchor`, `alignment`, `renderer.sharedMaterial/material`, `renderer.enabled`; `fontSize`/`richText` default (see §5) |
| `Camera` | 33 | `aspect`, `orthographic`, `orthographicSize` (11), `fieldOfView`, `near/farClipPlane`, `depth`, `cullingMask`, `clearFlags` (21), `backgroundColor`, `rect`, `pixelRect`, `projectionMatrix` (get **and set**), `targetTexture`, `Render()`, `enabled`, `ScreenPointToRay`, `WorldToScreenPoint`, `WorldToViewportPoint`, `ScreenToWorldPoint`, `Camera.main` / `Camera.mainCamera` |
| `RenderTexture` | 8 | `new RenderTexture(w,h,16)`, `filterMode`; assigned to `Material.mainTexture` and `Camera.targetTexture` |
| `Graphics.Blit` | 1 | Sepia.OnRenderImage |
| `Resources` | 49 | `Load(path)`, `Load(path,typeof(T))`, `UnloadAsset`, `UnloadUnusedAssets` |
| `Object` | 59 `Destroy` | `Destroy`, `DestroyImmediate` (12), `DontDestroyOnLoad` (33), `Instantiate` (14: GameObject/Transform/Material/Mesh) |
| `Time` | 16 | `deltaTime`, `timeScale`, `frameCount`; set `maximumDeltaTime/fixedDeltaTime` (no-NEON only: drop) |
| `Screen` | 16 | `width`, `height`, `fullScreen=true` (ignore) |
| `Application` | 12 | `Quit`, `dataPath` (string must end ".obb"), `streamingAssetsPath` (C# does Substring(11, len-11-8): fake `jar:file://<obb>!/assets`), `persistentDataPath`, `LoadLevel("dq5sp")`, `OpenURL` (no-op), `targetFrameRate` (drop), `internetReachability` (NotReachable) |
| `Input` | 8 | `touches[0]`, `touchCount`, `GetKeyDown(Escape)` |
| `Animation` | 2 files | `animation.Play(name)`, `CrossFade(name,t)`, `PlayQueued(name)`, `isPlaying`, `clip.wrapMode`, `animation[name].wrapMode`; `foreach` clip names "Take 001".. ; CgController keeps `Animation[]`+`AnimationState[]` (time, normalizedTime?, speed - read CgController.cs when porting) |
| colliders | 3 `.collider.Raycast` | `BoxCollider(center,size)`, `SphereCollider(radius)`, `MeshCollider(sharedMesh)`, `Raycast(Ray, out RaycastHit hit, float.MaxValue)`, `hit.point`, `collider.enabled`; **no physics engine, no rigidbodies, no triggers events** |
| Math | | `Vector2/3`, `Quaternion` (identity, Euler, AngleAxis, Inverse, `q*v`, `q*q`), `Matrix4x4` (TRS, Perspective, identity, indexer), `Color/Color32`, `Rect`, `Bounds`, `Ray`, `Mathf.{Abs,Sqrt,Sin,Cos,Atan2}`, `Color.Lerp`, `Vector3.{Lerp,Scale,ClampMagnitude}` |
| `AndroidJavaClass` | ApplicationQuit, Boot | replace by direct C++ (dialog, filelist) |
| NOT used anywhere | | uGUI, Physics, Rigidbody, Particle components, AudioSource/Listener, Light, Skybox/fog, Shadows, LOD, NavMesh, Network, PlayerPrefs, WWW, JSON, Reflection, `System.Threading`; `Resources.LoadAsync` none; `SendMessage` none |

C# BCL used and to be replaced: `List<T>`, `Dictionary`, `ArrayList` (FldObject), `Hashtable` (EndrollPort), `BitConverter`, `BinaryReader/Writer` on `MemoryStream` (MeshList, ChunkFile, FldObject.Load, Endroll), `System.Random`, `File.ReadAllBytes/Exists` (saves in NetConnection only), `Marshal.Copy`, `string.Split/Substring/IndexOf/Replace`, `ToString("D3")`.

## 2. Unity 4.7 semantics that the game relies on (must match)

### 2.1 Objects and lifetime
* **Destroy is deferred** to the end of the current frame (after LateUpdate and render of that frame is NOT guaranteed: it runs at the end of the frame's update loop - Unity destroys before rendering the next frame; use: flush at `EndFrame`). A destroyed object is still rendered/alive for the rest of the frame in Unity (e.g. `Destroy(go); ...; go.renderer.enabled=false` patterns). `DestroyImmediate` is immediate (Sepia, Spotlight use it).
* `Destroy(MeshFilter)` / `Destroy(MeshRenderer)` before `Destroy(gameObject)` is common in the C#: destroying components individually must work, `renderer` becomes null afterwards.
* **`DontDestroyOnLoad(obj)`**: for GameObject keeps the whole hierarchy; for a `Material/Mesh/Texture/Shader` asset only prevents unload (ignore). 33 uses.
* `Application.LoadLevel` destroys every root object not marked (and their hierarchy) at the *next frame boundary*, then loads the scene (Awake/Start queued). Our `Engine::LoadLevel` may run synchronously inside `Boot`'s coroutine step; ensure Boot's own objects are destroyed only after its coroutine yields.
* **Awake/Start timing**: `AddComponent<T>()` runs `Awake` immediately; `Start` runs before the first `Update` of that component (same frame if added during Update, because Unity processes the new component's Start before its Update **of the next player loop iteration if added after Update ran**: simplified rule we adopt: Start runs at the beginning of the next `RunUpdate` pass, before any Update). IEnumerator-Start classes (Boot, MainLoop, TouchPad, Capture): first `MoveNext` happens at that same time; each later `MoveNext` after `yield return 0` happens next frame **after** all Update calls (coroutines resume after Update in Unity 4).
* `GameObject.active` get = `activeInHierarchy`, set = `SetActive`. `SetActiveRecursively(b)` (50 uses, mostly menu parts) sets `activeSelf` of the object and **all descendants** (Unity 4 compat behaviour). A deactivated GameObject receives no Update/render; its Renderers are skipped; its coroutine/Update stops (MenuParts' MonoBehaviour behaviours rely on this).
* `GetComponentsInChildren<T>()` **skips inactive GameObjects** (default `includeInactive=false`). FldObject.init_fld calls it on instantiated map objects after `disable_game_object` - the order of calls in the C# is the spec; replicate.
* `renderer.material` getter returns a per-renderer **instance** (clones `sharedMaterial` once, leaves original untouched; the clone is destroyed with the renderer); `renderer.sharedMaterial` does not clone. `meshFilter.mesh` getter likewise clones `sharedMesh` once. `Instantiate(GameObject)` deep-clones the hierarchy but **shares** meshes/materials until accessed through `.mesh/.material` (this matters: FldObject mutates materials of instantiated models freely - give each renderer its own material instance *lazily*; do not deep copy textures).
* `Material.mainTexture = tex` and `.color` are shorthands for `_MainTex` and `_Color`; `.shader =` swaps shader keeping properties (the FldObject trans/transz/atest swap and `material.shader.name` re-find at line 2591: sepia variants are found by name `"Custom/fld_trans_sepia"` etc.).
* `renderQueue` getter returns the shader's queue when `-1`. Setting `renderQueue` on a material instance affects only it.

### 2.2 Transform
* Euler order: `Quaternion.Euler(x,y,z)` = `qY * qX * qZ` (Z applied first, then X, then Y) - "ZXY". `eulerAngles` getter returns 0..360 degrees. `transform.eulerAngles = v` sets world rotation. `Rotate(x,y,z)` (default Space.Self) = `localRotation *= Euler(x,y,z)`. `Translate(x,y,z)` default Space.Self = `localPosition += localRotation * (x,y,z)` (ignoring parent scale, like Unity).
* **Left-handed** world, +Y up, camera looks down +Z local. Positive angles rotate clockwise when looking along the axis (left-hand rule). Matrices: `TRS = T*R*S`, world = parent * local.
* `transform.parent = p`: Unity 4 setter calls `SetParent(p, worldPositionStays = true)`: world position/rotation/scale preserved (local values recomputed). Many call sites set `localPosition` AFTER `parent =` (so either way correct), but `MenuMessage`/`Fade`/`BackColor` set `parent` first and then local TRS. Implement the world-preserving behaviour (it is a no-op for freshly created objects at origin).
* `LookAt(target, up)`: rotation = `LookRotation(target - position, up)` (SceneCamera uses it each frame with `s_camUp`).
* `foreach (Transform t in transform)` enumerates direct children in order.

### 2.3 Camera (this is where the game's whole look is defined)
* Render loop: all enabled cameras, **ascending `depth`** (ties: creation order). Per camera:
  1. viewport = `rect` (normalized, origin bottom-left) of the render target; clip pixels outside.
  2. clear per `clearFlags`: `Skybox`(1)/`Color`(2) -> color (backgroundColor) + depth; `Depth`(3) -> depth only (color from previous cameras stays!); `Nothing`(4) -> none. (Unity 4 clears only the camera's rect.)
  3. cull: renderer visible iff `gameObject.activeInHierarchy && renderer.enabled && (cullingMask >> layer) & 1` (+ frustum, optional).
  4. sort & draw (2.4).
* `Camera.aspect` (default): `viewport pixel width / pixel height` of the camera rect on its target (UNVERIFIED whether rect-aware in 4.7.2: `SceneCamera.SetViewAndCenterY` reads `l_camera.aspect` before changing `rect`; `InputName` reads `camera.rect.height`/`pixelRect`. Implement rect-aware, then test town->battle->town transitions; every `InitParam.Apply()` ends with a rect assignment so a stale rect only matters for back-to-back battle camera applies).
* **Ortho** camera: `orthographicSize` = half-height in world units, half-width = size*aspect; near/far along local +Z. `SceneCamera.s_DSSAOrthoCamera.orthographicSize = 128.53125 / aspect`, MenuCamera `225 / aspect` => **fixed world width: 450 units (UI) / 257 units (DSSA) / 256 units (Field)**, height varies with the surface aspect.
* `projectionMatrix` set by `SceneCamera.Update` (only for MainCamera): `P = Translate_y(yOffset NDC) * swap(Perspective(fov*4/3, 1/aspect, near, far))` where `swap` exchanges `[0,0]` and `[1,1]` => **horizontal FOV is fixed (= s_fov*4/3 deg) and the vertical follows aspect** (DS 4:3 behaviour). Code (verbatim):
  ```
  num = s_fov * 1.3333334; a = 1/l_camera.aspect;
  m = Perspective(num, a, near, far);  swap(m[0,0], m[1,1]);  identity[1,3] = s_yOffset;  camera.projectionMatrix = identity * m;
  ```
  Unity convention for `Matrix4x4.Perspective`: right-handed GL-style clip (z in [-1,1]), camera looks along **+Z in view space after Unity's z flip**; to render with GL: `V = Scale(1,1,-1) * worldToCamera`, `P = projectionMatrix` as given. Keep `glFrontFace(GL_CW)` + cull back (Unity's front-face definition) - most game shaders are `Cull Off`, the `fld_*` ones are default `Cull Back`.
* Custom projection stays until `Camera.ResetProjectionMatrix`/fov change; here `SceneCamera` re-sets whenever `l_bCalcProjection`.
* `ScreenPointToRay(p)`: p in pixels of the *logical screen* (bottom-left origin) -> for ortho: origin = unproject at the near plane offset so that `origin.z == p.z` is **replaced**: RawPads sets `position.z = cam.transform.position.z` first; the C# then uses only `ray.origin.x/z` (world x and z of the UI plane, y is depth) and `ray.direction`. For the MenuCamera (looking down -Y at y=100) `origin` = (worldX, 100 - near..., worldZ). Collider tests are made against boxes lying in the XZ plane. => implement the generic Unity function.
* `Camera.main` = enabled camera tagged `MainCamera` (tag id 5 in the pack's DqvNode.tag). `Camera.mainCamera` same.
* Layers: culling masks used: 1 (Default), 256 (Menu), 512 (Field), 1024 (INPUT_NAME), 2048 (DSSA_OTHOR), 0xFFFFFFFF (mainData camera).
* `Camera.Render()` (Capture): renders **immediately** into `targetTexture` using the camera's current settings (including clear flags/rect) and then returns. Nested inside LateUpdate => our renderer must support re-entrancy outside the main render pass (flush pending state).
* `OnRenderImage(src,dst)` (Sepia, on Camera.main's GameObject): when the camera has such a component enabled, the camera renders into an intermediate RT (screen sized) and the component blits it to the real target with a material (`Graphics.Blit(src,dst,mat)` = draw a full-screen quad, `_MainTex = src`).

### 2.4 Renderer submission order and state
* Unity 4 order inside a camera: by **render queue** ascending, then opaque queues (< 2500 incl. Geometry 2000, AlphaTest 2450) sorted roughly front-to-back (we use view-space depth ascending for stable early-Z), **transparent queues (>= 2500) sorted back-to-front by distance of the renderer bounds center to the camera** within equal queue, ties by creation order. Queue names: Background 1000, Geometry 2000, AlphaTest 2450, Transparent 3000, Overlay 4000; `Transparent+N`, `Overlay-N` offsets (see 04).
* Many game objects rely on `renderQueue` **explicitly**: `Chara` = 3000 + `s_priority` (set via fld_etc op 8 / native), shadows `priority-1`; FLD map objects: `renderQueue = priority` or `3000+priority` (FldObject 1245-1265) - this is how the DS draw order (OT priority) is reproduced. Per-object depth testing: `Chara` writes depth (default ZWrite) with alpha test > 0; "transz" variants (`fld_transz`) write depth + discard alpha==0.
* Material state comes from the Shader (04): blend, ZWrite, ZTest, Cull, AlphaTest. `ZTest Always`/`False` shaders are overlays (fades, captures, DSSA transparent, riseup icons).
* Multi-material: `MeshRenderer.materials[i]` draws `submesh i` (i >= count: Unity draws the last material for extra submeshes? No: extra submeshes are not rendered). Used by MeshMaker (up to 3 materials) and Capture (2) and Spotlight (2).
* Per-draw uniforms: `_Color`, `_MainTex_ST` (scale/offset via `SetTextureScale/Offset`; default (1,1,0,0)), `glstate_matrix_mvp`, `_Object2World` (ImdShadeChara only), `_Brightness`, `_LDir/_LColor/_Ambient/_Emission`. Vertex attributes: position, texcoord0, color (RGBA8; **default white if the mesh has none**), normal (ImdShade only).
* Dynamic meshes: `mesh.vertices = arr; mesh.uv = arr; mesh.triangles = arr` replaces content and invalidates GPU copy; submit with client arrays or per-frame dynamic VBO (vitaGL `glDrawElements` with client pointers copies into its pool). Meshes updated every frame: DSSA meshes (<= 24 slots, each <= 1000 verts), particles (784 verts), Field grid UVs (4000 verts, only UV changes), HoppingNumber, Volcano, BackColor/Fade colors, MenuMessage canvas quad. Static: everything from the pack. Index type u16 (pack) - ensure `triangles` > 65535 vertex meshes never occur (converter guarantees u16).

### 2.5 Texture semantics
* Origin bottom-left, row 0 = bottom (pack keeps this): upload as-is to GL; UV (0,0) = bottom-left. Several C# paths use **negative V and rely on `wrapMode = Repeat`**: `PartsInfo.SetUV` produces `v = -y` (MenuParts) and `Chara` quad meshes use v in [0, 0.5]; `MeshChara` uses scale (1,-1)/offset (0,1). => samplers must wrap (`GL_REPEAT`) with power-of-two textures (Menu textures are POT; verify, otherwise emulate by folding negative V in the CPU when writing UVs).
* `FilterMode.Point` for FLD textures (`init_model`), `Clamp`; default for the rest is bilinear + repeat (Unity default for imported textures; the pack stores `wrap_mode/filter_mode`).
* Mipmaps: only 3 textures had mips; pack stores base level only. `mip_bias` ignore.
* Alpha8 textures sample as `(0,0,0,a)` in Unity 4 GLES2 (GL_ALPHA): shaders FontAlpha8/CanvasAlpha8 use `.w` only.

## 3. Shaders / render state: see 04. Fixed draw-state record used by the backend
```cpp
struct ShaderDesc { const char* name; int queue; uint8_t cull /*0 off 1 back*/, zwrite, ztest /*DQV_CMP_*/, blend /*0 none 1 alpha 2 add-alpha*/, atest /*0 none 1 >0 2 !=1 */; uint8_t op /*TEXMUL,A8,SEPIA,GRAY,INVERT,FADE...*/, useVertexColor, useTint, useST, noMVP /*clip-space verts*/, lit; };
```

## 4. Backend structure (vitaGL)

```
Engine::Render():
  for cam in sorted(cameras):  Backend::BeginCamera(cam)   // viewport/scissor into the logical-surface FBO, clear, set V,P
       list = gather(cam.cullingMask)   // renderers, text meshes
       sort(list)                       // queue, then depth (see 2.4)
       for item: Backend::Draw(mesh, submesh, material, worldMatrix)   // state cache; textures re-bound every draw (dq5core's glBindTexture!)
  Backend::Present()                    // blit logical-surface FBO to the 960x544 backbuffer (letterbox/rotate), vglSwapBuffers; swap interval 2
```
* **Logical surface FBO** (W x H = 408x544 default; see 06): all cameras render into one persistent colour+depth FBO; presentation = one textured quad with optional 90 deg rotation (TATE) and aspect fit. Gains: exact `Screen.width/height`, no reliance on `glViewport` clipping (vitaGL does not clip, REPORT_RUNTIME §H), simple Capture/Sepia (source = this FBO).
* Fixed-function path (vitaGL FFP: modulate, alpha test, blend, 2 tex units) for ~40 of the 51 shaders; 6 small CG/GLSL programs for the rest (04 §4). `glColor4f(_Color)` when the mesh has no vertex colours; when it has them and `_Color != white`, multiply on the CPU into the dynamic colour array **or** use the GLSL "tint" program.
* Texture formats: A8 (`GL_ALPHA`), RGB565, RGBA4444, RGBA8888 from the pack (all natively supported by vitaGL/GXM). Upload once on first `Resources.Load`, free on `UnloadAsset/UnloadUnusedAssets` (mark & sweep over live Materials; RAM budget ~160 MB for textures).
* Skinned meshes (49 fbx prefabs, 58 clips): CPU skinning each frame into a dynamic vertex array (NEON); `Animation` evaluates `DqvCurve`s (hermite/linear with stored tangents: pack stores in/out tangents; sample at fixed 30 Hz is acceptable) into bone `localPosition/Rotation/Scale` of the cloned hierarchy; `WrapMode` Once/Loop/ClampForever; `CrossFade` = linear weight blend of two states for `blend_time` (0.3 s), `PlayQueued` = start after current ends. Few animation events (pack has events) - none used by C#.
* Colliders: ray vs OBB (Box), ray vs sphere (Sphere), ray vs triangles (Mesh). Menu frames use MeshCollider on 9-slice quads: test the quad triangles (cheap).

## 5. TextMesh and fonts (bitmap, NOT dynamic)

**Finding:** the 5 language fonts used in-game are *pre-baked bitmap Unity fonts* (sharedassets0.assets; `Font` objects with `m_CharacterRects`, empty `m_FontData`): `SE-GARRIR` (EN; 218 glyphs; 256x256 Alpha8 atlas; size 20, lineSpacing 23.525, ascent 18.887), `GMaruGoPro-M` (JA; 9911 glyphs; 2048x2048 A8; size 19, line 38), `FOTK-YDHopeL` (KO), `FZHTK_TL` (TC), `FZHTK_SL` (SC) (size 19). Selected by `MenuParts.s_langParam[lang]` = (fontName, BIAS_FONT_SIZE 1.105, BIAS_FONT_LINE (EN 0.8013), BIAS_FONT_Y (EN 3.5), NAME_BASE_SCALE). Only `mplus-2c-medium_16` (dynamic TTF 1.38 MB, OBB `ad41087b...`) is a dynamic font and is used by MenuPortItemDebug / ObbDownloadRes only => **drop; no stb_truetype/FreeType needed** (keep ff3_vita's stb_truetype only as optional future fallback for missing glyphs: C# would show nothing for unknown chars).
`TextMesh` algorithm to implement (Unity 4, `fontSize==0`, `richText=true`):
1. `scale = characterSize * font.pixelScale (0.1)` (so `characterSize=10` => 1 unit/pixel; MenuParts uses `size * BIAS_FONT_SIZE(1.105)`).
2. Parse rich text: `<color=NAME|#rrggbb[aa]>...</color>`, `<b>`,`<i>`,`<size=N>` (only `<color=...>` appears: yellow, red, gray, black, cyan + `@cN` replacement done by C# in `MenuPortItem.SetItemParam`). Colors become vertex colors.
3. Lines split on `\n`. Pen advances by `CharInfo.width` (pixel advance) * scale (no kerning data). Line height = `font.lineSpacing * textMesh.lineSpacing` * scale (C# passes `lineSpace * BIAS_FONT_LINE`).
4. Glyph quad: `x0 = pen + vert.x*scale; x1 = x0 + vert.width*scale; yTop = lineTop + vert.y*scale; yBot = yTop + vert.height*scale` (**`vert.height` is negative; `vert.y` is negative = below the line top**); UV rect `(uv.x, uv.y)` = top-left, `uv.width` / `uv.height` (negative) extents; **`flipped` glyphs (e.g. JA glyphs 9733, 9660...) are stored rotated 90 degrees in the atlas: swap u/v corner mapping** (UNVERIFIED corner order - verify visually with a JA flipped glyph: Unity writes TL=(uv.x, uv.y+uv.h?)...).
5. Block origin by `TextAnchor` (0 UpperLeft .. 8 LowerRight, 4 MiddleCenter): origin shifts by (w*ax, -h*ay) where block width = longest line, height = lines * line height; `TextAlignment` (Left 0, Center 1, Right 2) shifts each line within block width.
6. Local space of a TextMesh is the XY plane facing -Z; MenuParts rotates it with `Rotate(90,0,0)` onto the XZ plane for the top-down UI camera.
7. Material: `FontAlpha8` (vertex colour * `_Color`, alpha from texture A8). The C# assigns shared coloured materials `l_colorFontList[color]` via `MenuParts.SetTextColor(renderer, idx)`.
Performance: mesh rebuild only when `text` changes (dirty flag), cache by (text,font,params).

## 6. Things deliberately NOT in mini-Unity
Lighting, shadows, fog, skybox, LOD, culling by occlusion, batching, physics, audio, GUI/OnGUI (`MenuPortItemDebug`'s GUISkin: drop), sorting layers, multi-threaded rendering, `Resources.LoadAsync`, `AssetBundle`, `PlayerPrefs`.
