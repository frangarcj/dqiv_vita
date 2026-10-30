// game/Cleaner.cpp -- OWNER: WP3. See Cleaner.hpp. C# source: Cleaner.cs
// Deviations: GC.Collect/WaitForPendingFinalizers are no-ops; ResetNativePacket x3 is skipped (the packet arrays are static and never move, 01 §1.3).
#include "game/Cleaner.hpp"
#include "game/Global.hpp"

void Cleaner::Cleaning() {
  Global::Flag |= 131072;
  (new GameObject("Cleaner"))->AddComponent<Cleaner>();
}

void Cleaner::Start() { StartCoroutine(Clean()); }

Coroutine Cleaner::Clean() {
  while (m_lateFrame > 0) co_yield 0;
  Resources::UnloadUnusedAssets();
  co_yield 0;
  // GC.Collect(2) ... GC.WaitForPendingFinalizers() ... NativeManager/MenuPortSystem/ParticleSystem.ResetNativePacket(): nothing to do
  co_yield 0;
  Global::Flag &= -131073;
  Object::Destroy(gameObject);
}

void Cleaner::Update() { m_lateFrame--; }
