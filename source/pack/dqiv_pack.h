/*
 * dqiv_pack.h - on-disk structures of the DQIV asset pack + reference loader API.
 * See FORMAT.md for the full byte-exact specification and the rationale.
 *
 * Plain C99, no dependencies besides libc.  All data is LITTLE ENDIAN; every struct below is
 * naturally aligned with no padding the compiler could change (sizes are asserted in dqiv_pack.c).
 * Offsets inside a blob are bytes from the start of that blob; strings are NUL-terminated utf-8 inside the
 * blob (offset 0 == "no string").  Object references ("gid") index DqvIndex::objects (0 == null);
 * gid | DQIV_BUILTIN_BIT == a Unity built-in resource (pathID in the low bits) that is NOT in the pack.
 *
 * Conventions kept from Unity (the C# code expects them): left-handed, +Y up, quaternions x,y,z,w,
 * texture/UV origin bottom-left (texture row 0 is the BOTTOM row), matrices stored column-major (GL).
 */
#ifndef DQIV_PACK_H
#define DQIV_PACK_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DQIV_FOURCC(a, b, c, d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))
#define DQIV_BUILTIN_BIT 0x80000000u

/* object kinds */
enum {
    DQIV_K_TEXTURE = 1, DQIV_K_MESH = 2, DQIV_K_MATERIAL = 3, DQIV_K_SHADER = 4,
    DQIV_K_PREFAB = 5, DQIV_K_ANIM = 6, DQIV_K_TEXT = 7, DQIV_K_FONT = 8, DQIV_K_COUNT = 9
};
#define DQIV_KMASK(k) (1u << (k))
#define DQIV_KMASK_ALL 0x1FEu

/* data files (DqvIndexHeader.data_files[i]) */
enum { DQIV_DF_META = 0, DQIV_DF_MESH = 1, DQIV_DF_TEX = 2, DQIV_DF_BLOB = 3, DQIV_DF_COUNT = 4 };

/* blob magics */
#define DQIV_MAGIC_INDEX  DQIV_FOURCC('D', 'Q', 'V', 'P')
#define DQIV_MAGIC_TEX    DQIV_FOURCC('D', 'Q', 'T', 'X')
#define DQIV_MAGIC_MESH   DQIV_FOURCC('D', 'Q', 'M', 'S')
#define DQIV_MAGIC_MAT    DQIV_FOURCC('D', 'Q', 'M', 'T')
#define DQIV_MAGIC_SHADER DQIV_FOURCC('D', 'Q', 'S', 'H')
#define DQIV_MAGIC_PREFAB DQIV_FOURCC('D', 'Q', 'P', 'F')
#define DQIV_MAGIC_ANIM   DQIV_FOURCC('D', 'Q', 'A', 'N')
#define DQIV_MAGIC_TEXT   DQIV_FOURCC('D', 'Q', 'T', 'A')
#define DQIV_MAGIC_FONT   DQIV_FOURCC('D', 'Q', 'F', 'N')

/* ======================================================================= index.bin */
typedef struct DqvIndexHeader {          /* 128 bytes, at file offset 0 */
    char     magic[4];                   /* "DQIVP" */
    uint32_t version;                    /* 1 */
    uint32_t obj_count;                  /* number of DqvObject entries INCLUDING the null entry 0 */
    uint32_t res_count;                  /* number of DqvResEntry */
    uint32_t objs_off;                   /* file offset of DqvObject[obj_count] */
    uint32_t res_off;                    /* file offset of DqvResEntry[res_count] (sorted) */
    uint32_t str_off;                    /* file offset of the string pool */
    uint32_t str_size;
    uint32_t data_file_count;            /* 4 */
    uint32_t flags;                      /* 0 */
    uint32_t pad0[6];
    struct { uint32_t name_off; uint32_t size; } data_files[8];   /* name = string pool offset */
} DqvIndexHeader;

typedef struct DqvObject {               /* 24 bytes; array index == gid */
    uint32_t kind;                       /* DQIV_K_*, 0 for the null entry */
    uint32_t data_file;                  /* DQIV_DF_* */
    uint32_t offset;                     /* byte offset in the data file (16-aligned) */
    uint32_t size;                       /* blob size in bytes (0 == conversion failed / absent) */
    uint32_t name_off;                   /* string pool offset: original asset name */
    uint32_t aux;                        /* TEX: (w<<16)|h, MESH: vertex count, TEXT: byte count, PREFAB: node count */
} DqvObject;

typedef struct DqvResEntry {             /* 16 bytes; sorted by (path bytes, kind, id) */
    uint32_t path_off;                   /* string pool offset, lower-case Resources.Load path */
    uint32_t obj_id;
    uint32_t kind;
    uint32_t reserved;
} DqvResEntry;

/* ======================================================================= Texture (DQTX) */
enum { DQIV_TF_A8 = 1, DQIV_TF_RGB565 = 2, DQIV_TF_RGBA4444 = 3, DQIV_TF_RGBA8888 = 4 };
enum { DQIV_TFL_NPOT = 1, DQIV_TFL_HAS_ALPHA = 2, DQIV_TFL_HAD_MIPS = 4, DQIV_TFL_NOT_RESIZED = 8 };

typedef struct DqvTexture {              /* 48 bytes, pixel data follows at data_off (== 48) */
    uint32_t magic, size;
    uint32_t format;                     /* DQIV_TF_*  (GL: A8=GL_ALPHA/UNSIGNED_BYTE, 565=UNSIGNED_SHORT_5_6_5,
                                            4444=UNSIGNED_SHORT_4_4_4_4 (R in bits 15..12), 8888=bytes R,G,B,A) */
    uint16_t width, height;              /* stored size (never resized) */
    uint16_t orig_width, orig_height;    /* size in the Unity asset (== stored) */
    uint32_t flags;                      /* DQIV_TFL_* */
    uint8_t  wrap_mode;                  /* Unity: 0 repeat, 1 clamp */
    uint8_t  filter_mode;                /* Unity: 0 point, 1 bilinear, 2 trilinear */
    uint8_t  aniso;
    uint8_t  orig_format;                /* Unity TextureFormat id of the source (3 RGB24, 4 RGBA32, 13 RGBA4444, ...) */
    uint32_t data_off;                   /* 48 */
    uint32_t data_size;                  /* width*height*bytes_per_pixel, tightly packed rows, row 0 = BOTTOM row */
    uint32_t mip_count;                  /* 1 (only the base level is stored) */
    float    mip_bias;
    uint32_t reserved;
} DqvTexture;

/* ======================================================================= Mesh (DQMS) */
enum { DQIV_MF_NRM = 1, DQIV_MF_COL = 2, DQIV_MF_UV0 = 4, DQIV_MF_UV1 = 8, DQIV_MF_TAN = 16, DQIV_MF_SKIN = 32 };
enum { DQIV_TOPO_TRIS = 0, DQIV_TOPO_STRIP = 1, DQIV_TOPO_QUADS = 2 /* never emitted */, DQIV_TOPO_LINES = 3,
       DQIV_TOPO_LINESTRIP = 4, DQIV_TOPO_POINTS = 5 };

typedef struct DqvMesh {                 /* 96 bytes */
    uint32_t magic, size;
    uint32_t vertex_count;
    uint32_t vertex_stride;              /* bytes; multiple of 4 */
    uint32_t index_count;                /* total u16 indices over all submeshes */
    uint32_t submesh_count;
    uint32_t bone_count;                 /* number of bind poses */
    uint32_t flags;                      /* DQIV_MF_* */
    int16_t  off_pos, off_nrm, off_col, off_uv0, off_uv1, off_tan;   /* byte offset inside a vertex, -1 if absent */
    uint32_t reserved0;
    uint32_t vertex_off;                 /* vertex_count * vertex_stride bytes, 16-aligned */
    uint32_t index_off;                  /* index_count * u16, 16-aligned */
    uint32_t submesh_off;                /* DqvSubmesh[submesh_count] */
    uint32_t bindpose_off;               /* float[16] * bone_count column-major, 0 if none */
    uint32_t skin_off;                   /* DqvSkinVertex[vertex_count], 0 if none */
    float    aabb_center[3], aabb_extent[3];
    uint32_t reserved1;
} DqvMesh;
/* vertex attribute formats are fixed: pos float3, nrm float3, tan float4, col ubyte4 (R,G,B,A normalized),
   uv0 float2, uv1 float2; fields are laid out in that order (pos,nrm,tan,col,uv0,uv1) without padding. */

typedef struct DqvSubmesh {              /* 48 bytes */
    uint32_t first_index;                /* in u16 elements */
    uint32_t index_count;
    uint32_t topology;                   /* DQIV_TOPO_* */
    uint32_t first_vertex, vertex_count;
    float    aabb_center[3], aabb_extent[3];
    uint32_t reserved;
} DqvSubmesh;

typedef struct DqvSkinVertex {           /* 20 bytes */
    uint8_t bone[4];                     /* indices into the bind pose array */
    float   weight[4];
} DqvSkinVertex;

/* ======================================================================= Material (DQMT) */
typedef struct DqvMaterial {             /* 64 bytes */
    uint32_t magic, size;
    uint32_t shader_id;                  /* gid of a Shader object, 0 if the material uses a Unity built-in shader */
    int32_t  render_queue;               /* m_CustomRenderQueue, -1 == use shader's */
    uint32_t shader_name_off;            /* declared name ("Custom/Chara") or "builtin:<file>:<pathid>" */
    uint32_t tex_count, tex_off;         /* DqvTexEnv[] */
    uint32_t float_count, float_off;     /* DqvFloatProp[] */
    uint32_t color_count, color_off;     /* DqvColorProp[] */
    uint32_t kw_count, kw_off;           /* uint32 string offsets (shader keywords) */
    uint32_t main_tex;                   /* gid of _MainTex (0 if none) */
    uint32_t reserved[2];
} DqvMaterial;
typedef struct DqvTexEnv { uint32_t name_off; uint32_t tex_id; float scale[2]; float offset[2]; } DqvTexEnv;   /* 24 */
typedef struct DqvFloatProp { uint32_t name_off; float value; } DqvFloatProp;                                 /* 8 */
typedef struct DqvColorProp { uint32_t name_off; float rgba[4]; } DqvColorProp;                               /* 20 */

/* ======================================================================= Shader (DQSH) */
typedef struct DqvShader {               /* 48 bytes */
    uint32_t magic, size;
    uint32_t name_off;                   /* declared ShaderLab name */
    int32_t  render_queue;               /* from SubShader Tags Queue (+offset), -1 if unspecified */
    uint32_t dump_name_off;              /* file name in pack_shaders/ (raw ShaderLab text), "" if none */
    uint32_t fallback_off;
    uint32_t pass_count, prop_count, gles_count;   /* gles_count = number of GLES SubProgram variants dumped */
    uint32_t passes_off, props_off;
    uint32_t reserved;
} DqvShader;
typedef struct DqvShaderPass {           /* 40 bytes; -1 == "not specified" */
    int32_t  zwrite;                     /* 0 off, 1 on */
    int32_t  ztest;                      /* DQIV_CMP_* */
    int32_t  cull;                       /* 0 off, 1 front, 2 back */
    int32_t  blend_src, blend_dst;       /* DQIV_BLEND_*, -1/-1 == blending off */
    int32_t  alpha_test_func;            /* DQIV_CMP_*, -1 == off */
    float    alpha_test_ref;
    int32_t  color_mask;                 /* bit0 R, 1 G, 2 B, 3 A; 15 default */
    float    offset_factor, offset_units;
} DqvShaderPass;
typedef struct DqvShaderProp { uint32_t name_off, type_off, default_off; } DqvShaderProp;   /* 12: strings */
enum { DQIV_CMP_NEVER, DQIV_CMP_LESS, DQIV_CMP_EQUAL, DQIV_CMP_LEQUAL, DQIV_CMP_GREATER, DQIV_CMP_NOTEQUAL, DQIV_CMP_GEQUAL,
       DQIV_CMP_ALWAYS };
enum { DQIV_BLEND_ZERO, DQIV_BLEND_ONE, DQIV_BLEND_SRC_COLOR, DQIV_BLEND_SRC_ALPHA, DQIV_BLEND_DST_COLOR,
       DQIV_BLEND_DST_ALPHA, DQIV_BLEND_ONE_MINUS_SRC_COLOR, DQIV_BLEND_ONE_MINUS_SRC_ALPHA,
       DQIV_BLEND_ONE_MINUS_DST_COLOR, DQIV_BLEND_ONE_MINUS_DST_ALPHA, DQIV_BLEND_SRC_ALPHA_SATURATE };

/* ======================================================================= Prefab / scene (DQPF) */
typedef struct DqvPrefab {               /* 64 bytes */
    uint32_t magic, size;
    uint32_t node_count, comp_count;
    uint32_t nodes_off;                  /* 64 */
    uint32_t comps_off;                  /* 64 + 80*node_count */
    uint32_t root_count;                 /* >1 only for scenes */
    uint32_t flags;                      /* 1 == scene */
    uint32_t reserved[8];
} DqvPrefab;

typedef struct DqvNode {                 /* 80 bytes; nodes are in depth-first pre-order (parent index < index) */
    uint32_t name_off;
    int32_t  parent;                     /* -1 for roots */
    int32_t  first_child;                /* -1 if none */
    int32_t  next_sibling;               /* -1 if none (roots are chained too) */
    float    pos[3];                     /* local */
    float    rot[4];                     /* local quaternion x,y,z,w */
    float    scale[3];                   /* local */
    uint8_t  layer;
    uint8_t  active;                     /* GameObject.activeSelf */
    uint16_t tag;                        /* Unity tag id (5 == MainCamera) */
    uint32_t comp_first;                 /* index into the comp table */
    uint32_t comp_count;
    uint32_t src_path_id;                /* Unity pathID of the GameObject (debug) */
    uint32_t reserved[2];
} DqvNode;

/* component type == Unity class id */
enum { DQIV_C_CAMERA = 20, DQIV_C_MESHRENDERER = 23, DQIV_C_MESHFILTER = 33, DQIV_C_ANIMATION = 111,
       DQIV_C_MONOBEHAVIOUR = 114, DQIV_C_SKINNEDMESHRENDERER = 137 };
typedef struct DqvComp {                 /* 16 bytes */
    uint32_t type;                       /* Unity class id; payload layout below, anything else = DqvPropList */
    uint32_t enabled;                    /* 1/0 (m_Enabled, 1 for classes without one) */
    uint32_t data_off;                   /* payload offset in the blob (4-aligned) */
    uint32_t data_size;
} DqvComp;

/* payloads (followed by their arrays inside data_size) */
typedef struct DqvMeshFilterData { uint32_t mesh; } DqvMeshFilterData;
typedef struct DqvMeshRendererData {
    uint32_t flags;                      /* bit0 cast shadows, bit1 receive shadows */
    int32_t  lightmap_index;             /* 255 == none */
    int32_t  sorting_order;
    uint32_t mat_count;
    /* uint32_t mats[mat_count]; */
} DqvMeshRendererData;
typedef struct DqvSkinnedData {
    uint32_t flags;                      /* bit0 cast, bit1 receive, bit2 update when offscreen */
    uint32_t mesh;
    int32_t  root_bone;                  /* node index or -1 */
    uint32_t mat_count, bone_count;
    float    aabb[6];                    /* center xyz, extent xyz */
    int32_t  quality;
    /* uint32_t mats[mat_count]; int32_t bones[bone_count] (node indices, -1 == outside the prefab) */
} DqvSkinnedData;
typedef struct DqvAnimationData {
    int32_t  wrap_mode;                  /* Unity WrapMode: 0 default, 1 once, 2 loop, 4 pingpong, 8 clamp forever */
    uint32_t play_automatically;
    int32_t  culling_type;
    uint32_t default_clip;               /* gid of AnimClip */
    uint32_t clip_count;
    /* uint32_t clips[clip_count]; */
} DqvAnimationData;
typedef struct DqvCameraData {
    uint32_t clear_flags;                /* Unity: 1 skybox, 2 solid colour, 3 depth only, 4 nothing */
    float    background[4];
    float    viewport[4];                /* x, y, w, h normalized */
    float    near_clip, far_clip, fov, ortho_size;
    uint32_t orthographic;
    float    depth;
    uint32_t culling_mask;
    int32_t  rendering_path;
    uint32_t target_texture;
    uint32_t hdr;
    uint32_t occlusion_culling;
} DqvCameraData;

/* Generic property list: MonoBehaviour (class_name = script class, fields flattened), TextMesh, GUILayer, colliders...
   Nested names are dotted ("m_Color.rgba"), arrays are "name.size" + "name[i]" (at most 64 elements kept). */
enum { DQIV_P_INT, DQIV_P_FLOAT, DQIV_P_BOOL, DQIV_P_STR, DQIV_P_VEC2, DQIV_P_VEC3, DQIV_P_VEC4, DQIV_P_ASSET, DQIV_P_LOCAL };
typedef struct DqvProp {                 /* 24 bytes */
    uint32_t name_off;
    uint32_t type;                       /* DQIV_P_*: INT/BOOL v.i[0]; FLOAT/VECn v.f[0..n-1] (colours/rects are VEC4);
                                            STR v.u[0] = string offset; ASSET v.u[0] = gid (or BUILTIN_BIT|pathID);
                                            LOCAL v.u[0] = node index in this prefab, v.u[1] = class id of the target
                                            component (4 == the node's Transform / GameObject) */
    union { int32_t i[4]; float f[4]; uint32_t u[4]; } v;
} DqvProp;
typedef struct DqvPropListData { uint32_t class_name_off; uint32_t count; /* DqvProp props[count]; */ } DqvPropListData;

/* ======================================================================= Animation clip (DQAN) */
enum { DQIV_CT_POS = 0, DQIV_CT_ROT = 1, DQIV_CT_SCALE = 2, DQIV_CT_FLOAT = 3 };
typedef struct DqvAnim {                 /* 64 bytes */
    uint32_t magic, size;
    float    sample_rate;                /* source sample rate (keys are at this rate in practice) */
    float    duration;                   /* last key time over all curves, seconds */
    int32_t  wrap_mode;                  /* AnimationClip.wrapMode as serialized */
    uint32_t curve_count, curves_off;    /* DqvCurve[] */
    uint32_t event_count, events_off;    /* DqvAnimEvent[] */
    float    bounds_center[3], bounds_extent[3];
    uint32_t reserved;
} DqvAnim;
typedef struct DqvCurve {                /* 32 bytes */
    uint32_t path_off;                   /* "root/child/bone" relative to the Animation's GameObject ("" == itself) */
    uint32_t attr_off;                   /* property name for DQIV_CT_FLOAT, else "" */
    uint32_t type;                       /* DQIV_CT_* */
    uint32_t class_id;                   /* target component class for FLOAT curves */
    uint32_t key_count;
    uint32_t keys_off;                   /* float keys: POS/SCALE {t, v[3], inTan[3], outTan[3]} (10 floats),
                                            ROT {t, q[4], inTan[4], outTan[4]} (13), FLOAT {t, v, in, out} (4) */
    int32_t  pre_infinity, post_infinity;/* Unity WrapMode value (serialized) */
} DqvCurve;
typedef struct DqvAnimEvent { float time; uint32_t func_off, data_off; float float_param; int32_t int_param; uint32_t reserved; } DqvAnimEvent;

/* ======================================================================= TextAsset (DQTA) */
typedef struct DqvText { uint32_t magic, size, data_size, reserved; /* bytes follow at offset 16 */ } DqvText;

/* ======================================================================= Font (DQFN) */
typedef struct DqvFont {                 /* 96 bytes */
    uint32_t magic, size;
    float    font_size, line_spacing, ascent, kerning, pixel_scale;
    int32_t  character_spacing, character_padding, convert_case, ascii_start_offset, default_style, rendering_mode;
    uint32_t texture_id, material_id;    /* gids (0 for dynamic fonts: the atlas is generated at runtime) */
    uint32_t rect_count, rects_off;      /* DqvCharRect[] (empty for dynamic TTF fonts) */
    uint32_t kern_count, kern_off;       /* DqvKernPair[] */
    uint32_t ttf_size, ttf_off;          /* embedded TrueType file, 16-aligned (0 if none) */
    uint32_t font_name_off;
    uint32_t fallback_count;
    uint32_t reserved;
} DqvFont;
typedef struct DqvCharRect {             /* 44 bytes; Unity CharacterInfo */
    uint32_t index;                      /* unicode code point */
    float uv_x, uv_y, uv_w, uv_h;        /* in the atlas, normalized (h is negative when flipped Y) */
    float vert_x, vert_y, vert_w, vert_h;/* glyph quad relative to the pen position, pixels */
    float advance;
    uint32_t flipped;
} DqvCharRect;
typedef struct DqvKernPair { uint16_t first, second; float amount; } DqvKernPair;

/* ======================================================================= reference loader */
typedef struct DqvPack DqvPack;

DqvPack*  dqiv_open(const char* dir);                       /* dir containing index.bin + *.pak; NULL on error */
void      dqiv_close(DqvPack* p);
uint32_t  dqiv_object_count(const DqvPack* p);              /* including null entry 0 */
const DqvObject* dqiv_object(const DqvPack* p, uint32_t id);
uint32_t  dqiv_res_count(const DqvPack* p);
const DqvResEntry* dqiv_res_entry(const DqvPack* p, uint32_t i);
const char* dqiv_str(const DqvPack* p, uint32_t pool_off);
/* Resources.Load: case-insensitive lookup; returns object id or 0. kind_mask = DQIV_KMASK(k)|..., 0 == any. */
uint32_t  dqiv_find(const DqvPack* p, const char* path, uint32_t kind_mask);
/* first resource entry index whose path equals `path` (case-insens.), *count = number of entries with that path */
uint32_t  dqiv_find_range(const DqvPack* p, const char* path, uint32_t* count);
/* Reads the blob into a 16-byte aligned malloc'd buffer (free with dqiv_free). NULL on error / missing object. */
void*     dqiv_load(DqvPack* p, uint32_t id, uint32_t* size_out);
void      dqiv_free(void* blob);
/* Reads `len` bytes at `off` inside object `id` (partial loads, e.g. pixel data only). Returns 0 on success. */
int       dqiv_read_part(DqvPack* p, uint32_t id, uint32_t off, uint32_t len, void* dst);

/* Zero-copy typed views over a loaded blob (NULL if magic/size mismatch). */
const DqvTexture* dqiv_as_texture(const void* blob, uint32_t size);
const DqvMesh*    dqiv_as_mesh(const void* blob, uint32_t size);
const DqvMaterial* dqiv_as_material(const void* blob, uint32_t size);
const DqvShader*  dqiv_as_shader(const void* blob, uint32_t size);
const DqvPrefab*  dqiv_as_prefab(const void* blob, uint32_t size);
const DqvAnim*    dqiv_as_anim(const void* blob, uint32_t size);
const DqvText*    dqiv_as_text(const void* blob, uint32_t size);
const DqvFont*    dqiv_as_font(const void* blob, uint32_t size);
#define DQIV_BLOB_STR(blob, off) ((off) ? (const char*)(blob) + (off) : "")
#define DQIV_BLOB_AT(T, blob, off) ((const T*)((const uint8_t*)(blob) + (off)))

/* Structural validation of a loaded blob (offsets, counts, string/ref/index ranges). Returns 0 if OK, otherwise
   writes a message to err and returns non-zero. */
int       dqiv_validate(const DqvPack* p, uint32_t id, const void* blob, uint32_t size, char* err, size_t errsz);

#ifdef __cplusplus
}
#endif
#endif /* DQIV_PACK_H */
