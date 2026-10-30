// game/Boot.hpp -- OWNER: WP3.  Port of Boot.cs (mainData's Main Camera script): logos, quit state machine, LoadFileList, LoadLevel("dq4sp").
#pragma once
#include "miniunity.hpp"
using namespace miniunity;

class Boot : public Component {
public:
  static bool s_useFixedUpdate;      // always false (NEON path)
  bool HasCoStart() const override { return true; }
  Coroutine StartCo() override;
private:
  int m_quiting = 0;
  bool Quiting();
  bool Touch(bool& prs);
  Coroutine Logo();
};
