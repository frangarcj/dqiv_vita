// engine/animation.cpp -- legacy Unity Animation playback + CPU skinning (WP1).
//
// Clips are keyed curves (position / rotation / scale per transform path; float curves only for material "_Color.rgba"). Each frame, after Update()+coroutines and before LateUpdate(), every active
// Animation advances its enabled states, then writes the weight-normalised pose into the local TRS of the target transforms (linear / slerp between keys,
// hold before the first and after the last key = the shipped pre/post infinity of 2). Skinning runs at the end of LateUpdate.
#include "miniunity.hpp"
#include "engine/engine_internal.hpp"
#include "utils/logger.h"

#include <algorithm>
#include <cmath>

namespace miniunity {

namespace {
std::vector<Animation*> g_anims;
std::vector<SkinnedMeshRenderer*> g_skins;

// ---- curve sampling
int FindKey(const AnimCurve& c, float t) {            // last key with time <= t (binary search); keys sorted
  int lo = 0, hi = c.keyCount() - 1;
  while (lo < hi) { int mid = (lo + hi + 1) >> 1; if (c.keys[(size_t)mid * c.stride] <= t) lo = mid; else hi = mid - 1; }
  return lo;
}
void SampleVec(const AnimCurve& c, float t, Vector3& out) {
  int n = c.keyCount();
  if (n == 0) return;
  const float* k = c.keys.data();
  int i = FindKey(c, t);
  const float* a = k + (size_t)i * c.stride;
  if (i >= n - 1 || t <= a[0]) { out = Vector3(a[1], a[2], a[3]); return; }
  const float* b = a + c.stride;
  float f = b[0] > a[0] ? (t - a[0]) / (b[0] - a[0]) : 0.f;
  out = Vector3(a[1] + (b[1] - a[1]) * f, a[2] + (b[2] - a[2]) * f, a[3] + (b[3] - a[3]) * f);
}
void SampleQuat(const AnimCurve& c, float t, Quaternion& out) {
  int n = c.keyCount();
  if (n == 0) return;
  const float* k = c.keys.data();
  int i = FindKey(c, t);
  const float* a = k + (size_t)i * c.stride;
  Quaternion qa; qa.x = a[1]; qa.y = a[2]; qa.z = a[3]; qa.w = a[4];
  if (i >= n - 1 || t <= a[0]) { out = qa; return; }
  const float* b = a + c.stride;
  Quaternion qb; qb.x = b[1]; qb.y = b[2]; qb.z = b[3]; qb.w = b[4];
  float f = b[0] > a[0] ? (t - a[0]) / (b[0] - a[0]) : 0.f;
  out = Quaternion::Slerp(qa, qb, f);
}

float SampleFloat(const AnimCurve& c, float t) {        // float property curves: keys {t, v}, linear
  int n = c.keyCount();
  if (n == 0) return 0.f;
  int i = FindKey(c, t);
  const float* a = c.keys.data() + (size_t)i * c.stride;
  if (i >= n - 1 || t <= a[0]) return a[1];
  const float* b = a + c.stride;
  float f = b[0] > a[0] ? (t - a[0]) / (b[0] - a[0]) : 0.f;
  return a[1] + (b[1] - a[1]) * f;
}

struct Acc {
  Vector3 pos, scl; float wp = 0, ws = 0;
  Quaternion rot; float wr = 0;
};

// the pose of a state at its current time
WrapMode Effective(const Animation* a, const AnimationState& s) {
  WrapMode w = s.wrapMode;
  if (w == WrapMode::Default) w = a->wrapMode;
  if (w == WrapMode::Default && s.clip) w = s.clip->wrapMode;
  if (w == WrapMode::Default) w = WrapMode::Once;
  return w;
}
}  // namespace

// ------------------------------------------------------------------------------------------------ Animation
Animation::Animation() { g_anims.push_back(this); }
Animation::Animation(const Animation& o) : Component(o), clip(o.clip), wrapMode(o.wrapMode), playAutomatically(o.playAutomatically), clips(o.clips) {
  g_anims.push_back(this);                              // runtime state (states, queue) is NOT cloned, like Unity
  SyncStates();
}
Animation::~Animation() { g_anims.erase(std::remove(g_anims.begin(), g_anims.end(), this), g_anims.end()); }

void Animation::SyncStates() {
  auto add = [&](AnimationClip* c) {
    if (!c || states.count(c->name)) return;
    AnimationState& s = states[c->name];
    s.name = c->name; s.clip = c; s.wrapMode = c->wrapMode;
  };
  add(clip);
  for (AnimationClip* c : clips) add(c);
}
void Animation::AddClip(AnimationClip* c, const std::string& n) {
  if (!c) return;
  if (std::find(clips.begin(), clips.end(), c) == clips.end()) clips.push_back(c);
  AnimationState& s = states[n];
  s.name = n; s.clip = c; s.wrapMode = c->wrapMode;
}
void Animation::Awake() {
  SyncStates();
  if (playAutomatically && clip) Play();
}
AnimationState& Animation::operator[](const std::string& n) {
  SyncStates();
  auto it = states.find(n);
  if (it != states.end()) return it->second;
  static AnimationState dummy;                            // Unity returns null (NRE in C#); never writes into the animation
  dummy = AnimationState();
  Debug::LogError("Animation[\"%s\"]: no such state on '%s'", n.c_str(), gameObject ? gameObject->name.c_str() : "?");
  return dummy;
}
bool Animation::isPlaying() const {
  for (const auto& kv : states) if (kv.second.enabled) return true;
  return false;
}
bool Animation::IsPlaying(const std::string& n) const { auto it = states.find(n); return it != states.end() && it->second.enabled; }
bool Animation::Play() { return clip ? Play(clip->name) : false; }
bool Animation::Play(const std::string& n) {
  SyncStates();
  auto it = states.find(n);
  if (it == states.end()) { Debug::LogError("Animation.Play: no state '%s' on '%s'", n.c_str(), gameObject ? gameObject->name.c_str() : "?"); return false; }
  queue.clear();
  for (auto& kv : states) if (&kv.second != &it->second && kv.second.enabled) { kv.second.enabled = false; kv.second.time = 0; kv.second.weight = 0; kv.second.fadeRate = 0; }
  AnimationState& s = it->second;
  if (!(s.enabled && s.weight >= 0.999f && s.fadeTarget >= 1.f)) {       // a state that is already playing is not rewound (Unity)
    s.enabled = true; s.time = s.speed >= 0 ? 0.f : s.length();
  }
  s.weight = 1; s.fadeTarget = 1; s.fadeRate = 0;
  return true;
}
void Animation::Stop() { for (auto& kv : states) { kv.second.enabled = false; kv.second.time = 0; kv.second.fadeRate = 0; } queue.clear(); }
void Animation::Stop(const std::string& n) { auto it = states.find(n); if (it != states.end()) { it->second.enabled = false; it->second.time = 0; it->second.fadeRate = 0; } }
void Animation::Rewind() { for (auto& kv : states) kv.second.time = 0; }
void Animation::Rewind(const std::string& n) { auto it = states.find(n); if (it != states.end()) it->second.time = 0; }
void Animation::CrossFade(const std::string& n, float t) {
  SyncStates();
  if (t <= 0.f) { Play(n); return; }
  auto it = states.find(n);
  if (it == states.end()) { Debug::LogError("Animation.CrossFade: no state '%s'", n.c_str()); return; }
  AnimationState& s = it->second;
  queue.clear();
  for (auto& kv : states) {
    AnimationState& o = kv.second;
    if (&o == &s || !o.enabled) continue;
    o.fadeTarget = 0; o.fadeRate = o.weight / t;
  }
  if (!s.enabled) { s.enabled = true; s.time = s.speed >= 0 ? 0.f : s.length(); s.weight = 0; }
  s.fadeTarget = 1; s.fadeRate = (1.f - s.weight) / t;
}
void Animation::PlayQueued(const std::string& n) {
  SyncStates();
  if (!states.count(n)) { Debug::LogError("Animation.PlayQueued: no state '%s'", n.c_str()); return; }
  if (!isPlaying()) Play(n); else queue.push_back(n);
}

void Animation::Tick(float dt) {
  if (!enabled || !gameObject || gameObject->isAsset || !gameObject->activeInHierarchy()) return;
  if (states.empty()) return;
  Transform* root = gameObject->transform;
  struct Eval { AnimationState* s; float t; };
  std::vector<Eval> eval;
  for (auto& kv : states) {
    AnimationState& s = kv.second;
    if (!s.enabled) continue;
    float len = s.length();
    // fade
    if (s.fadeRate > 0) {
      if (s.weight < s.fadeTarget) s.weight = std::min(s.fadeTarget, s.weight + s.fadeRate * dt);
      else s.weight = std::max(s.fadeTarget, s.weight - s.fadeRate * dt);
      if (s.weight == s.fadeTarget) s.fadeRate = 0;
    }
    s.time += dt * s.speed;
    WrapMode w = Effective(this, s);
    float t = s.time;
    bool ended = false;
    if (len > 0) {
      if (w == WrapMode::Loop) { t = std::fmod(t, len); if (t < 0) t += len; s.time = t; }
      else if (w == WrapMode::PingPong) { float m = std::fmod(t, 2 * len); if (m < 0) m += 2 * len; t = m > len ? 2 * len - m : m; }
      else if (w == WrapMode::ClampForever) { t = std::clamp(t, 0.f, len); s.time = t; }
      else if (s.time >= len || s.time < 0) { t = std::clamp(s.time, 0.f, len); ended = true; }          // Once
    } else t = 0;
    eval.push_back(Eval{ &s, t });
    if (ended) { s.enabled = false; s.time = 0; s.fadeRate = 0; }
    else if (s.fadeTarget <= 0.f && s.weight <= 0.f) { s.enabled = false; s.time = 0; s.fadeRate = 0; }      // fully faded out
  }
  if (!eval.empty()) {
    // accumulate the weighted pose per target transform
    std::unordered_map<Transform*, Acc> acc;
    for (const Eval& e : eval) {
      float wt = e.s->weight;
      if (wt <= 0.f || !e.s->clip) continue;
      for (const AnimCurve& c : e.s->clip->curves) {
        if (c.type > 2) continue;
        Transform* tr = c.path.empty() ? root : root->Find(c.path);
        if (!tr) continue;
        Acc& a = acc[tr];
        if (c.type == 1) {
          Quaternion q; SampleQuat(c, e.t, q);
          if (a.wr == 0) a.rot = q; else a.rot = Quaternion::Lerp(a.rot, q, wt / (a.wr + wt));
          a.wr += wt;
        } else {
          Vector3 v; SampleVec(c, e.t, v);
          if (c.type == 0) { a.pos = a.wp == 0 ? v : a.pos + (v - a.pos) * (wt / (a.wp + wt)); a.wp += wt; }
          else { a.scl = a.ws == 0 ? v : a.scl + (v - a.scl) * (wt / (a.ws + wt)); a.ws += wt; }
        }
      }
    }
    // float property curves (WP6, SlimeTouch CG): class 21 = Material, "_Color.r/g/b/a" -> the renderer's material instance of the target object
    for (const Eval& e : eval) {
      if (e.s->weight <= 0.f || !e.s->clip) continue;
      for (const AnimCurve& c : e.s->clip->curves) {
        if (c.type != 3 || c.classId != 21 || c.attr.size() != 8 || c.attr.compare(0, 7, "_Color.") != 0) continue;
        Transform* tr = c.path.empty() ? root : root->Find(c.path);
        Renderer* r = tr ? tr->gameObject->renderer() : nullptr;
        Material* m = r ? r->material() : nullptr;
        if (!m) continue;
        float v = SampleFloat(c, e.t);
        switch (c.attr[7]) { case 'r': m->color.r = v; break; case 'g': m->color.g = v; break; case 'b': m->color.b = v; break; case 'a': m->color.a = v; break; default: break; }
      }
    }
    for (auto& kv : acc) {
      Transform* tr = kv.first; const Acc& a = kv.second;
      if (a.wp > 0) tr->localPosition = a.pos;
      if (a.wr > 0) tr->localRotation = a.rot;
      if (a.ws > 0) tr->localScale = a.scl;
    }
  }
  if (!queue.empty() && !isPlaying()) { std::string n = queue.front(); queue.erase(queue.begin()); Play(n); }
}

namespace engine {
void TickAnimations(float dt) {
  std::vector<Animation*> list = g_anims;                // Animation components can be created/destroyed while ticking (Play() cannot, but be safe)
  for (Animation* a : list) if (std::find(g_anims.begin(), g_anims.end(), a) != g_anims.end()) a->Tick(dt);
}
void SkinAll() {
  std::vector<SkinnedMeshRenderer*> list = g_skins;
  for (SkinnedMeshRenderer* s : list) {
    if (std::find(g_skins.begin(), g_skins.end(), s) == g_skins.end()) continue;
    if (!s->enabled || !s->gameObject || s->gameObject->isAsset || !s->gameObject->activeInHierarchy()) continue;
    s->Skin();
  }
}
}  // namespace engine

// ------------------------------------------------------------------------------------------------ SkinnedMeshRenderer
SkinnedMeshRenderer::SkinnedMeshRenderer() { g_skins.push_back(this); }
SkinnedMeshRenderer::SkinnedMeshRenderer(const SkinnedMeshRenderer& o) : MeshRenderer(o), sharedMesh(o.sharedMesh), bones(o.bones), rootBone(o.rootBone),
  updateWhenOffscreen(o.updateWhenOffscreen), skinned(nullptr) { g_skins.push_back(this); }
SkinnedMeshRenderer::~SkinnedMeshRenderer() { g_skins.erase(std::remove(g_skins.begin(), g_skins.end(), this), g_skins.end()); }

void SkinnedMeshRenderer::RemapReferences(const std::unordered_map<Transform*, Transform*>& m) {
  for (Transform*& b : bones) { auto it = m.find(b); if (it != m.end()) b = it->second; }
  auto it = m.find(rootBone); if (it != m.end()) rootBone = it->second;
}

void SkinnedMeshRenderer::Skin() {
  if (!sharedMesh || sharedMesh->boneWeights.size() != sharedMesh->vertices.size() || sharedMesh->bindposes.empty() || !gameObject) return;
  if (!skinned || !engine::IsLive(skinned)) {
    skinned = new Mesh(*sharedMesh);
    skinned->boneWeights.clear(); skinned->bindposes.clear();
    skinned->engineOwned = true; skinned->packId = 0; skinned->instanceOwner = this; skinned->name = sharedMesh->name + " (Skinned)"; skinned->dynamic = true;
    MeshFilter* mf = gameObject->GetComponent<MeshFilter>();
    if (!mf) mf = gameObject->AddComponent<SkinnedMeshFilter>();
    mf->sharedMesh = skinned; mf->meshIsInstance = true;
  } else {
    MeshFilter* mf = gameObject->GetComponent<MeshFilter>();
    if (mf && mf->sharedMesh != skinned) { mf->sharedMesh = skinned; mf->meshIsInstance = true; }
  }
  const size_t nb = std::min(bones.size(), sharedMesh->bindposes.size());
  std::vector<Matrix4x4> M(sharedMesh->bindposes.size(), Matrix4x4::identity());
  Matrix4x4 w2l = transform()->worldToLocalMatrix();
  for (size_t i = 0; i < nb; i++) if (bones[i]) M[i] = w2l * bones[i]->localToWorldMatrix() * sharedMesh->bindposes[i];
  const std::vector<Vector3>& src = sharedMesh->vertices;
  std::vector<Vector3>& dst = skinned->vertices;
  dst.resize(src.size());
  Vector3 mn(1e30f, 1e30f, 1e30f), mx(-1e30f, -1e30f, -1e30f);
  const size_t nbp = M.size();
  for (size_t i = 0; i < src.size(); i++) {
    const SkinWeight& sw = sharedMesh->boneWeights[i];
    Vector3 acc; float wsum = 0;
    for (int k = 0; k < 4; k++) {
      float w = sw.weight[k];
      if (w <= 0.f || sw.bone[k] >= nbp) continue;
      acc = acc + M[sw.bone[k]].MultiplyPoint3x4(src[i]) * w;
      wsum += w;
    }
    Vector3 v = wsum > 0.f ? acc * (1.f / wsum) : src[i];
    dst[i] = v;
    mn.x = std::min(mn.x, v.x); mn.y = std::min(mn.y, v.y); mn.z = std::min(mn.z, v.z);
    mx.x = std::max(mx.x, v.x); mx.y = std::max(mx.y, v.y); mx.z = std::max(mx.z, v.z);
  }
  if (!src.empty()) { skinned->bounds.center = (mn + mx) * 0.5f; skinned->bounds.extents = (mx - mn) * 0.5f; }
  skinned->SetDirty();
}

}  // namespace miniunity
