// game/Cleaner.cpp -- OWNER: WP3. See Cleaner.hpp. C# source: Cleaner.cs (DQ4 version: Make(end) / Task_Cleaning, completion callback instead of the DQ5 GC + ResetNativePacket steps)
// Deviations: yield return Resources.UnloadUnusedAssets() is UnloadUnusedAssets + one frame.
#include "game/Cleaner.hpp"
#include "game/Global.hpp"

void Cleaner::Make(CallbackEnd end) {
  Cleaner* cleaner = (new GameObject("Cleaner"))->AddComponent<Cleaner>();
  cleaner->m_end = std::move(end);
}

Coroutine Cleaner::Task_Cleaning() {
  bool bLoop = true;
  Make([&bLoop]() { bLoop = false; });
  while (bLoop) co_yield 0;
}

void Cleaner::Cleaning() {
  Global::Flag |= 131072;
  Make([]() { Global::Flag &= -131073; });
}

void Cleaner::Start() { StartCoroutine(Clean()); }

Coroutine Cleaner::Clean() {
  while (m_lateFrame > 0) co_yield 0;
  Resources::UnloadUnusedAssets();
  co_yield 0;
  if (m_end) m_end();
  Object::Destroy(gameObject);
}

void Cleaner::Update() { m_lateFrame--; }
