// engine/coroutine.cpp -- coroutine runner (WP1). Unity 4: coroutines resume after all Update() of a frame;
// `yield return 0` => next frame, WaitForSeconds => Time.deltaTime countdown. A coroutine bound to a component
// stops when the component is destroyed or its GameObject is inactive (it is NOT resumed on re-activation).
#include "miniunity.hpp"
#include "engine/engine_internal.hpp"

#include <algorithm>

namespace miniunity {

namespace engine {
struct CoEntry { Coroutine co; Component* owner; Object* ownerObj; };
static std::vector<CoEntry> g_coros;

void StartCoroutineOwned(Coroutine c, Component* owner) {
  g_coros.push_back(CoEntry{std::move(c), owner, owner});
  // Unity runs the first step of StartCoroutine() immediately; the port does it at the next StepCoroutines
  // (the only call site, Boot, relies on neither)
}

void StepCoroutines(float dt) {
  // new coroutines started while stepping run next frame
  size_t n = g_coros.size();
  for (size_t i = 0; i < n; i++) {
    CoEntry& e = g_coros[i];
    if (e.co.Done()) continue;
    if (e.owner && (!IsLive(e.ownerObj) || e.owner->destroyed || !e.owner->gameObject || !e.owner->gameObject->activeInHierarchy())) {
      e.co = Coroutine();     // drop
      continue;
    }
    float w = e.co.WaitLeft();
    if (w > 0.f) {
      w -= dt;
      if (w > 0.f) { e.co.SetWaitLeft(w); continue; }
      e.co.SetWaitLeft(0.f);
    }
    e.co.MoveNext();
  }
  g_coros.erase(std::remove_if(g_coros.begin(), g_coros.end(), [](const CoEntry& e) { return e.co.Done(); }), g_coros.end());
}

void ClearCoroutinesOfDestroyed() {
  for (CoEntry& e : g_coros) if (e.owner && !IsLive(e.ownerObj)) e.co = Coroutine();
}
}  // namespace engine

void StartCoroutine(Coroutine c) { engine::StartCoroutineOwned(std::move(c), nullptr); }

}  // namespace miniunity
