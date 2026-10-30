// game/BattleUpCursor.hpp -- OWNER: WP4a.  Port of BattleUpCursor.cs: blinking battle-status cursor (icon frame 2/3 every 0.7 s).
#pragma once
#include "game/MenuParts.hpp"

class BattleUpCursor : public MenuParts {
public:
  static constexpr float c_waitTime = 0.7f;
  bool m_flip = false;
  float m_wait = 0.f;
  void ForceUpdate() { m_flip = false; m_list[0]->m_intChanger(2); m_wait = 0.f; }
  void Update() override {
    if ((m_wait += Time::deltaTime) >= 0.7f) {
      m_list[0]->m_intChanger(!(m_flip = !m_flip) ? 2 : 3);
      m_wait = 0.f;
    }
  }
};
