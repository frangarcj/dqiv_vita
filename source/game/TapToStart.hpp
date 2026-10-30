// game/TapToStart.hpp -- OWNER: WP4a.  Port of TapToStart.cs: title screen "TOUCH to START" blinker + copyright line (screen Make_TitleTouch).
#pragma once
#include "game/MenuParts.hpp"

class TapToStart : public MenuParts {
public:
  static constexpr float c_acc = 0.025f, c_bias = 0.68f, c_width = 223.04001f, c_height = 41.48f, c_copyrightWidth = 450.f, c_copyrightHeight = 40.0005f, c_copyrightAcc = 0.0625f;
  static constexpr int c_cwait = 20, c_firstWait = 720, c_copyrightFirstWait = 690, c_copyrightSecondWait = 2508;

  static TapToStart* Create();
  void OnDestroy() override;
  void CountReset();
  void Update() override;

private:
  Material* m_material = nullptr;
  float m_acc = 0;
  bool m_first = false;
  int m_beginFrame = 0, m_waitTouch = 0, m_waitCopyright = 0;
  float m_alpha = 0;
  MenuParts* m_copyright = nullptr;
  Material* m_copyrightMaterial = nullptr;
  float m_copyrightAlpha = 0;
  TapToStart* Setup();
};
