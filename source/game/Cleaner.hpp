// game/Cleaner.hpp -- OWNER: WP3.  Port of Cleaner.cs: after every field/stage load `Cleaning()` sets FLAG_CLEANER (blocks NativeManager.Update + MenuPort
// processing) for ~5 frames while assets unload.  Frame counting is kept exactly (3 Update ticks, then 2 more yields) so native sees the same latency.
#pragma once
#include <functional>
#include "miniunity.hpp"
using namespace miniunity;

class Cleaner : public Component {
public:
  using CallbackEnd = std::function<void()>;      // C# delegate void CallbackEnd()
  static void Make(CallbackEnd end = nullptr);
  static Coroutine Task_Cleaning();              // DQ4: waits until a Cleaner finished
  static void Cleaning();
  void Start() override;
  void Update() override;
private:
  CallbackEnd m_end;
  int m_lateFrame = 3;
  Coroutine Clean();
};
