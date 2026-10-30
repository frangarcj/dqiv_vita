# DQIV pack format, version 1

Output of `dqiv_convert.py`; loaded on Vita from `ux0:data/dqiv/pack/`. The authoritative C definitions are in
`dqiv_pack.h` (sizes are checked at compile time in `dqiv_pack.c`); this file explains the layout and the conventions.
Everything is **little endian**, naturally aligned. Reference loader: `dqiv_pack.c`; validator/test: `test_pack.c`;
pure-python dumper: `dqiv_inspect.py`.

## 0. Running the converter

```
cd /Users/frangar/Fun/dqiv/work/tools/convert
../venv/bin/python -I dqiv_convert.py                 # -> extract/pack/ and extract/pack_shaders/ (~25 s on 8 cores)
../venv/bin/python -I dqiv_convert.py --help          # --out --shaders-out --jobs --rgb24 --rgba32 --demote-opaque --limit-files --only-prefix -v
make && ./test_pack ../../extract/pack               # reference loader + full validation (C99, also builds for Vita)
python3 dqiv_inspect.py ls fbx/ ; python3 dqiv_inspect.py show scene/level0
```
Needs UnityPy, texture2ddecoder (already in `tools/venv`) and numpy (added to the venv). Copy `extract/pack/{index.bin,meta.pak,mesh.pak,tex.pak,blob.pak}`
to `ux0:data/dqiv/pack/` (index.txt/stats/failures are optional). Stages: read the ResourceManager of `mainData` → enumerate all 21 608 serialized files and give every
exportable object a gid → convert files in parallel workers (blobs already contain final gids) → dedupe + lay out sorted by Resources path → write index.

## 1. Files

| file | content | notes |
|---|---|---|
| `index.bin` | header, object table, Resources table, string pool | read fully at startup (~2.4 MB, kept in RAM) |
| `meta.pak` | Material, Shader, Prefab/scene, AnimClip blobs | small objects (3.7 MB) |
| `mesh.pak` | Mesh blobs | 38 MB |
| `tex.pak` | Texture blobs | 150 MB |
| `blob.pak` | TextAsset + Font blobs | 12.5 MB |
| `index.txt` | human readable list of every Resources path (path, kind, id, size, file:offset, aux) | not needed at runtime |
| `stats.json`, `failures.txt`, `warnings.txt` | converter report | not needed at runtime |

Why a few archives with an index instead of 21 600 files: opening a file on the Vita memory card costs milliseconds
and the object graph is huge (14k materials, 12.6k meshes, 5.9k textures). With four open `FILE*` handles every asset is one
`fseek+fread` into a 16-byte aligned buffer. Blobs are laid out sorted by Resources path (all pieces of one map / one
character are adjacent), identical blobs are stored once (dedupe by SHA-1: 22 MB of textures, 10.6 MB of meshes saved), and
blobs are position-independent (internal offsets are relative to the blob start) so a blob can be used in place after `fread`,
vertex/index arrays are 16-byte aligned for DMA/GPU upload. Only the part of the pack the game touches is ever read.

## 2. Identifiers

* **gid** (`obj_id`): index into the object table; `0` = null. All cross references between blobs are gids (u32).
* `gid | 0x80000000` (`DQIV_BUILTIN_BIT`) = reference to a Unity built-in resource (`pathID` in the low 31 bits) that is not in
  the pack (e.g. the cube mesh 10209 of the debug collider, the default `Font Material` shader 10101, the FBX importer's
  default shader `resources/unity_builtin_extra:7`). The engine should map them to its own defaults.
* **Resources path**: the lower-cased `Resources.Load` key (`fbx/c0003`, `charatex/h096`, `map/m/.../x.fld`, `menu/mplus-2c-medium_16`,
  `endroll/retro5_endroll_en_01.bin`, `field/field` ...). A path can have several entries of different kind (a `.fld` key holds the
  Prefab plus its Meshes/Materials; `menu/<font>` holds Font+Material+Texture; `shader/picture` holds 18 Shaders). Look up with
  `dqiv_find(pack, path, KMASK(kind))` (case-insensitive) or iterate `dqiv_find_range`. Two extra keys exist that are not in
  Unity's container: `scene/level0` and `scene/maindata` (the two player scenes exported as prefabs).
  `Resources.Load("x.fld", typeof(Transform))` = the Prefab entry of key `x.fld` (root GameObject → first node).
* A `GameObject` is only an exportable object when it is a prefab root named by the Resources container; the children are
  nodes inside the Prefab blob.

## 3. index.bin

```
0x000  DqvIndexHeader (128 B)
         char magic[4]="DQIV"; u32 version=1; u32 obj_count; u32 res_count; u32 objs_off; u32 res_off;
         u32 str_off; u32 str_size; u32 data_file_count=4; u32 flags=0; u32 pad[6];
         struct{u32 name_off; u32 size;} data_files[8]   // 0 meta.pak 1 mesh.pak 2 tex.pak 3 blob.pak
objs_off  DqvObject[obj_count]   (24 B, entry 0 = null)
           u32 kind; u32 data_file; u32 offset; u32 size; u32 name_off; u32 aux;
res_off   DqvResEntry[res_count] (16 B) sorted by (path bytes, kind, obj_id)
           u32 path_off; u32 obj_id; u32 kind; u32 reserved;
str_off   string pool: NUL terminated utf-8, offset 0 == ""
```
`kind`: 1 Texture, 2 Mesh, 3 Material, 4 Shader, 5 Prefab, 6 AnimClip, 7 TextAsset, 8 Font.
`offset` is a 16-aligned byte offset in `data_file`; `size == 0` marks an object that failed to convert (none in the shipped data).
`aux`: Texture `(w<<16)|h`; Mesh vertex count; TextAsset byte count; Prefab node count; Font ttf size; AnimClip curve count.

## 4. Blob header and strings

Every blob starts with `u32 magic; u32 size` (`size` = total blob size, a multiple of 16). Strings referenced inside a blob are
**u32 offsets from the blob start** to NUL-terminated utf-8 text placed at the end of the blob; `0` = empty. Arrays are referenced by
`(count, offset)` pairs with blob-relative offsets. Use `DQIV_BLOB_STR(blob, off)` / `DQIV_BLOB_AT(T, blob, off)`.

## 5. Texture `DQTX` (48 B header + pixels at 48)

```
0 magic 'DQTX'  4 size  8 u32 format  12 u16 width  14 u16 height  16 u16 orig_width  18 u16 orig_height
20 u32 flags  24 u8 wrap  25 u8 filter  26 u8 aniso  27 u8 orig_format  28 u32 data_off(=48)  32 u32 data_size
36 u32 mip_count(=1)  40 f32 mip_bias  44 u32 reserved
```
* `format`: 1 A8, 2 RGB565, 3 RGBA4444, 4 RGBA8888. Rows are tightly packed, **row 0 is the bottom row** (Unity/GL order, the C#'s
  UVs work unchanged; do not flip). GL mapping: A8 = `GL_ALPHA` bytes; RGB565 = `GL_UNSIGNED_SHORT_5_6_5` (R in bits 15..11);
  RGBA4444 = `GL_UNSIGNED_SHORT_4_4_4_4` (R in bits 15..12, A in 3..0); RGBA8888 = bytes R,G,B,A. 16-bit values are LE.
* Conversions (defaults): RGB24→RGB565 (opaque; `--rgb24 rgba8888` keeps 24-bit), RGBA32→RGBA8888 (alpha kept exactly;
  `--rgba32 rgba4444`, `--demote-opaque` are optional RAM savers; all shipped RGBA32 textures really use alpha), RGBA4444/RGB565/Alpha8 copied
  bit-exact, ETC1 (35 textures) decoded to RGB565, ARGB4444/ARGB32/BGRA32/DXT1/DXT5 supported by the converter (unused by this game).
  `orig_format` records the Unity `TextureFormat` id of the source.
* `flags`: 1 NPOT (4 textures; Vita handles NPOT only for non-mipmapped linear textures, none uses mipmaps), 2 HAS_ALPHA (any alpha ≠ opaque),
  4 HAD_MIPMAPS (source had a mip chain; only the base level is stored, `glGenerateMipmap` if wanted), 8 never resized.
* `wrap`: Unity 0 repeat / 1 clamp. `filter`: 0 point / 1 bilinear / 2 trilinear. Largest texture is 2048×2048 (`field/field` atlas etc.).
* One texture is 0×0 (`data_size 0`): Unity's placeholder atlas of the dynamic font `mplus-2c-medium_16`.

## 6. Mesh `DQMS` (96 B header)

```
0 magic  4 size  8 vertex_count  12 vertex_stride  16 index_count  20 submesh_count  24 bone_count  28 flags
32 i16 off_pos,off_nrm,off_col,off_uv0,off_uv1,off_tan (byte offset in the vertex or -1)   44 reserved
48 vertex_off  52 index_off  56 submesh_off  60 bindpose_off  64 skin_off   68 f32 aabb_center[3]  80 f32 aabb_extent[3]  92 reserved
```
* Vertex = interleaved, in this order and only the present attributes, no padding: `pos float3 | nrm float3 | tan float4 |
  col ubyte4 (R,G,B,A, normalized) | uv0 float2 | uv1 float2`. `vertex_stride` is a multiple of 4; the data block is 16-aligned and can be
  passed to `glVertexAttribPointer` as is (copy into the GPU pool). flags: 1 NRM, 2 COL, 4 UV0, 8 UV1, 16 TAN, 32 SKIN.
  (12 586 of 12 634 meshes are pos+col+uv0, stride 24.)
* Indices: `u16`, all submeshes concatenated. Submesh (48 B): `u32 first_index (in u16 units), index_count, topology, first_vertex,
  vertex_count; f32 aabb_center[3], aabb_extent[3]; u32 reserved`. topology 0 triangles (all shipped meshes), 1 strip, 3 lines, 4 line strip,
  5 points; Unity quads are converted to triangles by the converter. Winding/handedness as in Unity (left-handed, clockwise front).
* Skinning (29 meshes, flag 32): `bindpose_off`: `float[16]` per bone, **column-major** (`m[col*4+row]`, i.e. Unity `Matrix4x4.eRC` stored as
  `m[C*4+R]`), `skin_off`: per vertex `u8 bone[4]; f32 weight[4]` (20 B) indexing the bind poses; the bone→node mapping is in the
  SkinnedMeshRenderer component. Skin is kept out of the interleaved buffer so the engine can choose CPU skinning (29 meshes, a few hundred vertices each) or its own vertex-shader scheme.
* Compressed Unity meshes are not present in this game (`m_MeshCompression` is 0 everywhere); the converter raises if it meets one.

## 7. Material `DQMT` (64 B header)

```
0 magic 4 size 8 shader_id(gid or 0=builtin) 12 i32 render_queue(-1 = shader's) 16 shader_name_off
20 tex_count 24 tex_off 28 float_count 32 float_off 36 color_count 40 color_off 44 kw_count 48 kw_off 52 main_tex(gid) 56,60 reserved
```
* `DqvTexEnv` (24 B): `name_off, tex gid, f32 scale[2], f32 offset[2]` (`_MainTex_ST = (scale, offset)`); `DqvFloatProp` (8 B): `name_off, f32`;
  `DqvColorProp` (20 B): `name_off, f32 rgba[4]`; keywords: `u32 name_off[kw_count]`.
* `shader_name` is the declared ShaderLab name (`Custom/Chara`) or `builtin:<file>:<pathid>` for Unity built-ins
  (13 272 FBX-imported materials use `builtin:resources/unity_builtin_extra:7`; the C# replaces their shader at runtime, e.g. `Chara`, `Field`).
* Shipped materials only carry `_MainTex` (+scale/offset) and `_Color` (a few have `_Ambient/_Emission/_LColor/_LDir/_SpecColor/_Shininess...`).

## 8. Shader `DQSH` (48 B header)

```
0 magic 4 size 8 name_off 12 i32 render_queue(-1) 16 dump_name_off 20 fallback_off 24 pass_count 28 prop_count 32 gles_count 36 passes_off 40 props_off 44 reserved
```
* `DqvShaderPass` (40 B): `i32 zwrite(0/1,-1 unspecified), ztest(CMP), cull(0 off,1 front,2 back), blend_src, blend_dst (BLEND, -1/-1 = off), alpha_test_func(CMP,-1 off);
  f32 alpha_test_ref; i32 color_mask(bits RGBA, 15); f32 offset_factor, offset_units`. CMP: 0 never,1 less,2 equal,3 lequal,4 greater,5 notequal,6 gequal,7 always.
  BLEND: 0 zero,1 one,2 srccolor,3 srcalpha,4 dstcolor,5 dstalpha,6 1-srccolor,7 1-srcalpha,8 1-dstcolor,9 1-dstalpha,10 srcalphasaturate.
  Fixed-function state is parsed from the first SubShader (SubShader-level state is inherited by every pass).
* `DqvShaderProp` (12 B): three string offsets: name, type text (`Color`, `2D`, `Range(0,1)`...), default text.
* The compiled programs are not in the pack. For the engine author the converter writes to **`extract/pack_shaders/`**: `<Name>.txt` = the complete
  raw ShaderLab text (all GLES2/GLES3/other SubPrograms), and `<Name>.pP.vV.vert/.frag` = the GLES2 (`!!GLES`) vertex/fragment GLSL of pass `P`, variant `V`
  (first line `// keywords: ...`). Uniforms follow Unity 4 conventions (`glstate_matrix_mvp`, `_MainTex_ST`, `unity_Scale`, `_Color` ...).
  `gles_count` = number of such variants.

## 9. Prefab / scene `DQPF`

```
0 magic 4 size 8 node_count 12 comp_count 16 nodes_off(=64) 20 comps_off 24 root_count 28 flags(1=scene) 32..63 reserved
nodes_off: DqvNode[node_count] (80 B)   comps_off: DqvComp[comp_count] (16 B)   then component payloads (4-aligned), then strings
```
* Nodes are in depth-first pre-order (a parent always has a smaller index than its children; roots are chained with `next_sibling` too, scenes have
  several roots). `DqvNode`: `u32 name_off; i32 parent, first_child, next_sibling; f32 pos[3]; f32 rot[4] (x,y,z,w); f32 scale[3]; u8 layer; u8 active;
  u16 tag; u32 comp_first, comp_count; u32 src_path_id; u32 reserved[2]`. TRS are *local*. `tag` is Unity's tag id (5 = MainCamera).
* `DqvComp`: `u32 type (Unity class id); u32 enabled; u32 data_off; u32 data_size`. Transform is the node itself, not a component. The legacy Animator
  (FBX importer defaults) is dropped.
* Typed payloads: **MeshFilter(33)** `{u32 mesh}`; **MeshRenderer(23)** `{u32 flags(bit0 cast, bit1 receive); i32 lightmap_index; i32 sorting_order; u32 mat_count; u32 mats[]}`;
  **SkinnedMeshRenderer(137)** `{u32 flags(+bit2 update offscreen); u32 mesh; i32 root_bone(node); u32 mat_count, bone_count; f32 aabb[6]; i32 quality; u32 mats[]; i32 bones[] (node indices)}`
  (bones[i] ↔ bind pose i of the mesh); **Animation(111)** `{i32 wrap_mode; u32 play_automatically; i32 culling; u32 default_clip; u32 clip_count; u32 clips[]}`;
  **Camera(20)** (80 B) `{u32 clear_flags; f32 bg[4]; f32 viewport[4]; f32 near, far, fov, ortho_size; u32 orthographic; f32 depth; u32 culling_mask; i32 rendering_path; u32 target_texture; u32 hdr; u32 occlusion}`.
* **Everything else** (MonoBehaviour 114, TextMesh 102, GUILayer 92, SphereCollider 135, MeshCollider 64 ...) is a *property list*: `{u32 class_name_off; u32 count; DqvProp[count]}`
  (for MonoBehaviour `class_name` is the script class: `MainLoop`, `TouchPad`, `Boot`). `DqvProp` (24 B) = `u32 name_off; u32 type; 16-byte value`; types: 0 INT/enum (i[0]), 1 FLOAT (f[0]),
  2 BOOL (i[0]), 3 STRING (u[0]=string offset), 4/5/6 VEC2/VEC3/VEC4 (colours, rects and quaternions are VEC4), 7 ASSET (u[0]=gid or BUILTIN|pathID), 8 LOCAL (u[0]=node index in this prefab, u[1]=class id of
  the referenced component, 4 = the Transform/GameObject itself). Nested fields are dotted (`m_Color.rgba`), arrays are `name.size` + `name[i]` (first 64 elements).
  MonoBehaviour field layouts are decoded from the C# declarations (Unity stores no type tree for them): MainLoop (no fields), TouchPad (9 refs `m_siColDebug ... m_siMoveDirNormal`), Boot
  (`m_logoTexture[] m_logoShader m_obbMenuParts m_obbFont[] m_obbMat[] m_obbmsg m_quitmsg`).
* `scene/maindata` = Main Camera (+GUILayer + `Boot` script) and the "New Text" TextMesh with its MeshRenderer; `scene/level0` = MainCamera (+`MainLoop`), MenuCamera (+`TouchPad`),
  MPad and its cursor/direction meshes, `cdDebug`.

## 10. Animation clip `DQAN` (64 B header)

```
0 magic 4 size 8 f32 sample_rate 12 f32 duration 16 i32 wrap_mode 20 curve_count 24 curves_off 28 event_count 32 events_off 36 f32 bounds_center[3] 48 f32 bounds_extent[3] 60 reserved
```
* **Curves are stored keyed, not resampled** (exactly what Unity serializes: cubic Hermite with in/out tangents; all 58 clips are 60 Hz FBX bakes, so linear/slerp between keys is a fine approximation,
  Hermite evaluation is exact). `DqvCurve` (32 B): `u32 path_off ("a/b/c" relative to the GameObject that owns the Animation component, names match node names), attr_off, type, class_id, key_count, keys_off; i32 pre_infinity, post_infinity`
  (serialized value; 2 in every shipped curve = hold the first/last key). type 0 POSITION / 2 SCALE: keys of 10 floats `{t, v[3], inTangent[3], outTangent[3]}`; type 1 ROTATION (quaternion x,y,z,w): 13 floats `{t, q[4], in[4], out[4]}`;
  type 3 FLOAT (named property on `class_id`): 4 floats `{t, v, in, out}`. Keys are sorted by time (validated).
* `DqvAnimEvent` (24 B): `f32 time; u32 func_off, data_off; f32 float_param; i32 int_param; u32 reserved` (none in the shipped clips).
* Dropped: muscle clips/humanoid data (empty in these assets), `PPtr` curves (none).

## 11. TextAsset `DQTA`

`{u32 magic, size, data_size, reserved}` then the raw bytes at offset 16 (bit exact, e.g. `.map` tables, `.bin`, UTF-8 menu text, `sound/list`, `version`). Not NUL terminated.

## 12. Font `DQFN` (96 B header)

```
0 magic 4 size 8 f32 font_size 12 line_spacing 16 ascent 20 kerning 24 pixel_scale 28 i32 character_spacing 32 character_padding 36 convert_case
40 ascii_start_offset 44 default_style 48 rendering_mode 52 texture gid 56 material gid 60 rect_count 64 rects_off 68 kern_count 72 kern_off
76 ttf_size 80 ttf_off(16-aligned) 84 font_name_off 88 fallback_count 92 reserved
```
`DqvCharRect` (44 B) = Unity `CharacterInfo`: `u32 index(code point); f32 uv x,y,w,h; f32 vert x,y,w,h; f32 advance; u32 flipped`; `DqvKernPair` (8 B) `{u16 first, second; f32 amount}`.
`Menu/mplus-2c-medium_16` is a *dynamic* font (1.38 MB TrueType file embedded at `ttf_off`, no rects: rasterize it with stb_truetype/FreeType); the other five fonts
(`SE-GARRIR` 218 glyphs/256², `FOTK-YDHopeL` 8418/1024×2048, `FZHTK_TL` 2153 and `FZHTK_SL` 2138/1024², `GMaruGoPro-M` 9911/2048²) are
static bitmap fonts with `rect_count > 0` and an A8 atlas texture (`texture_id`).

## 13. Validation (`test_pack`)

`make && ./test_pack /Users/frangar/Fun/dqiv/work/extract/pack` opens the pack, checks the Resources table order/lookups, then loads **every** object and runs
`dqiv_validate` (magics, sizes, alignment, every offset/count range, string termination, all gid references and their kinds, mesh indices < vertex count,
skin bone indices, node tree links, payload sizes, curve key order). It must print `OK`.
