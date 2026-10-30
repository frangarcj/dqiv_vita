// engine/engine_core.cpp -- Engine lifecycle (WP1): BeginFrame / RunUpdate / RunLateUpdate / EndFrame / LoadLevel.
// Render/Present/Init live in engine/renderer.cpp + engine/backend_vitagl.cpp (WP2).
#include "miniunity.hpp"
#include "engine/engine_internal.hpp"
#include "utils/logger.h"

#include <algorithm>
#include <string>
#include <psp2/kernel/processmgr.h>

namespace miniunity {

namespace engine {
uint64_t g_lastTick;
void ProcessStartQueue() {
  // Start() runs before the first Update() of a component that is enabled on an active GameObject. A component added during
  // this pass (Awake/Start of another one) is started in the NEXT pass (simplification documented in 03 §2.1).
  std::vector<Component*> q;
  q.swap(g_startQueue);
  std::vector<Component*> keep;
  for (Component* c : q) {
    if (!IsLive(c) || c->destroyed) continue;
    if (c->startCalled) continue;
    if (!c->enabled || !c->gameObject || !c->gameObject->activeInHierarchy()) { keep.push_back(c); continue; }
    c->startCalled = true;
    if (c->HasCoStart()) {
      Coroutine co = c->StartCo();
      co.MoveNext();                                     // first step now; later steps after Update() of each frame
      if (!co.Done()) engine::StartCoroutineOwned(std::move(co), c);
    } else {
      c->Start();
    }
  }
  g_startQueue.insert(g_startQueue.begin(), keep.begin(), keep.end());
}
}  // namespace engine

namespace Engine {

static LevelBuilder g_levelBuilder;
static std::string g_pendingLevel;
static void DoLoadLevel(const char* name);

void BeginFrame() {
  uint64_t now = sceKernelGetProcessTimeWide();
  if (engine::g_lastTick) {
    float dt = (float)(now - engine::g_lastTick) * 1e-6f;
    Time::deltaTime = std::min(dt, 0.1f) * Time::timeScale;          // clamped to 1/10 s (01 §1.3)
  }
  engine::g_lastTick = now;
  Time::time += Time::deltaTime;
}

// Runs f on every component of g that may be updated. Components can be destroyed (DestroyImmediate) or added while f runs: g_delVersion tells us to re-validate.
// Unity semantics: components of objects already marked with Destroy() (pending until the end of the frame) still receive Update/LateUpdate.
template <class F> static void ForEachUpdatable(F f) {
  static std::vector<GameObject*> snapshot;                          // scratch (no per-frame allocation); ForEachUpdatable is never re-entered
  snapshot = engine::g_gameObjects;                                   // Update() may create/destroy objects
  for (GameObject* g : snapshot) {
    if (!engine::IsLive(g) || !g->activeInHierarchy()) continue;
    for (size_t i = 0; i < g->components.size();) {
      Component* c = g->components[i];
      if (!c->enabled || !c->startCalled) { ++i; continue; }
      uint32_t v = engine::g_delVersion;
      f(c);
      if (v != engine::g_delVersion) {                               // something was deleted: re-find the position of c
        if (!engine::IsLive(g)) break;
        auto it = std::find(g->components.begin(), g->components.end(), c);
        if (it == g->components.end()) continue;                      // c itself is gone: index i now holds the next component
        i = (size_t)(it - g->components.begin()) + 1;
      } else ++i;
    }
  }
}

void RunUpdate() {
  engine::ProcessStartQueue();
  ForEachUpdatable([](Component* c) { c->Update(); });
  engine::StepCoroutines(Time::deltaTime);                          // coroutines resume after Update() (Unity 4)
  engine::TickAnimations(Time::deltaTime);                          // legacy Animation is evaluated after scripts, before LateUpdate
}

void RunLateUpdate() {
  ForEachUpdatable([](Component* c) { c->LateUpdate(); });
  engine::SkinAll();                                                // skinned meshes follow the final bone pose of the frame
}

void EndFrame() {
  if (!g_pendingLevel.empty()) { engine::FlushDestroy(); std::string n; n.swap(g_pendingLevel); DoLoadLevel(n.c_str()); }
  engine::FlushDestroy();
  engine::ClearCoroutinesOfDestroyed();
  Time::frameCount++;
}

void SetLevelBuilder(LevelBuilder b) { g_levelBuilder = b; }

static void DoLoadLevel(const char* name) {
  // Destroys every root object not marked DontDestroyOnLoad, then builds the level. The level builders
  // ("dq4sp" == level0, "mainData") are WP1 work: they instantiate pack scenes `scene/level0` / `scene/maindata`
  // (see engine/pack_glue.cpp) and add the MonoBehaviours (MainLoop, TouchPad, Boot) by name.
  l_info("Engine::LoadLevel(%s)", name);
  engine::DestroyAllExceptDontDestroy();
  Resources::UnloadUnusedAssets();                        // Unity unloads unreferenced assets of the old level on a (non-additive) level load
  if (g_levelBuilder) g_levelBuilder(name);
}

// Application.LoadLevel takes effect at the next frame boundary (03 §2.1): the caller (usually Boot's coroutine) is not destroyed under its own feet.
void LoadLevel(const char* name) { g_pendingLevel = name; }

}  // namespace Engine
}  // namespace miniunity
