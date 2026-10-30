#ifdef DQIV_DEV_TOOLS
// engine/selftest.cpp -- engine core self tests (WP1). Run at boot when ux0:data/dqiv/enginetest.on exists (Engine::RunSelfTestsIfRequested), or on the host
// (tools/hosttest). Every check logs "[selftest] PASS/FAIL ..."; the summary line is "SELFTEST: <n> checks, <m> failures". Numbers (node/component counts, bounds,
// texture names, animation states, memory) are logged so a run documents the state of the asset pipeline.
#include "miniunity.hpp"
#include "engine/engine_internal.hpp"
#include "utils/logger.h"
#include "config.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <set>

namespace miniunity {

// NB: no printf-format attributes here and %lu for size_t: the Vita's newlib printf has no %zu (it prints 'zu' and shifts the arguments).
namespace {
int g_checks, g_fails;
void Check(bool ok, const char* fmt, ...) {
  char buf[512]; va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
  g_checks++; if (!ok) g_fails++;
  if (ok) l_info("[selftest] PASS %s", buf); else l_error("[selftest] FAIL %s", buf);
}
void Info(const char* fmt, ...) { char buf[512]; va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap); l_info("[selftest] %s", buf); }
bool Near(float a, float b, float e = 1e-3f) { return std::fabs(a - b) <= e; }
bool NearV(Vector3 a, Vector3 b, float e = 1e-3f) { return Near(a.x, b.x, e) && Near(a.y, b.y, e) && Near(a.z, b.z, e); }

struct Probe : Component {                              // lifecycle probe
  static int awake, start, update, late, destroyed;
  void Awake() override { awake++; } void Start() override { start++; } void Update() override { update++; } void LateUpdate() override { late++; } void OnDestroy() override { destroyed++; }
};
int Probe::awake, Probe::start, Probe::update, Probe::late, Probe::destroyed;
struct CoProbe : Component {
  static int steps;
  bool HasCoStart() const override { return true; }
  Coroutine StartCo() override { steps++; co_yield 0; steps++; co_yield 0; steps++; }
};
int CoProbe::steps;

void Frame() {                                          // one engine frame without time source
  Time::deltaTime = 1.f / 30.f; Time::time += Time::deltaTime;
  Engine::RunUpdate(); Engine::RunLateUpdate(); Engine::EndFrame();
}

Bounds WorldBounds(GameObject* g, bool* any) {          // union of the world-space bounds of all MeshFilter meshes below g (8 corners each)
  std::vector<MeshFilter*> mfs; g->GetComponentsInChildren<MeshFilter>(mfs, true);
  Bounds b; bool first = true;
  for (MeshFilter* mf : mfs) {
    if (!mf->sharedMesh || mf->sharedMesh->vertices.empty()) continue;
    Matrix4x4 l2w = mf->transform()->localToWorldMatrix();
    const Bounds& mb = mf->sharedMesh->bounds;
    for (int i = 0; i < 8; i++) {
      Vector3 c = mb.center + Vector3((i & 1) ? mb.extents.x : -mb.extents.x, (i & 2) ? mb.extents.y : -mb.extents.y, (i & 4) ? mb.extents.z : -mb.extents.z);
      Vector3 w = l2w.MultiplyPoint3x4(c);
      if (first) { b.center = w; b.extents = Vector3(); first = false; } else b.Encapsulate(w);
    }
  }
  if (any) *any = !first;
  return b;
}

int CountNodes(Transform* t) { int n = 1; for (Transform* c : t->children()) n += CountNodes(c); return n; }

struct Snapshot { Resources::Stats s; size_t live; };
Snapshot Snap() { return Snapshot{ Resources::GetStats(), engine::LiveCount() }; }

// ---------------------------------------------------------------------------------------------------------------------------------- math / transform
void TestMath() {
  Quaternion q = Quaternion::Euler(30, 60, 90);
  Vector3 e = q.eulerAngles();
  Quaternion q2 = Quaternion::Euler(e);
  Check(Near(std::fabs(Quaternion::Dot(q, q2)), 1.f, 1e-4f), "Euler(30,60,90) -> eulerAngles (%.2f %.2f %.2f) -> Euler round trip", e.x, e.y, e.z);
  // Unity: Euler = qY*qX*qZ -> a vector is rotated by Z first, then X, then Y.  Euler(0,90,0) * forward = right; Euler(90,0,0)*forward = -up (down)
  Check(NearV(Quaternion::Euler(0, 90, 0) * Vector3::forward, Vector3::right), "Euler(0,90,0)*forward == right");
  Check(NearV(Quaternion::Euler(90, 0, 0) * Vector3::forward, Vector3(0, -1, 0)), "Euler(90,0,0)*forward == down");
  Vector3 v = Quaternion::Euler(90, 90, 0) * Vector3::forward;   // X first: forward -> down, then Y(90): down stays down
  Check(NearV(v, Vector3(0, -1, 0)), "Euler(90,90,0)*forward == down (ZXY order)");
  Check(NearV(Quaternion::Euler(0, 90, 90) * Vector3::right, Quaternion::AngleAxis(90, Vector3::up) * (Quaternion::AngleAxis(90, Vector3::forward) * Vector3::right)), "Euler(0,90,90) == Ry*Rz");
  GameObject* a = new GameObject("a"); GameObject* b = new GameObject("b");
  a->transform->localPosition = Vector3(1, 2, 3); a->transform->localRotation = Quaternion::Euler(0, 90, 0); a->transform->localScale = Vector3(2, 2, 2);
  b->transform->SetParent(a->transform);
  b->transform->localPosition = Vector3(0, 0, 1);
  Check(NearV(b->transform->position(), Vector3(1 + 2, 2, 3)), "child world position = parent + R*S*local (%.2f %.2f %.2f)", b->transform->position().x, b->transform->position().y, b->transform->position().z);
  Vector3 wp = b->transform->position(); Quaternion wr = b->transform->rotation();
  b->transform->SetParent(nullptr);
  Check(NearV(b->transform->position(), wp) && Near(Quaternion::Dot(b->transform->rotation(), wr), 1.f, 1e-4f), "SetParent(null) keeps world TRS");
  b->transform->SetParent(a->transform);
  Check(NearV(b->transform->position(), wp), "SetParent(p) keeps world position");
  b->transform->setPosition(Vector3(10, 0, 0));
  Check(NearV(b->transform->position(), Vector3(10, 0, 0)), "setPosition under a rotated+scaled parent");
  Check(NearV(b->transform->InverseTransformPoint(b->transform->position()), Vector3()), "InverseTransformPoint(position) == 0");
  Matrix4x4 m = b->transform->localToWorldMatrix(), inv = m.inverse(), id = m * inv;
  Check(Near(id(0, 0), 1) && Near(id(1, 1), 1) && Near(id(2, 2), 1) && Near(id(0, 3), 0, 1e-3f), "Matrix4x4::inverse");
  Transform* t = (new GameObject("lk"))->transform;
  t->localPosition = Vector3(0, 0, 0); t->LookAt(Vector3(0, 0, 5));
  Check(NearV(t->forward(), Vector3::forward), "LookAt +Z");
  t->LookAt(Vector3(5, 0, 0));
  Check(NearV(t->forward(), Vector3::right), "LookAt +X");
  Object::DestroyImmediate(a); Object::DestroyImmediate(t->gameObject);
}

// ---------------------------------------------------------------------------------------------------------------------------------- lifecycle
void TestLifecycle() {
  Probe::awake = Probe::start = Probe::update = Probe::late = Probe::destroyed = 0; CoProbe::steps = 0;
  GameObject* p = new GameObject("probe");
  Probe* pr = p->AddComponent<Probe>();
  Check(Probe::awake == 1 && Probe::start == 0, "Awake immediately on AddComponent, Start deferred");
  Frame();
  Check(Probe::start == 1 && Probe::update == 1 && Probe::late == 1, "first frame: Start, Update, LateUpdate (%d %d %d)", Probe::start, Probe::update, Probe::late);
  p->SetActive(false); Frame();
  Check(Probe::update == 1, "inactive GameObject receives no Update");
  p->SetActive(true); Frame();
  Check(Probe::update == 2, "re-activated GameObject updates again");
  pr->enabled = false; Frame();
  Check(Probe::update == 2, "disabled component receives no Update");
  pr->enabled = true;
  GameObject* child = new GameObject("child"); child->transform->SetParent(p->transform);
  Probe* cpr = child->AddComponent<Probe>(); (void)cpr;
  p->SetActive(false);
  Check(!child->activeInHierarchy() && child->activeSelf, "activeInHierarchy follows the parent, activeSelf stays");
  p->SetActiveRecursively(true);
  Check(child->activeInHierarchy(), "SetActiveRecursively(true)");
  std::vector<Transform*> found; (void)found;
  // Destroy is deferred to the end of the frame, components of the dying object still see this frame's Update
  int before = Probe::update;
  Object::Destroy(p);
  Check(engine::IsLive(p) , "Destroy() keeps the object alive until the end of the frame");
  Engine::RunUpdate();
  Check(Probe::update > before, "pending-destroy object still updated this frame");
  Engine::RunLateUpdate(); Engine::EndFrame();
  Check(!engine::IsLive(p) && !engine::IsLive(child), "Destroy() flushed at EndFrame (parent and child)");
  Check(Probe::destroyed == 2, "OnDestroy called for both (%d)", Probe::destroyed);
  // destroying a single component
  GameObject* g = new GameObject("c"); g->AddComponent<MeshFilter>(); g->AddComponent<MeshRenderer>();
  Check(g->renderer() != nullptr, "renderer shortcut");
  Object::Destroy(g->GetComponent<MeshRenderer>()); Frame();
  Check(g->renderer() == nullptr && g->GetComponent<MeshFilter>() != nullptr, "Destroy(component) removes only that component");
  // coroutine Start
  GameObject* cg = new GameObject("co"); cg->AddComponent<CoProbe>();
  Frame(); Frame(); Frame();
  Check(CoProbe::steps == 3, "IEnumerator Start: 1 step per frame (%d)", CoProbe::steps);
  // DontDestroyOnLoad
  GameObject* keep = new GameObject("keep"); Object::DontDestroyOnLoad(keep);
  GameObject* lose = new GameObject("lose");
  engine::DestroyAllExceptDontDestroy();
  Check(engine::IsLive(keep) && !engine::IsLive(lose), "LoadLevel teardown keeps DontDestroyOnLoad roots only");
  Object::DestroyImmediate(keep);
  Check(GameObject::Find("keep") == nullptr, "DestroyImmediate");
}

// ---------------------------------------------------------------------------------------------------------------------------------- colliders
void TestColliders() {
  GameObject* g = new GameObject("box"); BoxCollider* bc = g->AddComponent<BoxCollider>(); bc->size = Vector3(2, 2, 2);
  RaycastHit h; Ray r{ Vector3(0, 5, 0), Vector3(0, -1, 0) };
  bool hit = bc->Raycast(r, &h, 1e9f);
  Check(hit && Near(h.distance, 4.f) && NearV(h.point, Vector3(0, 1, 0)), "BoxCollider ray from above: dist %.3f", h.distance);
  Check(!bc->Raycast(Ray{ Vector3(5, 5, 0), Vector3(0, -1, 0) }, &h, 1e9f), "BoxCollider miss");
  Check(!bc->Raycast(Ray{ Vector3(0, 0, 0), Vector3(0, -1, 0) }, &h, 1e9f), "ray starting inside a box does not hit");
  g->transform->localScale = Vector3(2, 1, 1); g->transform->localPosition = Vector3(10, 0, 0);
  Check(bc->Raycast(Ray{ Vector3(11.5f, 5, 0), Vector3(0, -2, 0) }, &h, 1e9f) && Near(h.distance, 4.f), "BoxCollider under scale+translation, non-unit direction: dist %.3f", h.distance);
  Check(!bc->Raycast(Ray{ Vector3(11.5f, 5, 0), Vector3(0, -2, 0) }, &h, 3.f), "maxDistance honoured");
  GameObject* s = new GameObject("sph"); SphereCollider* sc = s->AddComponent<SphereCollider>(); sc->radius = 1; s->transform->localPosition = Vector3(0, 0, 10);
  Check(sc->Raycast(Ray{ Vector3(0, 0, 0), Vector3(0, 0, 1) }, &h, 1e9f) && Near(h.distance, 9.f), "SphereCollider dist %.3f", h.distance);
  GameObject* m = new GameObject("mesh"); MeshCollider* mc = m->AddComponent<MeshCollider>();
  Mesh* quad = new Mesh(); quad->vertices = { Vector3(-1, 0, -1), Vector3(-1, 0, 1), Vector3(1, 0, 1), Vector3(1, 0, -1) }; quad->SetTriangles(std::vector<int>{ 0, 1, 2, 0, 2, 3 });
  mc->sharedMesh = quad; m->transform->localPosition = Vector3(0, 3, 0);
  Check(mc->Raycast(Ray{ Vector3(0.5f, 10, 0.5f), Vector3(0, -1, 0) }, &h, 1e9f) && Near(h.distance, 7.f), "MeshCollider quad dist %.3f", h.distance);
  Check(!mc->Raycast(Ray{ Vector3(2.5f, 10, 0.5f), Vector3(0, -1, 0) }, &h, 1e9f), "MeshCollider miss");
  Object::DestroyImmediate(g); Object::DestroyImmediate(s); Object::DestroyImmediate(m); Object::DestroyImmediate(quad);
}

// ---------------------------------------------------------------------------------------------------------------------------------- pack
void DescribeRenderers(GameObject* root, const char* tag) {
  std::vector<Renderer*> rs; root->GetComponentsInChildren<Renderer>(rs, true);
  int mats = 0, withTex = 0, sk = 0;
  std::set<std::string> shaders, texes;
  for (Renderer* r : rs) {
    if (dynamic_cast<SkinnedMeshRenderer*>(r)) sk++;
    for (Material* m : r->mats) if (m) { mats++; shaders.insert(m->shader ? m->shader->name : "(null)"); if (m->mainTexture) { withTex++; texes.insert(m->mainTexture->name); } }
  }
  std::string sh, tx; for (auto& s : shaders) sh += s + " "; for (auto& t : texes) { if (tx.size() < 200) tx += t + " "; }
  Info("%s: renderers=%lu (skinned %d) materials=%d withTexture=%d shaders=[%s] textures=[%s]", tag, rs.size(), sk, mats, withTex, sh.c_str(), tx.c_str());
}

void TestResources() {
  Texture* t = Resources::Load<Texture>("boot/sqex");
  Check(t && t->width > 0 && dynamic_cast<Texture2D*>(t)->glId == 0, "Texture 'boot/sqex' %dx%d loaded lazily (no GL yet)", t ? t->width : 0, t ? t->height : 0);
  if (t) { unsigned id = t->GetNativeTextureID(); Check(id != 0 && dynamic_cast<Texture2D*>(t)->glId == id, "GetNativeTextureID uploads on demand (gl=%u)", id); }
  Check(Resources::Load<Texture>("BOOT/SQEX") == t, "case-insensitive + cached");
  TextAsset* ta = Resources::Load<TextAsset>("menu/license_list_a");
  Check(ta && ta->bytes.size() > 1000, "TextAsset menu/license_list_a: %lu bytes", ta ? ta->bytes.size() : 0);
  Font* f = Resources::Load<Font>("menu/gmarugopro-m");
  Check(f && f->chars.size() > 9000 && f->texture && f->material, "Font GMaruGoPro-M: %lu glyphs, texture %s, material %s", f ? f->chars.size() : 0, f && f->texture ? f->texture->name.c_str() : "-", f && f->material ? f->material->name.c_str() : "-");
  if (f) { Font::CharInfo ci; Check(f->GetCharacterInfo(0x3042, ci) && ci.width > 0, "Font glyph U+3042 advance %.1f uv(%.3f,%.3f,%.3f,%.3f)", ci.width, ci.uv.x, ci.uv.y, ci.uv.width, ci.uv.height); }
  Font* fe = Resources::Load<Font>("menu/se-garrir");
  Check(fe && fe->chars.size() == 218, "Font SE-GARRIR: %lu glyphs (expect 218), lineSpacing %.3f ascent %.3f size %.1f", fe ? fe->chars.size() : 0, fe ? fe->lineSpacing : 0.f, fe ? fe->ascent : 0.f, fe ? fe->fontSize : 0.f);
  Material* mat = Resources::Load<Material>("boot/button");
  Check(mat != nullptr, "Material boot/button: shader %s tex %s color(%.2f %.2f %.2f %.2f) queue %d", mat && mat->shader ? mat->shader->name.c_str() : "-", mat && mat->mainTexture ? mat->mainTexture->name.c_str() : "-",
        mat ? mat->color.r : 0, mat ? mat->color.g : 0, mat ? mat->color.b : 0, mat ? mat->color.a : 0, mat ? mat->renderQueue : 0);
  Check(Resources::Load<Texture>("nonexistent/path") == nullptr, "missing asset -> nullptr");
  Check(Resources::Load<Texture>("menu/fotk-ydhopel") != nullptr && Resources::Load<Material>("menu/fotk-ydhopel") != nullptr && Resources::Load<Font>("menu/fotk-ydhopel") != nullptr, "one path, three kinds (Texture/Material/Font) resolved by type");
}

void TestFbx(const char* path) {
  Info("---- fbx %s", path);
  Snapshot s0 = Snap();
  GameObject* proto = Resources::Load<GameObject>(path);
  Check(proto && proto->isAsset, "%s: prefab asset loaded (asset=%d)", path, proto && proto->isAsset);
  if (!proto) return;
  Check(GameObject::Find(proto->name) == nullptr, "prefab asset is not in the scene");
  GameObject* g = dynamic_cast<GameObject*>(Object::Instantiate(proto));
  Check(g && !g->isAsset && g->name == proto->name + "(Clone)", "Instantiate -> scene clone '%s'", g ? g->name.c_str() : "-");
  if (!g) return;
  int nsrc = CountNodes(proto->transform), ndst = CountNodes(g->transform);
  Check(nsrc == ndst, "node count %d == %d", nsrc, ndst);
  DescribeRenderers(g, path);
  std::vector<SkinnedMeshRenderer*> sks; g->GetComponentsInChildren<SkinnedMeshRenderer>(sks, true);
  Animation* an = g->GetComponent<Animation>();
  Info("%s: nodes=%d skinned=%lu animation=%d states=%lu", path, ndst, sks.size(), an != nullptr, an ? an->states.size() : 0);
  if (an) for (auto& kv : an->states) Info("  state '%s' len=%.3f wrap=%d curves=%lu", kv.first.c_str(), kv.second.length(), (int)kv.second.wrapMode, kv.second.clip ? kv.second.clip->curves.size() : 0);
  // bones must point into the clone, not the asset
  bool bonesOk = true;
  for (SkinnedMeshRenderer* sk : sks) for (Transform* b : sk->bones) if (b && b->root() != g->transform) bonesOk = false;
  Check(bonesOk, "skinned bones remapped into the clone");
  if (!sks.empty()) {
    SkinnedMeshRenderer* sk = sks[0];
    sk->Skin();
    float maxd = 0; const Mesh* bind = sk->sharedMesh;
    for (size_t i = 0; i < bind->vertices.size(); i++) { Vector3 d = sk->skinned->vertices[i] - bind->vertices[i]; maxd = std::max(maxd, d.magnitude()); }
    Info("%s: rest-pose skin vs bind mesh: max vertex delta %.4f (mesh bounds extents %.2f %.2f %.2f, %lu verts)", path, maxd, bind->bounds.extents.x, bind->bounds.extents.y, bind->bounds.extents.z, bind->vertices.size());
    Check(sk->gameObject->GetComponent<MeshFilter>() && sk->gameObject->GetComponent<MeshFilter>()->sharedMesh == sk->skinned, "skinned mesh published through a MeshFilter");
  }
  bool any = false; Bounds wb = WorldBounds(g, &any);
  Info("%s: world bounds (bind meshes) center(%.2f %.2f %.2f) extents(%.2f %.2f %.2f) any=%d", path, wb.center.x, wb.center.y, wb.center.z, wb.extents.x, wb.extents.y, wb.extents.z, any);
  // mesh/material instancing: renderer.material clones once, shared otherwise
  std::vector<Renderer*> rs; g->GetComponentsInChildren<Renderer>(rs, true);
  if (!rs.empty() && !rs[0]->mats.empty() && rs[0]->mats[0]) {
    Material* shared = rs[0]->mats[0];
    Material* inst = rs[0]->material();
    Check(inst != shared && inst->instanceOwner == rs[0] && rs[0]->material() == inst, "renderer.material clones the shared material once");
    inst->color = Color(1, 0, 0, 1);
    Check(!(shared->color.r == 1.f && shared->color.g == 0.f && shared->color.b == 0.f) || (shared->color.r == 1.f && shared->color.g == 0.f), "shared pack material untouched by instance edit (%.2f %.2f %.2f)", shared->color.r, shared->color.g, shared->color.b);
    std::vector<Material*> all = rs[0]->materials();
    bool allInst = true; for (Material* m : all) if (m && m->instanceOwner != rs[0]) allInst = false;
    Check(allInst, "renderer.materials clones all (%lu)", all.size());
  }
  // animation playback
  if (an && !an->states.empty()) {
    AnimationState& st = an->states.begin()->second;
    std::string nm = st.name; Info("%s: playing '%s' (len %.3f, wrap %d)", path, nm.c_str(), st.length(), (int)st.wrapMode);
    Info("%s: playAutomatically=%d isPlaying before Play()=%d", path, an->playAutomatically, an->isPlaying());
    Check(an->Play(nm) && an->isPlaying(), "Play('%s') -> isPlaying", nm.c_str());
    {   // does frame 0 of the take equal the pose stored in the prefab nodes? (FBX bakes: yes, when the exporter's time slider was at 0)
      std::vector<Transform*> all; std::vector<Vector3> p0; std::vector<Quaternion> r0;
      std::vector<Transform*> stack{ g->transform };
      while (!stack.empty()) { Transform* t = stack.back(); stack.pop_back(); all.push_back(t); p0.push_back(t->localPosition); r0.push_back(t->localRotation); for (Transform* c : t->children()) stack.push_back(c); }
      an->Play(nm); an->Tick(0.f);
      float dp = 0, dr = 0; size_t changed = 0;
      for (size_t i = 0; i < all.size(); i++) { float a = (all[i]->localPosition - p0[i]).magnitude(), b = 1.f - std::fabs(Quaternion::Dot(all[i]->localRotation, r0[i])); dp = std::max(dp, a); dr = std::max(dr, b); if (a > 1e-4f || b > 1e-5f) changed++; }
      Info("%s: t=0 pose vs prefab node pose: %lu/%lu nodes differ, max |dpos| %.4f, max rot err %.5f", path, changed, all.size(), dp, dr);
      for (size_t i = 0; i < all.size(); i++) { all[i]->localPosition = p0[i]; all[i]->localRotation = r0[i]; }
      an->Stop();
    }
    if (!sks.empty()) {   // pose at t=0 of every take vs the bind mesh (informative: FBX takes do not necessarily start in the bind pose)
      for (auto& kv : an->states) {
        an->Play(kv.first); an->Tick(0.f); sks[0]->Skin();
        float md = 0; for (size_t i = 0; i < sks[0]->sharedMesh->vertices.size(); i++) md = std::max(md, (sks[0]->skinned->vertices[i] - sks[0]->sharedMesh->vertices[i]).magnitude());
        const Bounds &sb = sks[0]->skinned->bounds, &bb = sks[0]->sharedMesh->bounds;
        Info("%s: take '%s' t=0 pose vs bind mesh: max vertex delta %.3f; skinned bounds c(%.2f %.2f %.2f) e(%.2f %.2f %.2f) vs bind c(%.2f %.2f %.2f) e(%.2f %.2f %.2f)", path, kv.first.c_str(), md,
             sb.center.x, sb.center.y, sb.center.z, sb.extents.x, sb.extents.y, sb.extents.z, bb.center.x, bb.center.y, bb.center.z, bb.extents.x, bb.extents.y, bb.extents.z);
      }
      an->Stop(); an->Play(nm); an->Rewind(nm);
    }
    std::vector<Vector3> rest; for (Transform* c : g->transform->children()) rest.push_back(c->localPosition);
    Transform* probe = nullptr; for (const AnimCurve& c : st.clip->curves) if (c.type == 1) { probe = g->transform->Find(c.path); if (probe) break; }
    Quaternion q0 = probe ? probe->localRotation : Quaternion::identity;
    int frames = 0, maxFrames = (int)(st.length() * 30.f) + 90;
    bool moved = false;
    while (an->isPlaying() && frames < maxFrames) { Frame(); frames++; if (probe && Quaternion::Dot(q0, probe->localRotation) < 0.9999f) moved = true; if (frames > 3 && (int)st.wrapMode == 2) break; }
    Check(moved || !probe, "animation moved a bone (probe '%s')", probe ? probe->gameObject->name.c_str() : "-");
    Info("%s: animation ran %d frames, isPlaying=%d time=%.3f", path, frames, an->isPlaying(), st.time);
    int eff = (int)st.wrapMode; if (eff == 0) eff = (int)an->wrapMode; if (eff == 0 && st.clip) eff = (int)st.clip->wrapMode;
    if (eff != 2 && eff != 8) Check(!an->isPlaying() && frames >= (int)(st.length() * 30.f) - 1, "Once clip ends (isPlaying=false) after ~%d frames", (int)(st.length() * 30.f));
    if (!sks.empty()) {
      sks[0]->Skin();
      float md = 0; for (size_t i = 0; i < sks[0]->sharedMesh->vertices.size(); i++) md = std::max(md, (sks[0]->skinned->vertices[i] - sks[0]->sharedMesh->vertices[i]).magnitude());
      Info("%s: skinned mesh vs bind after animation: max delta %.3f", path, md);
    }
    // CrossFade between two states when present
    if (an->states.size() >= 2) {
      auto it = an->states.begin(); std::string a = it->first; ++it; std::string b = it->first;
      an->Play(a); an->CrossFade(b, 0.3f); Frame(); Frame();
      AnimationState &sa = an->states[a], &sb = an->states[b];
      Check(sb.enabled && sb.weight > 0.f && sb.weight < 1.f && sa.enabled && sa.weight < 1.f, "CrossFade mid-way weights a=%.2f b=%.2f", sa.weight, sb.weight);
      for (int i = 0; i < 12; i++) Frame();
      Check(sb.weight >= 0.999f && !sa.enabled, "CrossFade completes (a enabled=%d, b weight %.2f)", sa.enabled, sb.weight);
      an->Stop();
    }
  }
  Object::Destroy(g); Frame();
  Snapshot s1 = Snap();
  Info("%s: after destroy: live %lu -> %lu (go %d->%d, comp %d->%d)", path, s0.live, s1.live, s0.s.gameObjects, s1.s.gameObjects, s0.s.components, s1.s.components);
  Check(s1.s.gameObjects == s0.s.gameObjects && s1.s.components == s0.s.components, "no GameObject/Component leaked by Instantiate+Destroy");
}

void TestFld(const char* path) {
  Info("---- fld %s", path);
  Snapshot s0 = Snap();
  Transform* proto = Resources::Load<Transform>(path);
  Check(proto != nullptr, "%s: Resources.Load<Transform>", path);
  if (!proto) return;
  Transform* inst = dynamic_cast<Transform*>(Object::Instantiate(static_cast<Object*>(proto)));
  Check(inst && inst->parent() == nullptr, "Instantiate(Transform) -> root clone");
  if (!inst) return;
  GameObject* g = inst->gameObject;
  int n = CountNodes(inst);
  std::vector<Renderer*> rs; g->GetComponentsInChildren<Renderer>(rs);
  std::vector<MeshFilter*> mfs; g->GetComponentsInChildren<MeshFilter>(mfs);
  size_t verts = 0, tris = 0; for (MeshFilter* mf : mfs) if (mf->sharedMesh) { verts += mf->sharedMesh->vertices.size(); for (auto& t : mf->sharedMesh->subTriangles) tris += t.size() / 3; }
  Info("%s: nodes=%d children=%d renderers=%lu meshFilters=%lu verts=%lu tris=%lu", path, n, inst->childCount(), rs.size(), mfs.size(), verts, tris);
  DescribeRenderers(g, path);
  bool any = false; Bounds wb = WorldBounds(g, &any);
  Info("%s: world bounds center(%.1f %.1f %.1f) extents(%.1f %.1f %.1f)", path, wb.center.x, wb.center.y, wb.center.z, wb.extents.x, wb.extents.y, wb.extents.z);
  Check(any && n > 1 && !rs.empty(), "%s has geometry", path);
  // all mesh index ranges valid
  bool idxOk = true;
  for (MeshFilter* mf : mfs) if (mf->sharedMesh) for (auto& t : mf->sharedMesh->subTriangles) for (int i : t) if (i < 0 || i >= (int)mf->sharedMesh->vertices.size()) idxOk = false;
  Check(idxOk, "all triangle indices within vertex counts");
  // C# FldObject patterns: GetChild + Instantiate(child, pos, rot) + deactivate children + GetComponentsInChildren skips inactive
  if (inst->childCount() > 1) {
    Transform* child = inst->GetChild(1);
    Quaternion rot = Quaternion::Euler(0, 90, 0);
    Transform* c2 = dynamic_cast<Transform*>(Object::Instantiate(static_cast<Object*>(child), Vector3(5, 6, 7), rot));
    Check(c2 && c2->parent() == nullptr && NearV(c2->position(), Vector3(5, 6, 7)) && Near(Quaternion::Dot(c2->rotation(), rot), 1.f, 1e-4f), "Instantiate(child, pos, rot) places the clone at that world TRS");
    c2->SetParent(inst);
    std::vector<Renderer*> before; g->GetComponentsInChildren<Renderer>(before);
    c2->gameObject->SetActiveRecursively(false);
    std::vector<Renderer*> after; g->GetComponentsInChildren<Renderer>(after);
    Check(after.size() < before.size() || before.empty(), "GetComponentsInChildren skips inactive subtrees (%lu -> %lu)", before.size(), after.size());
    Object::Destroy(c2->gameObject);
  }
  Object::Destroy(g); Frame();
  Snapshot s1 = Snap();
  Check(s1.s.gameObjects == s0.s.gameObjects && s1.s.components == s0.s.components, "%s: no scene object leak (go %d->%d comp %d->%d)", path, s0.s.gameObjects, s1.s.gameObjects, s0.s.components, s1.s.components);
}

void TestScene() {
  Info("---- scene/level0");
  Snapshot s0 = Snap();
  std::vector<GameObject*> roots = Engine::InstantiateScene("scene/level0");
  int nodes = 0; for (GameObject* r : roots) nodes += CountNodes(r->transform);
  Check(roots.size() == 2 && nodes == 10, "scene/level0: %lu roots, %d nodes (expect 2 / 10)", roots.size(), nodes);
  for (GameObject* r : roots) {
    Camera* c = r->GetComponent<Camera>();
    Info("  root '%s' layer=%d tag=%s pos(%.2f %.2f %.2f) camera=%d%s", r->name.c_str(), r->layer, r->tag.c_str(), r->transform->localPosition.x, r->transform->localPosition.y, r->transform->localPosition.z, c != nullptr,
         c ? (c->orthographic ? " ortho" : " persp") : "");
  }
  if (roots.size() == 2) {
    Camera* mc = roots[1]->GetComponent<Camera>();
    Check(roots[0]->tag == "MainCamera" && roots[0]->GetComponent<Camera>() && mc && mc->orthographic && mc->cullingMask == 256 && roots[1]->layer == 8, "level0 cameras decoded (tag, ortho, cullingMask 256, layer 8)");
    SphereCollider* sc = nullptr; for (Transform* c : roots[1]->transform->children()) if ((sc = c->gameObject->GetComponent<SphereCollider>())) break;
    Check(sc && Near(sc->radius, 95.f), "MPad SphereCollider radius %.1f", sc ? sc->radius : 0.f);
  }
  for (GameObject* r : roots) Object::Destroy(r);
  Frame();
  Snapshot s1 = Snap();
  Check(s1.s.gameObjects == s0.s.gameObjects, "scene destroyed");
}

void TestLeaks(const char* fbx, const char* fld) {
  Info("---- leak check across a simulated level change");
  Resources::UnloadUnusedAssets();
  Snapshot base = Snap();
  Resources::LogStats("leak-base");
  for (int round = 0; round < 3; round++) {
    GameObject* a = dynamic_cast<GameObject*>(Object::Instantiate(Resources::Load(fbx)));
    Transform* b = dynamic_cast<Transform*>(Object::Instantiate(static_cast<Object*>(Resources::Load<Transform>(fld))));
    std::vector<Renderer*> rs; b->gameObject->GetComponentsInChildren<Renderer>(rs, true);
    for (Renderer* r : rs) { r->material(); r->materials(); }              // force instancing
    std::vector<MeshFilter*> mfs; b->gameObject->GetComponentsInChildren<MeshFilter>(mfs, true);
    for (MeshFilter* m : mfs) m->mesh();
    for (Renderer* r : rs) for (Material* m : r->mats) if (m && m->mainTexture) m->mainTexture->GetNativeTextureID();   // make textures resident
    Frame();
    if (round == 0) Resources::LogStats("leak-loaded");
    Object::Destroy(a); Object::Destroy(b->gameObject);
    Frame();
    engine::DestroyAllExceptDontDestroy();
    Resources::UnloadUnusedAssets();
    Snapshot now = Snap();
    Resources::LogStats(round == 0 ? "leak-after-1" : round == 1 ? "leak-after-2" : "leak-after-3");
    Check(now.s.gameObjects == base.s.gameObjects && now.s.components == base.s.components && now.s.prefabs == base.s.prefabs && now.s.materials == base.s.materials && now.s.meshes == base.s.meshes &&
          now.s.clips == base.s.clips && now.s.textAssets == base.s.textAssets && now.s.texturesResident <= base.s.texturesResident,
          "round %d: go %d/%d comp %d/%d prefabGO %d/%d mat %d/%d mesh %d/%d clip %d/%d text %d/%d texResident %d/%d (now/base)", round, now.s.gameObjects, base.s.gameObjects, now.s.components, base.s.components,
          now.s.prefabs, base.s.prefabs, now.s.materials, base.s.materials, now.s.meshes, base.s.meshes, now.s.clips, base.s.clips, now.s.textAssets, base.s.textAssets, now.s.texturesResident, base.s.texturesResident);
  }
}
}  // namespace

namespace engine {
int RunSelfTests(const char* fbx, const char* fld, const char* fld2) {
  g_checks = g_fails = 0;
  l_info("[selftest] ===== engine self tests begin =====");
  Resources::LogStats("start");
  TestMath();
  TestLifecycle();
  TestColliders();
  TestResources();
  TestFbx(fbx);
  TestFbx("fbx/c1095");
  TestFld(fld);
  TestFld(fld2);
  TestScene();
  TestLeaks(fbx, fld);
  Resources::LogStats("end");
  l_info("SELFTEST: %d checks, %d failures", g_checks, g_fails);
  return g_fails;
}
}  // namespace engine

std::vector<GameObject*> Engine::InstantiateScene(const char* resPath) {
  std::vector<GameObject*> out;
  GameObject* proto = Resources::Load<GameObject>(resPath);
  if (!proto) return out;
  GameObject* clone = dynamic_cast<GameObject*>(Object::Instantiate(proto));
  if (!clone) return out;
  std::vector<Transform*> kids = clone->transform->children();      // the container is dropped, its children become roots (original order)
  for (Transform* t : kids) { t->SetParent(nullptr, false); out.push_back(t->gameObject); }
  Object::DestroyImmediate(clone);
  return out;
}

bool Engine::RunSelfTestsIfRequested() {
  FILE* f = fopen(DATA_PATH "enginetest.on", "rb");
  if (!f) return false;
  fclose(f);
  engine::RunSelfTests("fbx/c0010", "map/btl/btl_dk/stage/btl_dk2.fld", "map/d/dn/out/dnout2.fld");
  Time::time = 0.f; Time::frameCount = 0; Time::deltaTime = 1.f / 30.f;
  return true;
}

}  // namespace miniunity

#endif  // DQIV_DEV_TOOLS
