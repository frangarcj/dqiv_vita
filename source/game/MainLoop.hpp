// game/MainLoop.hpp -- OWNER: WP3.  Port of MainLoop.cs (level0's MainCamera script): boot of libdq4core, then NativeManager.Update() each frame.
#pragma once
#include "miniunity.hpp"
using namespace miniunity;

class MainLoop : public Component {
public:
  static Camera* m_camera;
  static bool s_ready;                      // set when the native boot in StartCo is finished (dev aids wait for it)
  bool HasCoStart() const override { return true; }
  Coroutine StartCo() override;             // IEnumerator Start()
  void Update() override;                   // NativeManager.Update() unless FLAG_PAUSE|FLAG_CLEANER
  void OnApplicationPause(bool pause) override;
  void OnApplicationQuit() override;
};
