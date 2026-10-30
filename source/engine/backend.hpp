// engine/backend.hpp -- vitaGL backend interface (WP2, private to engine/).
#pragma once
#include "miniunity.hpp"

namespace miniunity { namespace backend {

// Fragment "ops" of 04_SHADERS.md §1. Each is realised with vitaGL fixed function (texture environment COMBINE) unless noted.
enum Op : uint8_t {
  OP_MUL,          // tex * C                     (C = vertexColor and/or _Color, flags below)
  OP_FLAT,         // C                           (no texture)
  OP_A8,           // (C.rgb, C.a * tex.a)        FontAlpha8 / CanvasAlpha8 (A8 textures sample as (1,1,1,a) in vitaGL: GL_ALPHA = U8_R111)
  OP_RGB_A1,       // (tex.rgb * C.rgb, C.a)      Capture, CaptureBlur
  OP_INV_A,        // (C.rgb, (1 - tex.a) * C.a)  Pre-Spotlight
  OP_POST_A,       // (C.rgb, tex.a * C.a)        Post-Spotlight
  OP_INVERT,       // (1 - tex.rgb, tex.a)        ButtonReverse
  OP_GRAY,         // (|tex.rgb| * _Brightness).xxx, tex.a      ButtonGray        (CG program, FFP approximation without it)
  OP_SEPIA_FLD,    // sepia of tex*C                              fld_*_sepia       (CG program, FFP approximation without it)
  OP_LIT,          // ImdShadeChara: per-vertex lighting done on the CPU, then MUL
  OP_SLEEP,        // CharaSleep(Ex): MUL with clip.xy = (mvp*v).xy and clip.zw = (VP * _sleepMatrix * v).zw (CG vertex program dqiv_sleep.v.cg; FFP fallback = MUL)
  OP_COUNT
};
enum { CMP_ALWAYS = 0, CMP_LEQUAL = 1, CMP_LESS = 2 };
enum { BLEND_NONE = 0, BLEND_ALPHA = 1, BLEND_ADD = 2 };
enum { AT_NONE = 0, AT_GREATER0 = 1, AT_EQUAL1 = 2 };

struct PassDesc {
  uint8_t cull;        // 0 off, 1 back
  uint8_t zwrite;      // 0/1
  uint8_t ztest;       // CMP_*
  uint8_t blend;       // BLEND_*
  uint8_t atest;       // AT_*
  uint8_t op;          // Op
  uint8_t colorMask;   // RGBA bits, 15 = all
};
struct ShaderDesc {
  const char* name; int queue;
  uint8_t npass; PassDesc pass[2];
  uint8_t useVertexColor;   // vertex colour is part of C
  uint8_t useTint;          // _Color is part of C
  uint8_t st;               // 0: _MainTex_ST not used, 1: scale only, 2: scale + offset
  uint8_t noMVP;            // clip-space vertices (gl_Position = _glesVertex)
};

struct DrawCall {
  const ShaderDesc* sd; int pass;
  unsigned texture;                    // GL name or 0 (0 == Unity's default white texture)
  Color tint;                          // _Color
  float brightness;                    // _Brightness (ButtonGray)
  Vector2 stScale, stOffset;           // _MainTex_ST
  const Vector3* positions; const Vector2* uvs; const Color32* colors; int vertexCount;
  const int* indices; int indexCount;  // triangles
  const uint16_t* indices16 = nullptr; int maxIndex = -1;   // optional pre-converted u16 indices (cached per mesh revision by the renderer); maxIndex = highest index
  const Mesh* mesh = nullptr; int sub = 0;                  // source mesh/submesh when the draw comes from a MeshFilter (enables the GL buffer-object path for immutable pack meshes)
  Matrix4x4 model;                     // object -> world
  Matrix4x4 sleepMatrix;               // _sleepMatrix (OP_SLEEP only)
};

// Per-frame render counters (reset by platform/perf.cpp when DQIVN_MEMSTATS is on; cheap increments otherwise).
struct FrameStats { int draws = 0, texBinds = 0, progSwitches = 0, indices = 0, camPasses = 0, renderables = 0, blits = 0, clientArrayBytes = 0, cpuColorMul = 0, idxConvert = 0, vboDraws = 0, tintCached = 0;
  int shDraws[64] = {}, shVerts[64] = {}, shMul[64] = {};   // per ShaderDesc index (draws, vertices sent through client arrays, CPU colour multiplies)
  int vboWhy[8] = {};   // why a mesh draw did not use a buffer object: 0 no mesh, 1 not a pack mesh, 2 instance, 3 dynamic, 4 ST, 5 build failed, 6 tint+colours, 7 other
};
FrameStats& Stats();

bool Init(int viewW, int viewH, int viewX0, int viewY0, bool rotated90);
void Shutdown();

// logical surface / render targets
void BeginFrame();                                      // clears the logical surface (colour black + depth)
void BindTarget(RenderTexture* rt);                     // null: the logical surface FBO
void BeginCamera(int x, int y, int w, int h, bool clearColor, bool clearDepth, Color bg, const Matrix4x4& view, const Matrix4x4& proj);
void Draw(const DrawCall& dc);
void EndFrameAndPresent();                              // blit logical surface to the display, vglSwapBuffers
// Copies the colour buffer of the current target into `dst` (screen-sized texture); used by Sepia/Capture style image effects.
void CopyTargetTo(unsigned tex, int w, int h);
// fullscreen pass helper: draws `tex` over the whole target through the shader `sd` (Graphics.Blit)
void BlitFullscreen(unsigned srcTex, const ShaderDesc* sd, Color tint, float brightness);
void SetBlitRect(int x, int y, int w, int h);          // restrict BlitFullscreen to a pixel rect of the target (the camera rect, split layout); w <= 0 = whole target

// textures (Texture2D / RenderTexture glue)
unsigned CreateTexture(int w, int h, int packFormat, const void* pixels);       // DQIV_TF_* formats; pixels may be null
void UpdateTexture(unsigned id, int w, int h, int packFormat, const void* pixels);
void DeleteTexture(unsigned id);
void SetTextureSampler(unsigned id, int filter /*0 point 1 bilinear*/, int wrap /*0 repeat 1 clamp*/);
bool CreateRenderTexture(int w, int h, unsigned* fbo, unsigned* colorTex, unsigned* depthRb);
void DestroyRenderTexture(unsigned fbo, unsigned colorTex, unsigned depthRb);

const ShaderDesc* FindShader(const char* name);         // engine/shaders.cpp; never null (falls back to the default MUL desc)
int ShaderId(const ShaderDesc*);
int ShaderCount();
const ShaderDesc* ShaderAt(int i);

}}  // namespace miniunity::backend
