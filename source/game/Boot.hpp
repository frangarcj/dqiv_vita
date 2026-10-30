// game/Boot.hpp -- OWNER: WP3.  Port of Boot.cs (mainData's Main Camera script): logos, quit state machine, device model / GPU name, LoadLevel("dq4sp").
#pragma once
#include <string>
#include "miniunity.hpp"
using namespace miniunity;

class Boot : public Component {
public:
  static bool s_useFixedUpdate;      // always false (NEON path)
  static std::string s_deviceModel;  // Boot.s_deviceModel (Build.MODEL upper-cased); DQ4-only, read by Field.SystemInit
  static std::string s_gpuName;      // Boot.s_gpuName (egl.cfg); DQ4-only, read by Field.SystemInit
  bool HasCoStart() const override { return true; }
  Coroutine StartCo() override;
private:
  int m_quiting = 0;
  bool Quiting();
  bool Touch(bool& prs);
  Coroutine Logo();
};
