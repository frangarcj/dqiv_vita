// engine/components.cpp -- Material property blocks, Renderer/MeshFilter instancing getters, Mesh bounds, reference purging (WP1).
// Unity 4 semantics: `renderer.material` clones the shared material ONCE per renderer (the clone knows its owner), `renderer.materials` clones all of them,
// `meshFilter.mesh` clones the shared mesh once; Instantiate() shares assets until accessed through these getters (03_MINI_UNITY §2.1).
// Instances are engine-owned: Resources::UnloadUnusedAssets frees them when the renderer is gone.
#include "miniunity.hpp"
#include "engine/engine_internal.hpp"
#include "utils/logger.h"

#include <algorithm>
#include <cstring>

namespace miniunity {

void (*g_meshGpuFree)(unsigned, const std::vector<unsigned>&) = nullptr;
Mesh::~Mesh() { if ((gpu.vbo || !gpu.ibo.empty()) && g_meshGpuFree) g_meshGpuFree(gpu.vbo, gpu.ibo); }

// ------------------------------------------------------------------------------------------------ Material
Material::Material(Shader* s) : shader(s) { name = s ? s->name : ""; }
Material::Material(const Material& o) : Object(o), shader(o.shader), color(o.color), mainTexture(o.mainTexture), renderQueue(o.renderQueue),
  mainTextureOffset(o.mainTextureOffset), mainTextureScale(o.mainTextureScale), colors(o.colors), floats(o.floats), vectors(o.vectors), textures(o.textures),
  keywords(o.keywords), instanceOwner(nullptr), packId(0) {}

static bool IsMain(const char* p) { return !p || !std::strcmp(p, "_MainTex"); }
void Material::SetTextureOffset(const char* prop, Vector2 v) { if (IsMain(prop)) mainTextureOffset = v; else textures[prop].offset = v; }
void Material::SetTextureScale(const char* prop, Vector2 v) { if (IsMain(prop)) mainTextureScale = v; else textures[prop].scale = v; }
void Material::SetColor(const char* prop, Color c) { if (!prop || !std::strcmp(prop, "_Color")) color = c; else colors[prop] = c; }
Color Material::GetColor(const char* prop) const {
  if (!prop || !std::strcmp(prop, "_Color")) return color;
  auto it = colors.find(prop); return it != colors.end() ? it->second : Color::white;
}
void Material::SetFloat(const char* prop, float f) { floats[prop] = f; }
float Material::GetFloat(const char* prop) const { auto it = floats.find(prop); return it != floats.end() ? it->second : 0.f; }
void Material::SetVector(const char* prop, float x, float y, float z, float w) { vectors[prop] = Color(x, y, z, w); }
Color Material::GetVector(const char* prop) const { auto it = vectors.find(prop); return it != vectors.end() ? it->second : Color(0, 0, 0, 0); }
void Material::SetTexture(const char* prop, Texture* t) { if (IsMain(prop)) mainTexture = t; else textures[prop].tex = t; }
Texture* Material::GetTexture(const char* prop) const {
  if (IsMain(prop)) return mainTexture;
  auto it = textures.find(prop); return it != textures.end() ? it->second.tex : nullptr;
}
bool Material::HasProperty(const char* prop) const {
  if (!prop) return false;
  if (!std::strcmp(prop, "_Color") || !std::strcmp(prop, "_MainTex")) return true;
  return colors.count(prop) || floats.count(prop) || vectors.count(prop) || textures.count(prop);
}

// ------------------------------------------------------------------------------------------------ Renderer
static Material* MakeInstance(Material* src, Renderer* owner) {
  Material* m = new Material(*src);
  m->instanceOwner = owner; m->engineOwned = true;
  m->name = src->name + " (Instance)";
  return m;
}
static void SyncSlot0(Renderer* r) { if (r->mats.empty() && r->sharedMaterial) r->mats.push_back(r->sharedMaterial); }

Material* Renderer::material() {
  SyncSlot0(this);
  if (mats.empty() || !mats[0]) return nullptr;
  if (mats[0]->instanceOwner != this) mats[0] = MakeInstance(mats[0], this);
  sharedMaterial = mats[0]; materialIsInstance = true;
  return mats[0];
}
std::vector<Material*> Renderer::materials() {
  SyncSlot0(this);
  for (Material*& m : mats) if (m && m->instanceOwner != this) m = MakeInstance(m, this);
  if (!mats.empty()) { sharedMaterial = mats[0]; materialIsInstance = true; }
  return mats;
}

// ------------------------------------------------------------------------------------------------ MeshFilter / Mesh
Mesh* MeshFilter::mesh() {
  if (!sharedMesh) { sharedMesh = new Mesh(); sharedMesh->instanceOwner = this; sharedMesh->engineOwned = true; meshIsInstance = true; return sharedMesh; }   // Unity: `.mesh` of an empty filter creates an empty mesh
  if (sharedMesh->instanceOwner != this) {
    Mesh* m = new Mesh(*sharedMesh);
    m->instanceOwner = this; m->engineOwned = true; m->packId = 0; m->name = sharedMesh->name + " Instance";
    sharedMesh = m;
    meshIsInstance = true;
  }
  return sharedMesh;
}

void Mesh::RecalculateBounds() {
  if (vertices.empty()) { bounds = Bounds(); return; }
  Vector3 mn = vertices[0], mx = vertices[0];
  for (const Vector3& v : vertices) {
    mn.x = std::min(mn.x, v.x); mn.y = std::min(mn.y, v.y); mn.z = std::min(mn.z, v.z);
    mx.x = std::max(mx.x, v.x); mx.y = std::max(mx.y, v.y); mx.z = std::max(mx.z, v.z);
  }
  bounds.center = (mn + mx) * 0.5f; bounds.extents = (mx - mn) * 0.5f;
  revision++;
}

// ------------------------------------------------------------------------------------------------ PurgeReferences
namespace engine {
void PurgeReferences(Object* o) {
  if (!o) return;
  Material* om = dynamic_cast<Material*>(o);
  Mesh* omesh = dynamic_cast<Mesh*>(o);
  Texture* otex = dynamic_cast<Texture*>(o);
  AnimationClip* oclip = dynamic_cast<AnimationClip*>(o);
  Font* ofont = dynamic_cast<Font*>(o);
  if (!om && !omesh && !otex && !oclip && !ofont) return;
  ForEachLive([&](Object* x) {
    if (x == o) return;
    if (auto* m = dynamic_cast<Material*>(x)) {
      if (otex) { if (m->mainTexture == otex) m->mainTexture = nullptr; for (auto& kv : m->textures) if (kv.second.tex == otex) kv.second.tex = nullptr; }
    } else if (auto* r = dynamic_cast<Renderer*>(x)) {
      if (om) { for (Material*& mm : r->mats) if (mm == om) mm = nullptr; if (r->sharedMaterial == om) r->sharedMaterial = nullptr; }
      if (auto* sr = dynamic_cast<SkinnedMeshRenderer*>(x)) { if (omesh) { if (sr->sharedMesh == omesh) sr->sharedMesh = nullptr; if (sr->skinned == omesh) sr->skinned = nullptr; } }
    } else if (auto* mf = dynamic_cast<MeshFilter*>(x)) {
      if (omesh && mf->sharedMesh == omesh) mf->sharedMesh = nullptr;
    } else if (auto* mc = dynamic_cast<MeshCollider*>(x)) {
      if (omesh && mc->sharedMesh == omesh) mc->sharedMesh = nullptr;
    } else if (auto* a = dynamic_cast<Animation*>(x)) {
      if (oclip) { if (a->clip == oclip) a->clip = nullptr; a->clips.erase(std::remove(a->clips.begin(), a->clips.end(), oclip), a->clips.end()); for (auto& kv : a->states) if (kv.second.clip == oclip) { kv.second.clip = nullptr; kv.second.enabled = false; } }
    } else if (auto* cam = dynamic_cast<Camera*>(x)) {
      if (otex && cam->targetTexture == (RenderTexture*)otex) cam->targetTexture = nullptr;
    } else if (auto* tm = dynamic_cast<TextMesh*>(x)) {
      if (ofont && tm->font == ofont) tm->font = nullptr;
    } else if (auto* f = dynamic_cast<Font*>(x)) {
      if (om && f->material == om) f->material = nullptr;
      if (otex && f->texture == (Texture2D*)otex) f->texture = nullptr;
    }
  });
}
}  // namespace engine

}  // namespace miniunity
