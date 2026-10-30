// game/ShakeFrame.hpp -- OWNER: WP4a/b.  Port of ShakeFrame.cs (+ShakeFrameEx: FixedUpdate variant, unused): a frame that shakes with MenuMessage::s_shakeTable when hit.
#pragma once
#include <functional>
#include "game/MenuMessage.hpp"
#include "game/MenuParts.hpp"

class ShakeFrame : public MenuParts {
public:
  int m_shake = 0;
  std::function<void()> m_shakeEndCallback;
  void Update() override {
    if (m_shake > 0) {
      m_shake--;
      transform()->Translate((float)MenuMessage::s_shakeTable[m_shake][0], 0.f, (float)MenuMessage::s_shakeTable[m_shake][1]);
      if (m_shake == 0 && m_shakeEndCallback) m_shakeEndCallback();
    }
  }
};
