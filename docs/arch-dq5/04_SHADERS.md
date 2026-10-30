# 04 - Shader inventory and vitaGL implementation plan

Raw material (all produced by UnityPy from `extract/unity/**`): `arch/shaders/<Name>.shader.txt` (full ShaderLab text with the gles/gles3 programs),
`arch/shaders/<Name>.gles.txt` (only the GLES2 sub-programs), `arch/shaders/INDEX.txt` (auto summary: queue, render state, properties, uniforms).
53 custom Shader objects exist (51 game shaders + `MPad`, `Picture`, `MenuParts`, `BackColor`(file "shader1") counted in the 51 differently by the earlier census; all are listed below);
built-ins: `Transparent/Diffuse` ("Alpha-Diffuse", 809 materials), `Transparent/VertexLit` ("Alpha-VertexLit"), and `unity_builtin_extra:7` (13,272 materials - the FLD prefabs' default shader, **always replaced at runtime** by `FldObject.set_shader`, see 3.2), `unity default resources:10101` (6 materials).
Material census (UnityPy over all 21,604 files): builtin-extra:7 = 13,272; Alpha-Diffuse = 809; Effect0 = 52 (fbx models); DssaCharaTransparent 18; ImdShadeChara 12; ImdShadeChara2 2; MPad 2; Picture 1; MenuParts 1; default-resources 6. Everything else is created in C# by `Shader.Find`.

## 1. What the GLSL looks like (all of it)
Every game program is: vertex = `gl_Position = mvp * v` (or **clip-space passthrough**: `Capture`, `CaptureBlur`, `BackOrtho`, `BackFade`, `FrontFade`, `Pre/Post-Spotlight`, `Sepia`), optional `uv = uv0 * _MainTex_ST.xy + _MainTex_ST.zw`, optional `color = vertexColor * _Color`;
fragment = `texture2D(_MainTex,uv) * color` with optional `discard`. No loops, no derivatives, no extensions, no lights (except Imd*), 1 texture, `lowp` everywhere. Unity `_glesColor` defaults to white when the mesh has no colour array.
=> a handful of fragment "ops" cover all 53. Table below: **op** codes used by the backend `ShaderDesc`.

| op | fragment result (rgb, a) |
|---|---|
| `MUL` | `tex * C` where `C` = vertexColor*_Color / vertexColor / _Color (flags in the desc) |
| `MUL_AT0` | same + `discard if a == 0` (= alpha test GREATER 0) |
| `MUL_AT1` | same + `discard if a != 1` (CharaEx pass 1) |
| `FLAT` | `C` (no texture): fades, BGClear, SmallMapSymbol, DebugColl, BackColor |
| `A8` | `(C.rgb, C.a * tex.a)` (FontAlpha8: C = vertex*_Color; CanvasAlpha8: C = _Color only) |
| `RGB_A1` | `(tex.rgb, 1) * C` (Capture, CaptureBlur) |
| `INV_A` | `(C.rgb, (1 - tex.a) * C.a)` Pre-Spotlight; `(C.rgb, tex.a*C.a)` Post-Spotlight |
| `SEPIA_SCR` | `l=(r+g+b)/3; (l+0.109375, l-0.0546875, l, a)` (screen post-process, clip-space quad) |
| `SEPIA_FLD` | `c=tex*C; (dot(c.rgb,(0.299,0.587,0.114)) + (0.109375,-0.0546875,0), c.a)` |
| `GRAY` | `(sqrt(dot(t.rgb,t.rgb)) * _Brightness).xxx, t.a` (ButtonGray, disabled button) |
| `INVERT` | `(1 - t.rgb, t.a)` (ButtonReverse, pressed button) |
| `LIT` | ImdShadeChara: vertex stage computes `col = clamp( max(0, dot(vec3(-L.xy, L.z), normalize(mat3(Object2World)*normalize(n)))) * (_LColor*vc*_Color) + _Ambient + _Emission, 0, 1)`; `a = (_LColor*vc*_Color).a`; fragment `tex * col` |

## 2. Table of the 53 shaders

State columns: queue (Unity number), `cull` (default Back when blank), `zw` ZWrite (default On), `zt` ZTest (default LEqual), `blend` (`A` = SrcAlpha,OneMinusSrcAlpha; `ADD` = SrcAlpha,One), `at` alpha test.
`vc` = uses vertex colour, `C` = `_Color` uniform, `ST` = `_MainTex_ST`.  "FFP" = doable with vitaGL fixed function; "CG" = needs one of the 7 tiny CG programs (§4).

| shader (declared name) | used by (C#) | queue | cull | zw | zt | blend | at | op / inputs | impl |
|---|---|---|---|---|---|---|---|---|---|
| Custom/BackColor (file "shader1") | `BackColor.cs` (gradient quad, child of MainCamera at z=20), `SceneCamera.Apply` clear plane (layer 8) | 1000 Background | | off | always | - | - | FLAT: vertex colour only, **clip-space** (`gl_Position = _glesVertex`; the quad's transform is irrelevant but its GameObject layer/bounds still decide which camera draws it) | FFP |
| Custom/BGClear | `Shader.Find` only (s_clearShader unused) | 1000 | off | off | always | - | - | FLAT `_Color` | drop |
| Custom/BackFade | `Fade` plane [1] (BG fade) | 1000 | | off | always | A | - | FLAT `_Color`, **clip-space** (no mvp: verts are x,y in -1..1) | FFP |
| Custom/FrontFade | `Fade` plane [0] (front fade; swapped to BackFade with `SetFadePriority`) | 4000 Overlay | | off | always | A | - | FLAT `_Color`, clip-space | FFP |
| Custom/BackOrtho | MenuPortItemTable:2693 (encyclopedia background) | 1100 | off | off | always | - | - | `tex` raw, clip-space | FFP |
| Custom/Picture | `MenuParts.l_pictureShader` (logos, TapToStart, pictures, faces) | 3000 | | off | | A | - | MUL: `tex*_Color`, ST | FFP |
| Custom/MenuParts | `MenuParts.l_partsShader` (all UI frames/buttons/icons) | 3000 | | off | | A | - | MUL: `tex*_Color` (no ST) | FFP |
| Custom/FontAlpha8 | `MenuParts.l_fontShader` (all TextMesh) | 3000 | | off | | A | - | A8: `(vc*_Color).rgb, a*tex.a`, ST | FFP (GL_ALPHA tex, MODULATE) |
| Custom/CanvasAlpha8 | `MenuMessage` text canvas (A8 512x512 written by native) | 3050 | | off | | A | - | A8 with `_Color` only, ST (scroll) | FFP |
| Custom/ButtonGray | `MenuParts.l_grayShader` (disabled buttons) | 3000 | off | off | | A | - | GRAY `_Brightness`, ST | CG |
| Custom/ButtonReverse | `MenuParts.l_reverseShader` (pressed) | 3000 | off | off | | A | - | INVERT, ST | CG |
| Custom/MPad | TouchPad virtual-pad meshes (sharedassets1), fixed-function SetTexture combine | 3000 | off | off | | A | - | `tex * _Color` | FFP (drop if no virtual pad) |
| Custom/TestMenu | unused (debug) | 3000 | | off | | A | - | `_Color` | drop |
| Custom/Capture | `Capture` mesh child of MenuCamera (z=180) with RenderTexture of the world | 3000 | off | off | always | A | - | RGB_A1: `(tex.rgb,1) * (vc*_Color)`; **clip-space** verts (-1..1) | FFP (combine) or CG |
| Custom/CaptureBlur | `Capture` second material (half-size RT of MenuCamera) | 1000 | off | off | always | ADD | - | RGB_A1 with `_Color`, clip-space | FFP |
| Custom/Sepia | `Sepia.OnRenderImage` full-screen Blit (verts in [0,1] -> `(v-0.5)*2`) | 4000-50 | | off | always | - | - | SEPIA_SCR | CG |
| Custom/Pre-Spotlight | `Spotlight` outer ring material (sub-mesh 1 of `m_LightPoly`) | 4000-50 (3950) | | off | always | A | - | INV_A: `(color.rgb, (1-tex.a)*color.a)`, clip-space | CG |
| Custom/Post-Spotlight | `Spotlight` light disc | 4000-49 (3951) | | off | always | A | - | INV_A variant: `(color.rgb, tex.a*color.a)`, clip-space | FFP combine / CG |
| Custom/Chara | `Chara` (all billboard NPC/party sprites), `Riseup` ex, `MeshChara` list[5] | 3000 + priority | off | on | | A | >0 | MUL_AT0: `tex * _Color`, ST | FFP |
| Custom/CharaEx | `Chara` when `no & 0x800000`; **2 passes**: (1) cull off, ZWrite on, no blend, `discard if a!=1`; (2) cull off, ZWrite off, blend A, no discard | 3000 + priority | off | on/off | | none / A | `!=1` | MUL_AT1 then MUL, ST | FFP (glAlphaFunc(EQUAL,1)) |
| Custom/CharaShadow | `Chara` blob shadow (fixed-function; `ConstantColor [_Color]`, queue = priority-1) | 3000-1 | | off | | A | - | `tex * _Color` | FFP |
| Custom/Riseup | `Riseup` (item/damage icons, ZTest False) | 4000-150 | off | off | **always** | A | - | MUL: `tex*vc*_Color`, ST | FFP |
| Custom/Field | `Field.cs` world-map tile grid (`m_material`) | 2000 | off | on | | - | - | MUL: `tex * _Color`, no ST | FFP |
| Custom/Field_iOS | unused (iOS variant) | 2000 | off | on | | - | - | same | drop |
| Custom/FieldSymbol | `Field.cs` symbols (ships, markers) | 3000 | off | off | | A | - | MUL `tex*_Color` | FFP |
| Custom/FieldVolcano | `Volcano.cs` animated volcano | 3001 | off | off | | A | - | MUL `tex*vc` | FFP |
| Custom/SmallMapSymbol | `BlinkMapSymbol` | 2000 | off | on | | - | - | FLAT `_Color` | FFP |
| Custom/fld_normal | `FldObject.set_shader` (.tim texture opaque non-RGBA32) | 2000 | | on | | - | - | MUL: `tex * (vc*_Color)` | **CG TINT** (vc and `_Color` both vary) |
| Custom/fld_atest | same, RGBA32 texture with a==1 colour | 2450 AlphaTest | | on | | - | `a==0` discard | MUL_AT0 | CG TINT+AT |
| Custom/fld_trans | same, alpha<1 colour or non-.tim textures; also temporary alpha fades | 3000 (+priority) | | off | | A | - | MUL | CG TINT |
| Custom/fld_transz | like fld_trans but ZWrite ON + discard(a==0) (flag `0x08000000` objects / `tz_` textures) | 3000 | | on | | A | >0 | MUL_AT0 | CG TINT+AT |
| Custom/fld_*_sepia (normal/atest/trans) | `FldObject.calculateExecSepia` (op 1 fld_color) swaps by name `material.shader.name + "_sepia"` | same | | | | | | SEPIA_FLD | CG |
| Custom/DssaNormal | `MeshChara` list[4]; `DSSAObject` opaque | 2000 | off | on | | A | >0 | MUL_AT0 `tex*vc*_Color`, ST | FFP/CG |
| Custom/DssaChara | list[0] | 3100 | off | on | | A | >0 | MUL_AT0 | |
| Custom/DssaMonster0 | list[1] | 3150 | off | off | | A | - | MUL `tex*vc*_Color` | |
| Custom/DssaMonster1 | list[2] | 3200 | off | off | | A | - | MUL | |
| Custom/DssaCharaTransparent | list[3] (default for `no>=1000` meshes) | 4000-150 | off | off | **always** | A | - | MUL | |
| Custom/BssaCharaTransparent | list[6] | 3000 | off | on | less | A | >0 | MUL_AT0 | |
| Custom/BssaEffectTransparent | list[7] (effects) | 3001 | off | off | less | A | - | MUL (`vc`, ST without offset) | |
| Custom/Effect0 | fbx models' own materials (52) | 3300 | off | off | | A | >0 | MUL_AT0 `tex*_Color`, ST | FFP |
| Custom/Particle | `ParticleSystem` | 3400 | off | off | | A | - | MUL `tex*vc` | FFP |
| Custom/ImdChara | prefab materials (cutscene/minigame characters) | 2000 | | on | | A | - | MUL `tex*vc*_Color` | CG TINT |
| Custom/ImdShadeChara, ImdShadeChara2 (queue 3000) | prefab materials (14); props `_LDir,_LColor,_Ambient,_Emission` set in the material asset | 2000 / 3000 | | on / | | A | - | LIT | CG LIT (or CPU lighting) |
| Custom/PanchThrough, PanchThroughColor | prefab materials (slime-touch UI/boards) | 3000 | off / | off | | A | - | `tex` / `tex*_Color`, ST | FFP |
| Custom/Simple, SimpleTransparent | prefab materials (fixed function `SetTexture combine texture`) | 2000 / 3000 | | on / off | | - / A | - | `tex` | FFP |
| Custom/DebugColl | DebugDispColl | 2000 | off | | | - | - | FLAT `vc*_Color` | drop |
| Transparent/Diffuse ("Alpha-Diffuse") | 809 materials (prefabs); no lights in any scene => result = `tex * _Color * ambient(0.2)*2`?? (UNVERIFIED; only matters for materials not re-shaded by FldObject: verify which prefabs keep it) | 3000 | | off | | A | >0 | MUL | FFP |
| Transparent/VertexLit | built-in, unused | | | | | | | | drop |

(Queue numbers: Background 1000, Geometry 2000, AlphaTest 2450, Transparent 3000, Overlay 4000.)

## 3. How the C# chooses shaders (so the engine must support runtime swaps)

### 3.1 Static shader users
`Chara`: `Custom/Chara` or `CharaEx`; shadow `CharaShadow`; `Riseup`: `Custom/Riseup` or `Custom/Chara`; `MeshChara`: array `{DssaChara, DssaMonster0, DssaMonster1, DssaCharaTransparent, DssaNormal, Chara, BssaCharaTransparent, BssaEffectTransparent}` indexed by native op 25; `Fade`, `BackColor`, `Capture`, `Spotlight`, `Sepia`, `Particle`, `Field`, `FieldSymbol`, `FieldVolcano`, `SmallMapSymbol`, MenuParts: `MenuParts/FontAlpha8/Picture/ButtonGray/ButtonReverse`, `CanvasAlpha8`, `BackOrtho`.

### 3.2 FLD (town/dungeon) models
All `.fld` prefab materials start with the built-in shader and no `_Color`... `init_model` (on load) -> for every material with a `mainTexture`: `set_shader(material)`:
```
tex = (Texture2D) mtl.mainTexture
if tex.name contains ".tim":        // PS2-converted textures keep the file extension in the asset name!
     if mtl.color.a != 1          -> fld_trans
     else if tex.format != RGBA32 -> fld_normal            (also mtl.SetVector("_TexScale", (1,1,0,0)) - property does not exist in the shader: no-op)
     else                          -> fld_atest
else                               -> fld_trans
tex.filterMode = Point; tex.wrapMode = Clamp;
```
Later logic reads `tex.name.IndexOf("tz_")` (transz split), `material.name.IndexOf(x) > 0` (palette animation targets), `material.GetColor("_OrgColor")` (`save_org_color` sets it), `material.shader.name` (+ "_sepia"). **Contract for the pack/engine: `Texture2D.name` must be the original asset name (with ".tim"/".tm2"), `Texture2D.format` must report the ORIGINAL Unity TextureFormat (RGBA32=4 vs RGB24=3...), `Material.name` must be preserved.** (pack: `DqvObject.name_off` and `DqvTexture.orig_format` exist - good.)
FLD colour tinting each frame: `m_color` (time-of-day, op 35) `material.color = _OrgColor * m_color` for normal materials; `transz` ones use `_OrgColor`.

## 4. Implementation proposal on vitaGL (checked against `/Users/frangar/Fun/vitaGL`)

vitaGL capabilities confirmed by source: fixed-function pipeline with texture environment (GL_MODULATE, GL_COMBINE unless `NO_TEX_COMBINER`), 2 texture units by default, `glAlphaFunc`, blend, depth func/mask, cull, scissor, client-array drawing, VBOs, FBOs with texture + renderbuffer attachments (`glFramebufferTexture2D`, `glBlitFramebuffer`, `vglTexImageDepthBuffer`), `glCopyTexSubImage2D`, textures: `GL_ALPHA/LUMINANCE`, RGB565, RGBA4444, RGBA8888, ETC1/PVRTC, runtime GLSL->GXP translator (`vglSetupRuntimeShaderCompiler`, needs `libshacccg.suprx`) **or** precompiled GXP binaries via `glShaderBinary` (CG compiled offline with `psp2cgc`; ff3_vita and dsvita (`src/core/graphics/gpu_3d/shaders/cg/*.cg`) use exactly this). `vglInit*` before use, `vglSetShaderCachePath` irrelevant for precompiled.

**Decision**
1. **Primary: fixed-function** for every `FFP` row (about 40 of 53, and every shader that matters for the first 90 % of screen time: UI, text, chara sprites, field, DSSA, fades). No shader compile, no libshacccg, no first-run hitches.
2. **Secondary: 7 precompiled CG programs** (`.gxp` files shipped in the VPK, compiled by the build with `psp2cgc`; sources in `port/shaders/*.cg`, named like `dqv_<op>_v.cg/_f.cg`):
   `tint` (tex * vc * uColor, flags: alpha-test mode, A8, RGB_A1 via `#define`), `sepia_fld`, `sepia_scr`, `gray`, `invert`, `inv_a` (spotlight), `lit` (ImdShade). All uniform names mirror the Unity ones (`_Color`, `_MainTex_ST`, `_Brightness`, `_LDir`...).
3. The backend selects by `ShaderDesc.impl`. A CPU fallback exists for `tint` when `vc != null && _Color != white`: multiply into the per-draw dynamic colour array (acceptable for the dynamic meshes; for static FLD meshes use the CG).
4. Do **not** rely on vitaGL's GLSL translator for the Alpha-Diffuse/Imd variants (`Alpha-Diffuse` has SH-lighting code that the translator may not handle; REPORT_RUNTIME §H) - and we don't need it.
5. Clip-space shaders (`Capture*`, `*Fade`, `BackOrtho`, `Sepia`, spotlight): set MVP = identity (or draw a fullscreen quad in a pass with identity matrices) - the C# gives vertices already in [-1,1].
6. State changes: sort within a queue by (shader, texture) only if depth order is irrelevant; otherwise keep Unity order. Cache GL state, but **always rebind the texture** at each draw (dq5core's `TextCanvasTransfer` leaves its own binding behind).
7. Alpha test GREATER 0 = `glAlphaFunc(GL_GREATER, 0.0f)`. Equal-1 for CharaEx pass 1 = `glAlphaFunc(GL_EQUAL, 1.0f)` (if vitaGL rejects EQUAL, use the `tint` CG with `MUL_AT1`).
8. Blend `SrcAlpha, OneMinusSrcAlpha` is the only blend except `CaptureBlur` (`SrcAlpha, One`).
9. DS-GX effects are *not* ported as GX state (no polygon IDs, toon, edge marking, fog): the native side already flattened DSSA/BSSA into quads+alpha (02 §E). `/Users/frangar/Fun/dsvita` (Rust DS emulator, vitaGL CG renderer) is **not needed**; useful only as an example of precompiled CG (`gpu_3d/shaders/cg/render_{vert,frag}.cg`) and Vita buffer handling.

## 5. Special rendering paths using these shaders
* **Capture** (`Capture.cs`, 1115 lines, 4 coroutine effects + snapshots): `RenderTexture capTex (W x H, depth16)` captures `Field.m_camera` (world map) or `MainLoop.m_camera`; a mesh of 32 x `divY` quads (divY = 32/aspect) child of MenuCamera at z=180 displays the texture ("Capture" mat) distorted per frame by the effect (Tabi/warp swirl, DarkLamp, Riremito, Battle swirl/drop) while `TouchPad.m_camera.clearFlags = Color` during the effect; blur path renders the Menu camera into two half-size RTs (ping-pong) and draws them with additive `CaptureBlur`. Port: `Camera::Render()` into an FBO texture; keep meshes dynamic. These effects are cosmetic: **schedule after "first field" (WP-F)**.
* **Sepia**: `OnRenderImage` on Camera.main => render MainCamera into the logical-surface FBO, then a full-screen `SEPIA_SCR` pass into itself via a ping-pong texture (or `glCopyTexSubImage2D` of the colour buffer into a temp texture then draw).
* **Spotlight**: 12-vertex ring polygon (2 sub-meshes, 2 materials, `etc/en01|en02` mask texture), child of MainCamera at z=20 layer 0.
* **BackColor**: 4 vertices with vertex colours; `mesh.colors` updated when the native sends op 9.
