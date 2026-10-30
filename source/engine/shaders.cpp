// engine/shaders.cpp -- ShaderDesc table + Shader::Find (WP2). Source of truth: re/arch/04_SHADERS.md §2, cross-checked against the GLES2 programs in
// extract/pack_shaders (which attributes/uniforms each program really reads: vertex colour, _Color, _MainTex_ST) and the ShaderLab state of
// re/arch/shaders/*.shader.txt (Cull default Back, ZWrite default On, ZTest default LEqual; AlphaTest lines are realised by the programs' `discard`).
//
// DQ4 pack: 45 shader objects (docs/TODO_DQ4.md lists them) (DQ5's Sepia, Spotlight, Particle, Volcano, Bssa*, DssaNormal, ImdShade*, Field_iOS do not exist);  the coverage table is printed by `ShaderTableDump()` at start-up in debug builds.
#include "engine/backend.hpp"
#include "utils/logger.h"

#include <cstring>
#include <strings.h>

namespace miniunity { namespace backend {

#define P(cull, zw, zt, bl, at, op) { cull, zw, zt, bl, at, op, 15 }
#define PM(cull, zw, zt, bl, at, op, mask) { cull, zw, zt, bl, at, op, mask }
static const PassDesc kNoPass = { 0, 0, 0, 0, 0, 0, 15 };
#define ONE(p) 1, { p, kNoPass }

// Columns: name, queue, passes, vc (vertex colour in C), tint (_Color in C), st (0 none/1 scale/2 scale+offset), noMVP (clip-space)
//                 cull,zw,ztest,blend,atest,op
static const ShaderDesc k_table[] = {
  // ---- screen planes / fades (clip-space vertices)
  { "BackColor",                  1000, ONE(P(1,0,CMP_ALWAYS,BLEND_NONE,AT_NONE,OP_FLAT)),    1,0,0, 1 },   // SceneCamera clear plane, BackColor gradient
  { "Custom/BackColor",           1000, ONE(P(1,0,CMP_ALWAYS,BLEND_NONE,AT_NONE,OP_FLAT)),    1,0,0, 1 },
  { "Custom/BGClear",             1000, ONE(P(0,0,CMP_ALWAYS,BLEND_NONE,AT_NONE,OP_FLAT)),    0,1,0, 0 },
  { "Custom/BackFade",            1000, ONE(P(1,0,CMP_ALWAYS,BLEND_ALPHA,AT_NONE,OP_FLAT)),   0,1,0, 1 },
  { "Custom/FrontFade",           4000, ONE(P(1,0,CMP_ALWAYS,BLEND_ALPHA,AT_NONE,OP_FLAT)),   0,1,0, 1 },
  { "Custom/BackOrtho",           1100, ONE(P(0,0,CMP_ALWAYS,BLEND_NONE,AT_NONE,OP_MUL)),     0,0,0, 1 },
  // ---- UI
  { "Custom/Picture",             3000, ONE(P(1,0,CMP_LEQUAL,BLEND_ALPHA,AT_NONE,OP_MUL)),    0,1,2, 0 },
  { "Custom/MenuParts",           3000, ONE(P(1,0,CMP_LEQUAL,BLEND_ALPHA,AT_NONE,OP_MUL)),    0,1,0, 0 },
  { "Custom/FontAlpha8",          3000, ONE(P(1,0,CMP_LEQUAL,BLEND_ALPHA,AT_NONE,OP_A8)),     1,1,2, 0 },
  { "Custom/CanvasAlpha8",        3050, ONE(P(1,0,CMP_LEQUAL,BLEND_ALPHA,AT_NONE,OP_A8)),     0,1,2, 0 },
  { "Custom/ButtonGray",          3000, ONE(P(0,0,CMP_LEQUAL,BLEND_ALPHA,AT_NONE,OP_GRAY)),   0,0,2, 0 },
  { "Custom/ButtonReverse",       3000, ONE(P(0,0,CMP_LEQUAL,BLEND_ALPHA,AT_NONE,OP_INVERT)), 0,0,2, 0 },
  { "Custom/MPad",                3000, ONE(P(0,0,CMP_LEQUAL,BLEND_ALPHA,AT_NONE,OP_MUL)),    0,1,2, 0 },
  { "Custom/TestMenu",            3000, ONE(P(1,0,CMP_LEQUAL,BLEND_ALPHA,AT_NONE,OP_FLAT)),   0,1,0, 0 },
  { "Custom/PanchThrough",        3000, ONE(P(0,0,CMP_LEQUAL,BLEND_ALPHA,AT_NONE,OP_MUL)),    0,0,2, 0 },
  { "Custom/PanchThroughColor",   3000, ONE(P(1,0,CMP_LEQUAL,BLEND_ALPHA,AT_NONE,OP_MUL)),    0,1,2, 0 },
  // ---- image effects (clip-space)
  { "Custom/Capture",             3000, ONE(P(0,0,CMP_ALWAYS,BLEND_ALPHA,AT_NONE,OP_RGB_A1)), 1,1,0, 1 },
  { "Custom/CaptureBlur",         1000, ONE(P(0,0,CMP_ALWAYS,BLEND_ADD,AT_NONE,OP_RGB_A1)),   0,1,0, 1 },
  // ---- billboards / sprites
  { "Custom/Chara",               3000, ONE(P(0,1,CMP_LEQUAL,BLEND_ALPHA,AT_GREATER0,OP_MUL)), 0,1,2, 0 },
  { "Custom/CharaEx",             3000, 2, { P(0,1,CMP_LEQUAL,BLEND_NONE,AT_EQUAL1,OP_MUL), P(0,0,CMP_LEQUAL,BLEND_ALPHA,AT_NONE,OP_MUL) }, 0,1,2, 0 },
  { "Custom/CharaSleep",          3000, ONE(P(0,1,CMP_LEQUAL,BLEND_ALPHA,AT_GREATER0,OP_SLEEP)), 0,1,2, 0 },     // DQ4: lying sprite, depth from _sleepMatrix
  { "Custom/CharaSleepEx",        3000, 2, { P(0,1,CMP_LEQUAL,BLEND_NONE,AT_EQUAL1,OP_SLEEP), P(0,0,CMP_LEQUAL,BLEND_ALPHA,AT_GREATER0,OP_SLEEP) }, 0,1,2, 0 },
  { "Custom/CharaShadow",         2999, ONE(P(1,0,CMP_LEQUAL,BLEND_ALPHA,AT_NONE,OP_MUL)),    0,1,0, 0 },
  { "Custom/Riseup",              3850, ONE(P(0,0,CMP_ALWAYS,BLEND_ALPHA,AT_NONE,OP_MUL)),    1,1,2, 0 },
  { "Custom/Effect0",             3300, ONE(P(0,0,CMP_LEQUAL,BLEND_ALPHA,AT_GREATER0,OP_MUL)), 0,1,2, 0 },
  // ---- world map
  { "Custom/Field",               2000, ONE(P(0,1,CMP_LEQUAL,BLEND_NONE,AT_NONE,OP_MUL)),     0,1,0, 0 },
  { "Custom/FieldSymbol",         3000, ONE(P(0,0,CMP_LEQUAL,BLEND_ALPHA,AT_NONE,OP_MUL)),    0,1,0, 0 },
  { "Custom/SmallMapSymbol",      2000, ONE(P(0,1,CMP_LEQUAL,BLEND_NONE,AT_NONE,OP_FLAT)),    0,1,0, 0 },
  // ---- FLD (town/dungeon) models: tex * (vc * _Color), no ST
  { "Custom/fld_normal",          2000, ONE(P(1,1,CMP_LEQUAL,BLEND_NONE,AT_NONE,OP_MUL)),      1,1,0, 0 },
  { "Custom/fld_atest",           2450, ONE(P(1,1,CMP_LEQUAL,BLEND_NONE,AT_GREATER0,OP_MUL)),  1,1,0, 0 },
  { "Custom/fld_trans",           3000, ONE(P(1,0,CMP_LEQUAL,BLEND_ALPHA,AT_NONE,OP_MUL)),     1,1,0, 0 },
  { "Custom/fld_transz",          3000, ONE(P(1,1,CMP_LEQUAL,BLEND_ALPHA,AT_GREATER0,OP_MUL)), 1,1,0, 0 },
  { "Custom/fld_normal_sepia",    2000, ONE(P(1,1,CMP_LEQUAL,BLEND_NONE,AT_NONE,OP_SEPIA_FLD)),     1,1,0, 0 },
  { "Custom/fld_atest_sepia",     2450, ONE(P(1,1,CMP_LEQUAL,BLEND_NONE,AT_GREATER0,OP_SEPIA_FLD)), 1,1,0, 0 },
  { "Custom/fld_trans_sepia",     3000, ONE(P(1,0,CMP_LEQUAL,BLEND_ALPHA,AT_NONE,OP_SEPIA_FLD)),    1,1,0, 0 },
  // ---- DSSA / BSSA (DS 3D-sprite emulation quads): tex * vc * _Color
  { "Custom/DssaChara",           3100, ONE(P(0,1,CMP_LEQUAL,BLEND_ALPHA,AT_GREATER0,OP_MUL)), 1,1,2, 0 },
  { "Custom/DssaMonster0",        3150, ONE(P(0,0,CMP_LEQUAL,BLEND_ALPHA,AT_NONE,OP_MUL)),     1,1,2, 0 },
  { "Custom/DssaMonster1",        3200, ONE(P(0,0,CMP_LEQUAL,BLEND_ALPHA,AT_NONE,OP_MUL)),     1,1,2, 0 },
  { "Custom/DssaCharaTransparent",3850, ONE(P(0,0,CMP_ALWAYS,BLEND_ALPHA,AT_NONE,OP_MUL)),     1,1,2, 0 },
  // ---- prefab materials (cutscene / minigame characters, props)
  { "Custom/ImdChara",            2000, ONE(P(1,1,CMP_LEQUAL,BLEND_ALPHA,AT_NONE,OP_MUL)),     1,1,0, 0 },
  { "Custom/Simple",              2000, ONE(P(1,1,CMP_LEQUAL,BLEND_NONE,AT_NONE,OP_MUL)),      0,0,2, 0 },
  { "Custom/SimpleTransparent",   3000, ONE(P(1,0,CMP_LEQUAL,BLEND_ALPHA,AT_NONE,OP_MUL)),     0,0,2, 0 },
  { "Custom/DebugColl",           2000, ONE(P(0,1,CMP_LEQUAL,BLEND_NONE,AT_NONE,OP_FLAT)),     1,1,0, 0 },
  // ---- built-in
  { "Transparent/Diffuse",        3000, ONE(PM(1,0,CMP_LEQUAL,BLEND_ALPHA,AT_GREATER0,OP_MUL,7)), 0,1,2, 0 },
  { "Transparent/VertexLit",      3000, ONE(PM(1,0,CMP_LEQUAL,BLEND_ALPHA,AT_GREATER0,OP_MUL,7)), 0,1,2, 0 },
};
// Unity's default (unity_builtin_extra:7 "Diffuse"): every FLD material starts with it and FldObject.set_shader replaces it before the first draw.
static const ShaderDesc k_default = { "(default)", 2000, ONE(P(1,1,CMP_LEQUAL,BLEND_NONE,AT_NONE,OP_MUL)), 1,1,2, 0 };

// File-name aliases used by Shader.Find call sites / pack names that differ from the declared name.
static const struct { const char* alias; const char* name; } k_alias[] = {
  { "Alpha-Diffuse", "Transparent/Diffuse" }, { "Alpha-VertexLit", "Transparent/VertexLit" },
  { "Custom/BackColor", "BackColor" }, { "shader1", "BackColor" },
  { "Picture", "Custom/Picture" }, { "MenuParts", "Custom/MenuParts" },
};

const ShaderDesc* FindShader(const char* name) {
  for (const ShaderDesc& d : k_table)
    if (!strcasecmp(d.name, name)) return &d;
  for (const auto& a : k_alias)
    if (!strcasecmp(a.alias, name))
      for (const ShaderDesc& d : k_table) if (!strcasecmp(d.name, a.name)) return &d;
  static const char* seen[64]; static int n;
  for (int i = 0; i < n; i++) if (seen[i] && !strcmp(seen[i], name)) return &k_default;
  if (n < 64) { seen[n++] = strdup(name); l_warn("Shader::Find(%s): unknown shader, using the default (Diffuse-like) state", name); }
  return &k_default;
}
int ShaderId(const ShaderDesc* d) { return d == &k_default ? -1 : (int)(d - k_table); }
int ShaderCount() { return (int)(sizeof(k_table) / sizeof(k_table[0])); }
const ShaderDesc* ShaderAt(int i) { return i >= 0 && i < ShaderCount() ? &k_table[i] : nullptr; }

}}  // namespace miniunity::backend

namespace miniunity {
int Shader::DefaultQueue() const { const backend::ShaderDesc* d = backend::ShaderAt(id); return d ? d->queue : 2000; }
Shader* Shader::Find(const std::string& name) {
  static std::vector<Shader*> cache;
  for (Shader* s : cache) if (s->name == name) return s;
  Shader* s = new Shader();
  s->name = name;
  s->id = backend::ShaderId(backend::FindShader(name.c_str()));
  cache.push_back(s);
  return s;
}
}  // namespace miniunity
