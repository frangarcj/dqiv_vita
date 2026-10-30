// engine/pack_glue.cpp -- Resources.Load over the converted asset pack (WP1). Pack format: tools/convert/FORMAT.md, loader: source/pack/dqiv_pack.c.
//
// Every pack object kind is supported: Texture (lazy GL residency), Mesh (CPU copy + skin data), Material (property blocks), Shader (-> Shader::Find),
// Prefab (-> inert GameObject asset tree, cloned by Object::Instantiate), AnimClip, TextAsset, Font.
// Loaded objects are shared per gid (a material used by 40 prefabs is one Material) and cached per (path, kind).
// Memory policy (Resources::UnloadUnusedAssets, called by LoadLevel and game/Cleaner): mark & sweep, see the comment above Resources::UnloadUnusedAssets.
#include "miniunity.hpp"
#include "engine/backend.hpp"
#include "engine/engine_internal.hpp"
#include "pack/dqiv_pack.h"
#include "platform/perf.hpp"
#include "utils/logger.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <unordered_map>
#include <unordered_set>
#ifdef __vita__
#include <malloc.h>
#endif

namespace miniunity {

static DqvPack* g_pack;
static std::unordered_map<uint32_t, Object*> g_gidCache;           // gid -> loaded object (shared)
static std::unordered_map<Object*, uint32_t> g_objGid;             // reverse
static std::unordered_map<std::string, Object*> g_pathCache;       // "lower/path\x01<kind>" -> object (nullptr = looked up, nothing there)

static std::string Lower(const std::string& s) { std::string r = s; for (char& c : r) c = (char)tolower((unsigned char)c); return r; }
static const char* PackName(uint32_t id) { const DqvObject* o = dqiv_object(g_pack, id); return o ? dqiv_str(g_pack, o->name_off) : ""; }

static void Cache(uint32_t gid, Object* o) {
  if (!o) return;
  g_gidCache[gid] = o; g_objGid[o] = gid; o->resCached = true;
}

bool Resources::Init(const char* packDir) {
  if (g_pack) return true;
  g_pack = dqiv_open(packDir);
  if (!g_pack) { l_error("Resources: cannot open the asset pack at %s", packDir); return false; }
  l_success("Resources: pack %s: %u objects, %u resource entries", packDir, dqiv_object_count(g_pack) - 1, dqiv_res_count(g_pack));
  return true;
}

// ------------------------------------------------------------------------------------------------ property list helpers (MonoBehaviour / TextMesh / colliders)
namespace {
struct Props {
  const uint8_t* blob; const DqvPropListData* hdr; const DqvProp* p;
  Props(const uint8_t* b, uint32_t off) : blob(b), hdr((const DqvPropListData*)(b + off)), p((const DqvProp*)(b + off + sizeof(DqvPropListData))) {}
  const char* className() const { return DQIV_BLOB_STR(blob, hdr->class_name_off); }
  const DqvProp* find(const char* name) const {
    for (uint32_t i = 0; i < hdr->count; i++) if (!strcmp(DQIV_BLOB_STR(blob, p[i].name_off), name)) return &p[i];
    return nullptr;
  }
  int i(const char* n, int d = 0) const { const DqvProp* q = find(n); return q ? q->v.i[0] : d; }
  float f(const char* n, float d = 0) const { const DqvProp* q = find(n); return q ? q->v.f[0] : d; }
  Vector3 v3(const char* n, Vector3 d = Vector3()) const { const DqvProp* q = find(n); return q ? Vector3(q->v.f[0], q->v.f[1], q->v.f[2]) : d; }
  uint32_t asset(const char* n) const { const DqvProp* q = find(n); return q ? q->v.u[0] : 0; }
  std::string s(const char* n) const { const DqvProp* q = find(n); return q ? DQIV_BLOB_STR(blob, q->v.u[0]) : ""; }
};
}  // namespace

// ------------------------------------------------------------------------------------------------ builtin (Unity default resource) references
static Mesh* BuiltinMesh(uint32_t pathId) {
  static std::unordered_map<uint32_t, Mesh*> cache;
  auto it = cache.find(pathId);
  if (it != cache.end() && engine::IsLive(it->second)) return it->second;
  Mesh* m = new Mesh();
  m->dontDestroyOnLoad = true;                            // pinned: builtin meshes live forever
  auto quad = [&](Vector3 a, Vector3 b, Vector3 c, Vector3 d, std::vector<int>& tri) {
    int base = (int)m->vertices.size();
    m->vertices.push_back(a); m->vertices.push_back(b); m->vertices.push_back(c); m->vertices.push_back(d);
    m->uv.push_back(Vector2(0, 0)); m->uv.push_back(Vector2(1, 0)); m->uv.push_back(Vector2(1, 1)); m->uv.push_back(Vector2(0, 1));
    int t[6] = { base, base + 1, base + 2, base, base + 2, base + 3 }; tri.insert(tri.end(), t, t + 6);
  };
  std::vector<int> tri;
  if (pathId == 10209) {                                  // Plane: 10x10 in XZ, normal +Y
    m->name = "Plane";
    quad(Vector3(-5, 0, -5), Vector3(-5, 0, 5), Vector3(5, 0, 5), Vector3(5, 0, -5), tri);
  } else if (pathId == 10210) {                           // Quad: 1x1 in XY facing -Z
    m->name = "Quad";
    quad(Vector3(-.5f, -.5f, 0), Vector3(.5f, -.5f, 0), Vector3(.5f, .5f, 0), Vector3(-.5f, .5f, 0), tri);
  } else {                                                // Cube (10202) and anything unknown
    m->name = pathId == 10202 ? "Cube" : "Builtin";
    if (pathId != 10202) l_warn("Resources: unknown builtin mesh %u, using a cube", pathId);
    const float h = 0.5f;
    quad(Vector3(-h, -h, -h), Vector3(-h, h, -h), Vector3(h, h, -h), Vector3(h, -h, -h), tri);   // -Z
    quad(Vector3(h, -h, h), Vector3(h, h, h), Vector3(-h, h, h), Vector3(-h, -h, h), tri);       // +Z
    quad(Vector3(-h, -h, h), Vector3(-h, h, h), Vector3(-h, h, -h), Vector3(-h, -h, -h), tri);   // -X
    quad(Vector3(h, -h, -h), Vector3(h, h, -h), Vector3(h, h, h), Vector3(h, -h, h), tri);       // +X
    quad(Vector3(-h, h, -h), Vector3(-h, h, h), Vector3(h, h, h), Vector3(h, h, -h), tri);       // +Y
    quad(Vector3(-h, -h, h), Vector3(-h, -h, -h), Vector3(h, -h, -h), Vector3(h, -h, h), tri);   // -Y
  }
  m->SetTriangles(tri, 0);
  cache[pathId] = m;
  return m;
}

static Object* LoadObject(uint32_t gid);

static Mesh* MeshRef(uint32_t gid) {
  if (!gid) return nullptr;
  if (gid & DQIV_BUILTIN_BIT) return BuiltinMesh(gid & ~DQIV_BUILTIN_BIT);
  return dynamic_cast<Mesh*>(LoadObject(gid));
}
static Material* MaterialRef(uint32_t gid) { return (!gid || (gid & DQIV_BUILTIN_BIT)) ? nullptr : dynamic_cast<Material*>(LoadObject(gid)); }
static Texture* TextureRef(uint32_t gid) { return (!gid || (gid & DQIV_BUILTIN_BIT)) ? nullptr : dynamic_cast<Texture*>(LoadObject(gid)); }

// ------------------------------------------------------------------------------------------------ Texture (lazy)
static int BytesPerPixel(int pf) { return pf == DQIV_TF_A8 ? 1 : (pf == DQIV_TF_RGB565 || pf == DQIV_TF_RGBA4444) ? 2 : 4; }

static Object* LoadTexture(uint32_t id) {
  DqvTexture hdr;
  if (dqiv_read_part(g_pack, id, 0, sizeof(hdr), &hdr) != 0 || hdr.magic != DQIV_MAGIC_TEX) return nullptr;
  Texture2D* t = new Texture2D();
  t->engineOwned = false;                                 // textures are never deleted by a sweep, only evicted
  t->name = PackName(id);
  t->width = hdr.width; t->height = hdr.height; t->packFormat = (int)hdr.format; t->origFormat = hdr.orig_format; t->packId = id;
  t->format = hdr.format == DQIV_TF_A8 ? TextureFormat::Alpha8 : hdr.format == DQIV_TF_RGB565 ? TextureFormat::RGB565 : hdr.format == DQIV_TF_RGBA4444 ? TextureFormat::RGBA4444 : TextureFormat::RGBA32;
  t->wrapMode = hdr.wrap_mode ? TextureWrapMode::Clamp : TextureWrapMode::Repeat;
  t->filterMode = hdr.filter_mode == 0 ? FilterMode::Point : hdr.filter_mode == 1 ? FilterMode::Bilinear : FilterMode::Trilinear;
  return t;
}

bool Texture2D::MakeResident() const {
  if (glId) return true;
  if (!packId || !g_pack || width <= 0 || height <= 0) return false;
  size_t bytes = (size_t)width * height * BytesPerPixel(packFormat);
  void* px = malloc(bytes);
  if (!px) { l_error("Texture '%s': out of memory for %lu bytes", name.c_str(), (unsigned long)bytes); return false; }
  if (dqiv_read_part(g_pack, packId, 48, (uint32_t)bytes, px) != 0) { l_error("Texture '%s': pack read failed", name.c_str()); free(px); return false; }
  Texture2D* self = const_cast<Texture2D*>(this);
  self->glId = backend::CreateTexture(width, height, packFormat, px);
  free(px);
  self->ApplySampler();
  return glId != 0;
}
void Texture2D::Evict() {
  if (!packId || !glId) return;
  backend::DeleteTexture(glId);
  glId = 0;
}
size_t Texture2D::ResidentBytes() const { return glId ? (size_t)width * height * BytesPerPixel(packFormat) : 0; }
unsigned Texture2D::GetNativeTextureID() const {
  if (!glId && packId) MakeResident();
  const_cast<Texture2D*>(this)->lastUse = (uint32_t)Time::frameCount;
  return glId;
}

// ------------------------------------------------------------------------------------------------ TextAsset
static Object* LoadText(uint32_t id) {
  uint32_t size = 0;
  void* blob = dqiv_load(g_pack, id, &size);
  if (!blob) return nullptr;
  const DqvText* t = dqiv_as_text(blob, size);
  TextAsset* ta = nullptr;
  if (t) { ta = new TextAsset(); const uint8_t* p = (const uint8_t*)blob + 16; ta->bytes.assign(p, p + t->data_size); ta->name = PackName(id); ta->engineOwned = true; }
  dqiv_free(blob);
  return ta;
}

// ------------------------------------------------------------------------------------------------ Mesh
static Object* LoadMesh(uint32_t id) {
  uint32_t size = 0;
  void* blob = dqiv_load(g_pack, id, &size);
  if (!blob) return nullptr;
  const DqvMesh* h = dqiv_as_mesh(blob, size);
  if (!h) { dqiv_free(blob); return nullptr; }
  const uint8_t* b = (const uint8_t*)blob;
  Mesh* m = new Mesh();
  m->engineOwned = true; m->name = PackName(id); m->packId = id;
  const uint32_t n = h->vertex_count, stride = h->vertex_stride;
  const uint8_t* vb = b + h->vertex_off;
  m->vertices.resize(n);
  for (uint32_t i = 0; i < n; i++) std::memcpy(&m->vertices[i], vb + i * stride + h->off_pos, 12);
  if (h->off_nrm >= 0) { m->normals.resize(n); for (uint32_t i = 0; i < n; i++) std::memcpy(&m->normals[i], vb + i * stride + h->off_nrm, 12); }
  if (h->off_uv0 >= 0) { m->uv.resize(n); for (uint32_t i = 0; i < n; i++) std::memcpy(&m->uv[i], vb + i * stride + h->off_uv0, 8); }
  if (h->off_col >= 0) { m->colors32.resize(n); for (uint32_t i = 0; i < n; i++) std::memcpy(&m->colors32[i], vb + i * stride + h->off_col, 4); }
  const uint16_t* idx = (const uint16_t*)(b + h->index_off);
  const DqvSubmesh* sm = (const DqvSubmesh*)(b + h->submesh_off);
  m->subTriangles.resize(h->submesh_count);
  for (uint32_t s = 0; s < h->submesh_count; s++) {
    if (sm[s].topology != DQIV_TOPO_TRIS) { l_warn("Mesh '%s' submesh %u: topology %u not supported (skipped)", m->name.c_str(), s, sm[s].topology); continue; }
    m->subTriangles[s].assign(idx + sm[s].first_index, idx + sm[s].first_index + sm[s].index_count);
  }
  m->bounds.center = Vector3(h->aabb_center[0], h->aabb_center[1], h->aabb_center[2]);
  m->bounds.extents = Vector3(h->aabb_extent[0], h->aabb_extent[1], h->aabb_extent[2]);
  if (h->bone_count && h->bindpose_off) {
    m->bindposes.resize(h->bone_count);
    std::memcpy(m->bindposes.data(), b + h->bindpose_off, h->bone_count * sizeof(Matrix4x4));
    if (h->skin_off) {
      m->boneWeights.resize(n);
      const DqvSkinVertex* sv = (const DqvSkinVertex*)(b + h->skin_off);
      for (uint32_t i = 0; i < n; i++) { std::memcpy(m->boneWeights[i].bone, sv[i].bone, 4); std::memcpy(m->boneWeights[i].weight, sv[i].weight, 16); }
    }
  }
  dqiv_free(blob);
  return m;
}

// ------------------------------------------------------------------------------------------------ Shader / Material
static Object* LoadShader(uint32_t id) {
  uint32_t size = 0;
  void* blob = dqiv_load(g_pack, id, &size);
  if (!blob) return nullptr;
  const DqvShader* sh = dqiv_as_shader(blob, size);
  Shader* s = sh ? Shader::Find(DQIV_BLOB_STR(blob, sh->name_off)) : nullptr;
  dqiv_free(blob);
  return s;
}

static Object* LoadMaterial(uint32_t id) {
  uint32_t size = 0;
  void* blob = dqiv_load(g_pack, id, &size);
  if (!blob) return nullptr;
  const DqvMaterial* h = dqiv_as_material(blob, size);
  if (!h) { dqiv_free(blob); return nullptr; }
  // shader: declared name; built-in (FBX importer default, replaced by the C# at runtime) -> "Diffuse"
  const char* sname = DQIV_BLOB_STR(blob, h->shader_name_off);
  Shader* shader = Shader::Find(strncmp(sname, "builtin:", 8) == 0 || !*sname ? "Diffuse" : sname);
  Material* m = new Material(shader);
  m->engineOwned = true; m->name = PackName(id); m->packId = id; m->renderQueue = h->render_queue;
  const DqvTexEnv* te = DQIV_BLOB_AT(DqvTexEnv, blob, h->tex_off);
  for (uint32_t i = 0; i < h->tex_count; i++) {
    const char* pn = DQIV_BLOB_STR(blob, te[i].name_off);
    Texture* tex = TextureRef(te[i].tex_id);
    if (!strcmp(pn, "_MainTex")) { m->mainTexture = tex; m->mainTextureScale = Vector2(te[i].scale[0], te[i].scale[1]); m->mainTextureOffset = Vector2(te[i].offset[0], te[i].offset[1]); }
    else { Material::TexEnv& e = m->textures[pn]; e.tex = tex; e.scale = Vector2(te[i].scale[0], te[i].scale[1]); e.offset = Vector2(te[i].offset[0], te[i].offset[1]); }
  }
  const DqvFloatProp* fp = DQIV_BLOB_AT(DqvFloatProp, blob, h->float_off);
  for (uint32_t i = 0; i < h->float_count; i++) m->floats[DQIV_BLOB_STR(blob, fp[i].name_off)] = fp[i].value;
  const DqvColorProp* cp = DQIV_BLOB_AT(DqvColorProp, blob, h->color_off);
  for (uint32_t i = 0; i < h->color_count; i++) {
    const char* pn = DQIV_BLOB_STR(blob, cp[i].name_off);
    Color c(cp[i].rgba[0], cp[i].rgba[1], cp[i].rgba[2], cp[i].rgba[3]);
    if (!strcmp(pn, "_Color")) m->color = c; else m->colors[pn] = c;
  }
  const uint32_t* kw = DQIV_BLOB_AT(uint32_t, blob, h->kw_off);
  for (uint32_t i = 0; i < h->kw_count; i++) m->keywords.push_back(DQIV_BLOB_STR(blob, kw[i]));
  dqiv_free(blob);
  return m;
}

// ------------------------------------------------------------------------------------------------ Font
static Object* LoadFont(uint32_t id) {
  uint32_t size = 0;
  void* blob = dqiv_load(g_pack, id, &size);
  if (!blob) return nullptr;
  const DqvFont* h = dqiv_as_font(blob, size);
  if (!h) { dqiv_free(blob); return nullptr; }
  Font* f = new Font();
  f->name = PackName(id);
  f->fontSize = h->font_size; f->lineSpacing = h->line_spacing; f->ascent = h->ascent; f->pixelScale = h->pixel_scale > 0 ? h->pixel_scale : 0.1f;
  f->dynamic = h->rect_count == 0;
  f->texture = dynamic_cast<Texture2D*>(TextureRef(h->texture_id));
  f->material = MaterialRef(h->material_id);
  const DqvCharRect* r = DQIV_BLOB_AT(DqvCharRect, blob, h->rects_off);
  f->chars.reserve(h->rect_count);
  for (uint32_t i = 0; i < h->rect_count; i++) {
    Font::CharInfo c;
    c.index = (int)r[i].index; c.uv = Rect(r[i].uv_x, r[i].uv_y, r[i].uv_w, r[i].uv_h); c.vert = Rect(r[i].vert_x, r[i].vert_y, r[i].vert_w, r[i].vert_h);
    c.width = r[i].advance; c.flipped = r[i].flipped != 0;
    f->lookup[c.index] = (int)f->chars.size();
    f->chars.push_back(c);
  }
  dqiv_free(blob);
  return f;
}

bool Font::GetCharacterInfo(uint16_t ch, CharInfo& out) const {
  auto it = lookup.find(ch);
  if (it != lookup.end()) { out = chars[it->second]; return true; }
  if (lookup.empty() && !chars.empty()) for (const CharInfo& c : chars) if (c.index == ch) { out = c; return true; }
  return false;
}

// ------------------------------------------------------------------------------------------------ AnimationClip
static Object* LoadAnim(uint32_t id) {
  uint32_t size = 0;
  void* blob = dqiv_load(g_pack, id, &size);
  if (!blob) return nullptr;
  const DqvAnim* h = dqiv_as_anim(blob, size);
  if (!h) { dqiv_free(blob); return nullptr; }
  AnimationClip* c = new AnimationClip();
  c->engineOwned = true; c->name = PackName(id); c->packId = id;
  c->length = h->duration; c->sampleRate = h->sample_rate; c->wrapMode = (WrapMode)h->wrap_mode;
  const DqvCurve* cv = DQIV_BLOB_AT(DqvCurve, blob, h->curves_off);
  c->curves.resize(h->curve_count);
  for (uint32_t i = 0; i < h->curve_count; i++) {
    AnimCurve& o = c->curves[i];
    o.path = DQIV_BLOB_STR(blob, cv[i].path_off);
    while (!o.path.empty() && o.path.back() == '/') o.path.pop_back();
    o.attr = DQIV_BLOB_STR(blob, cv[i].attr_off); o.type = (int)cv[i].type; o.classId = (int)cv[i].class_id;
    const float* k = DQIV_BLOB_AT(float, blob, cv[i].keys_off);
    int srcStride = cv[i].type == DQIV_CT_ROT ? 13 : cv[i].type == DQIV_CT_FLOAT ? 4 : 10;
    o.stride = cv[i].type == DQIV_CT_ROT ? 5 : cv[i].type == DQIV_CT_FLOAT ? 2 : 4;
    o.keys.reserve((size_t)cv[i].key_count * o.stride);
    for (uint32_t j = 0; j < cv[i].key_count; j++) for (int q = 0; q < o.stride; q++) o.keys.push_back(k[(size_t)j * srcStride + q]);   // t + value (tangents dropped)
  }
  dqiv_free(blob);
  return c;
}

// ------------------------------------------------------------------------------------------------ Prefab -> asset GameObject tree
namespace {
struct AssetBuildScope { bool saved; AssetBuildScope() : saved(engine::g_assetBuild) { engine::g_assetBuild = true; } ~AssetBuildScope() { engine::g_assetBuild = saved; } };
}
static std::unordered_map<std::string, Component* (*)()>& ScriptFactories() { static auto* m = new std::unordered_map<std::string, Component* (*)()>(); return *m; }
void RegisterScriptFactory(const char* className, Component* (*factory)()) { ScriptFactories()[className] = factory; }

static Object* LoadPrefab(uint32_t id) {
  uint32_t size = 0;
  void* blob = dqiv_load(g_pack, id, &size);
  if (!blob) return nullptr;
  const DqvPrefab* h = dqiv_as_prefab(blob, size);
  if (!h) { dqiv_free(blob); return nullptr; }
  const uint8_t* b = (const uint8_t*)blob;
  const DqvNode* nodes = (const DqvNode*)(b + h->nodes_off);
  const DqvComp* comps = (const DqvComp*)(b + h->comps_off);
  AssetBuildScope scope;
  std::vector<GameObject*> gos(h->node_count, nullptr);
  GameObject* container = nullptr;
  if (h->root_count > 1 || (h->flags & 1)) {              // scene: several roots under a container named like the scene
    container = new GameObject(PackName(id));
  }
  for (uint32_t i = 0; i < h->node_count; i++) {
    const DqvNode& n = nodes[i];
    GameObject* g = new GameObject(DQIV_BLOB_STR(b, n.name_off));
    gos[i] = g;
    g->layer = n.layer; g->activeSelf = n.active != 0; g->tag = n.tag == 5 ? "MainCamera" : "Untagged";
    Transform* t = g->transform;
    t->localPosition = Vector3(n.pos[0], n.pos[1], n.pos[2]);
    t->localRotation.x = n.rot[0]; t->localRotation.y = n.rot[1]; t->localRotation.z = n.rot[2]; t->localRotation.w = n.rot[3];
    t->localScale = Vector3(n.scale[0], n.scale[1], n.scale[2]);
    if (n.parent >= 0) t->SetParent(gos[n.parent]->transform, false);
    else if (container) t->SetParent(container->transform, false);
  }
  struct SkinFix { SkinnedMeshRenderer* smr; std::vector<int> bones; int root; };
  std::vector<SkinFix> fixes;
  for (uint32_t i = 0; i < h->node_count; i++) {
    const DqvNode& n = nodes[i];
    GameObject* g = gos[i];
    for (uint32_t ci = 0; ci < n.comp_count; ci++) {
      const DqvComp& dc = comps[n.comp_first + ci];
      const uint8_t* d = b + dc.data_off;
      Component* made = nullptr;
      switch (dc.type) {
        case DQIV_C_MESHFILTER: {
          MeshFilter* mf = g->AddComponent<MeshFilter>();
          mf->sharedMesh = MeshRef(((const DqvMeshFilterData*)d)->mesh);
          made = mf; break;
        }
        case DQIV_C_MESHRENDERER: {
          const DqvMeshRendererData* r = (const DqvMeshRendererData*)d;
          MeshRenderer* mr = g->AddComponent<MeshRenderer>();
          mr->castShadows = r->flags & 1; mr->receiveShadows = (r->flags & 2) != 0;
          const uint32_t* mats = (const uint32_t*)(d + sizeof(DqvMeshRendererData));
          for (uint32_t k = 0; k < r->mat_count; k++) mr->mats.push_back(MaterialRef(mats[k]));
          mr->sharedMaterial = mr->mats.empty() ? nullptr : mr->mats[0];
          made = mr; break;
        }
        case DQIV_C_SKINNEDMESHRENDERER: {
          const DqvSkinnedData* r = (const DqvSkinnedData*)d;
          SkinnedMeshRenderer* sr = g->AddComponent<SkinnedMeshRenderer>();
          sr->castShadows = r->flags & 1; sr->receiveShadows = (r->flags & 2) != 0; sr->updateWhenOffscreen = (r->flags & 4) != 0;
          sr->sharedMesh = MeshRef(r->mesh);
          const uint32_t* mats = (const uint32_t*)(d + sizeof(DqvSkinnedData));
          for (uint32_t k = 0; k < r->mat_count; k++) sr->mats.push_back(MaterialRef(mats[k]));
          sr->sharedMaterial = sr->mats.empty() ? nullptr : sr->mats[0];
          SkinFix fx; fx.smr = sr; fx.root = r->root_bone;
          const int32_t* bn = (const int32_t*)(mats + r->mat_count);
          for (uint32_t k = 0; k < r->bone_count; k++) fx.bones.push_back(bn[k]);
          fixes.push_back(fx);
          made = sr; break;
        }
        case DQIV_C_ANIMATION: {
          const DqvAnimationData* r = (const DqvAnimationData*)d;
          Animation* a = g->AddComponent<Animation>();
          a->wrapMode = (WrapMode)r->wrap_mode; a->playAutomatically = r->play_automatically != 0;
          a->clip = dynamic_cast<AnimationClip*>(LoadObject(r->default_clip));
          const uint32_t* cl = (const uint32_t*)(d + sizeof(DqvAnimationData));
          for (uint32_t k = 0; k < r->clip_count; k++) if (AnimationClip* c = dynamic_cast<AnimationClip*>(LoadObject(cl[k]))) a->clips.push_back(c);
          made = a; break;
        }
        case DQIV_C_CAMERA: {
          const DqvCameraData* r = (const DqvCameraData*)d;
          Camera* c = g->AddComponent<Camera>();
          c->clearFlags = (CameraClearFlags)r->clear_flags; c->backgroundColor = Color(r->background[0], r->background[1], r->background[2], r->background[3]);
          c->rect = Rect(r->viewport[0], r->viewport[1], r->viewport[2], r->viewport[3]);
          c->nearClipPlane = r->near_clip; c->farClipPlane = r->far_clip; c->fieldOfView = r->fov; c->orthographicSize = r->ortho_size;
          c->orthographic = r->orthographic != 0; c->depth = r->depth; c->cullingMask = (int)r->culling_mask;
          made = c; break;
        }
        case 135: {                                       // SphereCollider
          Props p(b, dc.data_off);
          SphereCollider* c = g->AddComponent<SphereCollider>(); c->radius = p.f("m_Radius", 0.5f); c->center = p.v3("m_Center"); made = c; break;
        }
        case 65: {                                        // BoxCollider
          Props p(b, dc.data_off);
          BoxCollider* c = g->AddComponent<BoxCollider>(); c->size = p.v3("m_Size", Vector3::one); c->center = p.v3("m_Center"); c->isTrigger = p.i("m_IsTrigger") != 0; made = c; break;
        }
        case 64: {                                        // MeshCollider
          Props p(b, dc.data_off);
          MeshCollider* c = g->AddComponent<MeshCollider>(); c->sharedMesh = MeshRef(p.asset("m_Mesh")); made = c; break;
        }
        case 102: {                                       // TextMesh
          Props p(b, dc.data_off);
          TextMesh* t = g->AddComponent<TextMesh>();
          t->text = p.s("m_Text"); t->characterSize = p.f("m_CharacterSize", 1); t->lineSpacing = p.f("m_LineSpacing", 1); t->anchor = p.i("m_Anchor"); t->alignment = p.i("m_Alignment");
          t->fontSize = p.i("m_FontSize"); t->fontStyle = p.i("m_FontStyle"); t->richText = p.i("m_RichText", 1) != 0; t->offsetZ = p.f("m_OffsetZ"); t->tabSize = p.f("m_TabSize", 4);
          t->font = dynamic_cast<Font*>(LoadObject(p.asset("m_Font")));
          uint32_t rgba = (uint32_t)p.i("m_Color.rgba", -1);
          t->color = Color((rgba & 255) / 255.f, ((rgba >> 8) & 255) / 255.f, ((rgba >> 16) & 255) / 255.f, ((rgba >> 24) & 255) / 255.f);
          made = t; break;
        }
        case DQIV_C_MONOBEHAVIOUR: {
          Props p(b, dc.data_off);
          auto it = ScriptFactories().find(p.className());
          if (it == ScriptFactories().end()) { l_debug("Prefab '%s': script '%s' not registered (skipped)", PackName(id), p.className()); break; }
          Component* sc = it->second();
          g->AttachComponent(sc);                          // asset mode: no Awake
          made = sc; break;
        }
        default: break;                                    // GUILayer, AudioListener ... not needed
      }
      if (made) made->enabled = dc.enabled != 0;
    }
  }
  for (SkinFix& fx : fixes) {
    for (int bi : fx.bones) fx.smr->bones.push_back(bi >= 0 && bi < (int)gos.size() ? gos[bi]->transform : nullptr);
    fx.smr->rootBone = fx.root >= 0 && fx.root < (int)gos.size() ? gos[fx.root]->transform : nullptr;
  }
  GameObject* root = container ? container : gos[0];
  dqiv_free(blob);
  return root;
}

// ------------------------------------------------------------------------------------------------ Resources
static Object* LoadObject(uint32_t gid) {
  if (!gid || (gid & DQIV_BUILTIN_BIT) || !g_pack) return nullptr;
  auto it = g_gidCache.find(gid);
  if (it != g_gidCache.end() && engine::IsLive(it->second)) return it->second;
  const DqvObject* o = dqiv_object(g_pack, gid);
  if (!o || !o->size) return nullptr;
  Object* r = nullptr;
  switch (o->kind) {
    case DQIV_K_TEXTURE:  r = LoadTexture(gid); break;
    case DQIV_K_MESH:     r = LoadMesh(gid); break;
    case DQIV_K_MATERIAL: r = LoadMaterial(gid); break;
    case DQIV_K_SHADER:   r = LoadShader(gid); break;
    case DQIV_K_PREFAB:   r = LoadPrefab(gid); break;
    case DQIV_K_ANIM:     r = LoadAnim(gid); break;
    case DQIV_K_TEXT:     r = LoadText(gid); break;
    case DQIV_K_FONT:     r = LoadFont(gid); break;
    default: break;
  }
  if (r && o->kind != DQIV_K_SHADER) Cache(gid, r);       // shaders are owned by Shader::Find's table
  return r;
}
Object* Resources::LoadByGid(uint32_t gid) { return LoadObject(gid); }

static const int kAnyOrder[] = { DQIV_K_PREFAB, DQIV_K_TEXTURE, DQIV_K_TEXT, DQIV_K_FONT, DQIV_K_MATERIAL, DQIV_K_MESH, DQIV_K_ANIM, DQIV_K_SHADER };
static int KindOf(ResKind k) {
  switch (k) {
    case ResKind::Texture: return DQIV_K_TEXTURE; case ResKind::Mesh: return DQIV_K_MESH; case ResKind::Material: return DQIV_K_MATERIAL;
    case ResKind::Shader: return DQIV_K_SHADER; case ResKind::Prefab: return DQIV_K_PREFAB; case ResKind::Anim: return DQIV_K_ANIM;
    case ResKind::Text: return DQIV_K_TEXT; case ResKind::Font: return DQIV_K_FONT; default: return 0;
  }
}

Object* Resources::Load(const std::string& path, ResKind kind) {
  if (!g_pack) return nullptr;
  std::string key = Lower(path);
  key += '\x01'; key += (char)('0' + (int)kind);
  auto it = g_pathCache.find(key);
  if (it != g_pathCache.end() && (!it->second || engine::IsLive(it->second))) return it->second;
  uint32_t cnt = 0, first = dqiv_find_range(g_pack, Lower(path).c_str(), &cnt);
  Object* result = nullptr;
  if (cnt) {
    uint32_t want = (uint32_t)KindOf(kind);
    if (want) {
      for (uint32_t i = 0; i < cnt && !result; i++) { const DqvResEntry* e = dqiv_res_entry(g_pack, first + i); if (e->kind == want) result = LoadObject(e->obj_id); }
    } else {
      for (int k : kAnyOrder) {
        for (uint32_t i = 0; i < cnt && !result; i++) { const DqvResEntry* e = dqiv_res_entry(g_pack, first + i); if ((int)e->kind == k) result = LoadObject(e->obj_id); }
        if (result) break;
      }
    }
  }
  if (!result) l_warn("Resources.Load(%s): %s", path.c_str(), cnt ? "no such asset of the requested type / load failed" : "not found");
  g_pathCache[key] = result;
  return result;
}

namespace engine {
void ResourcesOnObjectDeleted(Object* o) {
  auto git = g_objGid.find(o);
  if (git != g_objGid.end()) {
    auto cit = g_gidCache.find(git->second);
    if (cit != g_gidCache.end() && cit->second == o) g_gidCache.erase(cit);
    g_objGid.erase(git);
  }
  for (auto it = g_pathCache.begin(); it != g_pathCache.end();) { if (it->second == o) it = g_pathCache.erase(it); else ++it; }
}
}  // namespace engine

void Resources::UnloadAsset(Object* o) {
  if (!o) return;
  if (dynamic_cast<Shader*>(o)) return;                  // shaders belong to Shader::Find's table
  Object::DestroyImmediate(o);                           // clears references (PurgeReferences), cache entries go through ~Object
}

// ---- mark & sweep ------------------------------------------------------------------------------------------------------------------
namespace {
struct Marker {
  std::unordered_set<Object*> marked;
  void Mat(Material* m) {
    if (!m || !marked.insert(m).second) return;
    Tex(m->mainTexture);
    for (auto& kv : m->textures) Tex(kv.second.tex);
  }
  void Tex(Texture* t) { if (t) marked.insert(t); }
  void Mesh_(Mesh* m) { if (m) marked.insert(m); }
  void Clip(AnimationClip* c) { if (c) marked.insert(c); }
  void FontObj(Font* f) { if (!f || !engine::IsLive(f) || !marked.insert(f).second) return; if (f->material && engine::IsLive(f->material)) Mat(f->material); Tex(f->texture); }
  void Comp(Component* c) {
    if (auto* r = dynamic_cast<Renderer*>(c)) {
      if (r->sharedMaterial && !engine::IsLive(r->sharedMaterial)) l_error("STALE sharedMaterial %p in '%s' (renderer %p)", (void*)r->sharedMaterial, r->gameObject ? r->gameObject->name.c_str() : "?", (void*)r);
      for (Material* m : r->mats) if (m && !engine::IsLive(m)) l_error("STALE mat %p in '%s' (renderer %p)", (void*)m, r->gameObject ? r->gameObject->name.c_str() : "?", (void*)r);
      if (r->sharedMaterial && engine::IsLive(r->sharedMaterial)) Mat(r->sharedMaterial); for (Material* m : r->mats) if (m && engine::IsLive(m)) Mat(m); }
    if (auto* mf = dynamic_cast<MeshFilter*>(c)) Mesh_(mf->sharedMesh);
    if (auto* sr = dynamic_cast<SkinnedMeshRenderer*>(c)) { Mesh_(sr->sharedMesh); Mesh_(sr->skinned); }
    if (auto* a = dynamic_cast<Animation*>(c)) { Clip(a->clip); for (AnimationClip* k : a->clips) Clip(k); for (auto& kv : a->states) Clip(kv.second.clip); }
    if (auto* mc = dynamic_cast<MeshCollider*>(c)) Mesh_(mc->sharedMesh);
    if (auto* cam = dynamic_cast<Camera*>(c)) Tex(cam->targetTexture);
    if (auto* tm = dynamic_cast<TextMesh*>(c)) FontObj(tm->font);
  }
  void GO(GameObject* g) { for (Component* c : g->components) Comp(c); for (Transform* t : g->transform->children()) GO(t->gameObject); }
  void Obj(Object* o) {        // a root object that is kept: mark what it references
    // NB: Mat()/FontObj() do their own marked.insert() and return early when already marked, so they must run before the object is inserted here
    if (auto* f = dynamic_cast<Font*>(o)) { FontObj(f); return; }
    if (auto* m = dynamic_cast<Material*>(o)) { Mat(m); return; }
    marked.insert(o);
    if (auto* g = dynamic_cast<GameObject*>(o)) GO(g);
  }
};
}  // namespace

namespace engine {
void ResourcesSweep() {
  Marker mk;
  // roots 1: the scene
  for (GameObject* g : g_gameObjects) for (Component* c : g->components) mk.Comp(c);
  // roots 2: everything the game created itself or pinned (game-owned assets, DontDestroyOnLoad assets, pinned prefab trees)
  std::vector<Object*> all;
  ForEachLive([&](Object* o) { all.push_back(o); });
  std::vector<GameObject*> assetRoots;
  for (Object* o : all) {
    if (auto* g = dynamic_cast<GameObject*>(o)) { if (g->isAsset && !g->transform->parent()) { if (g->dontDestroyOnLoad) mk.Obj(g); else assetRoots.push_back(g); } continue; }
    if (dynamic_cast<Component*>(o) || dynamic_cast<Shader*>(o)) continue;
    if (auto* f = dynamic_cast<Font*>(o)) { mk.FontObj(f); continue; }          // fonts are never swept, so their material/atlas must not be either (else Font::material dangles -> crash at the next sweep)
    if (auto* t = dynamic_cast<Texture2D*>(o)) { if (t->packId && !t->dontDestroyOnLoad) continue; }      // pack textures are evictable caches, not roots
    if (!o->engineOwned || o->dontDestroyOnLoad) mk.Obj(o);
  }
  int nTex = 0, nMat = 0, nMesh = 0, nClip = 0, nText = 0, nPrefab = 0;
  // sweep 1: prefab asset trees (nobody references them except the caches)
  for (GameObject* g : assetRoots) if (IsLive(g)) { Object::DestroyImmediate(g); nPrefab++; }
  // sweep 2: engine-owned assets that nothing references; unreferenced pack textures lose their GL copy
  for (Object* o : all) {
    if (!IsLive(o) || mk.marked.count(o)) continue;
    if (auto* t = dynamic_cast<Texture2D*>(o)) { if (t->packId && t->glId) { t->Evict(); nTex++; } continue; }
    if (!o->engineOwned || o->dontDestroyOnLoad) continue;
    if (dynamic_cast<Material*>(o)) nMat++;
    else if (dynamic_cast<Mesh*>(o)) nMesh++;
    else if (dynamic_cast<AnimationClip*>(o)) nClip++;
    else if (dynamic_cast<TextAsset*>(o)) nText++;
    else continue;
    delete o;
  }
  perf::LogMemory("sweep");
  l_info("Resources.UnloadUnusedAssets: freed %d prefab trees, %d materials, %d meshes, %d clips, %d text assets; evicted %d textures", nPrefab, nMat, nMesh, nClip, nText, nTex);
}
}  // namespace engine

void Resources::UnloadUnusedAssets() { engine::ResourcesSweep(); }

int Resources::EnforceTextureBudget(size_t maxBytes, uint32_t minAgeFrames) {
  std::vector<Texture2D*> res; size_t total = 0;
  engine::ForEachLive([&](Object* o) { if (auto* t = dynamic_cast<Texture2D*>(o)) if (t->packId && t->glId) { res.push_back(t); total += t->ResidentBytes(); } });
  if (total <= maxBytes) return 0;
  std::sort(res.begin(), res.end(), [](Texture2D* a, Texture2D* b) { return a->lastUse < b->lastUse; });
  uint32_t now = (uint32_t)Time::frameCount; int n = 0;
  for (Texture2D* t : res) {
    if (total <= maxBytes) break;
    if (now - t->lastUse < minAgeFrames) break;                  // sorted by age: everything after is newer
    total -= t->ResidentBytes(); t->Evict(); n++;
  }
  if (n) l_info("texture budget: evicted %d textures, %.1f MB resident (budget %.1f MB)", n, total / 1048576.0, maxBytes / 1048576.0);
  return n;
}

Resources::Stats Resources::GetStats() {
  Stats s;
  engine::ForEachLive([&](Object* o) {
    s.live++;
    if (auto* g = dynamic_cast<GameObject*>(o)) { if (g->isAsset) s.prefabs++; else s.gameObjects++; }
    else if (auto* c = dynamic_cast<Component*>(o)) { if (!c->gameObject || !c->gameObject->isAsset) s.components++; }
    else if (dynamic_cast<Material*>(o)) s.materials++;
    else if (auto* m = dynamic_cast<Mesh*>(o)) { s.meshes++; s.meshBytes += m->vertices.size() * 12 + m->uv.size() * 8 + m->colors32.size() * 4 + m->normals.size() * 12 + m->boneWeights.size() * 20; for (auto& t : m->subTriangles) s.meshBytes += t.size() * 4; }
    else if (auto* t = dynamic_cast<Texture2D*>(o)) { s.textures++; if (t->glId) { s.texturesResident++; s.texBytes += t->ResidentBytes(); } }
    else if (dynamic_cast<TextAsset*>(o)) s.textAssets++;
    else if (dynamic_cast<AnimationClip*>(o)) s.clips++;
  });
  s.cacheEntries = g_gidCache.size();
  return s;
}

void Resources::LogStats(const char* tag) {
  Stats s = GetStats();
  size_t heap = 0;
#ifdef __vita__
  struct mallinfo mi = mallinfo(); heap = (size_t)mi.uordblks;
#endif
  l_info("MEM[%s]: objects=%d go=%d comp=%d prefabGO=%d mat=%d mesh=%d(%.2fMB) tex=%d(resident %d, %.2fMB) text=%d clip=%d cache=%lu heapInUse=%.2fMB", tag, s.live, s.gameObjects, s.components, s.prefabs,
         s.materials, s.meshes, s.meshBytes / 1048576.0, s.textures, s.texturesResident, s.texBytes / 1048576.0, s.textAssets, s.clips, (unsigned long)s.cacheEntries, heap / 1048576.0);
}

}  // namespace miniunity
