// game/LicenseNobu.hpp -- OWNER: WP4a/b.  Port of LicenseNobu.cs (scroll bar knob of the licence screen, dragged with the touch ray) and LicensePolicy.cs (privacy-policy button).
#pragma once
#include "game/MenuParts.hpp"
#include "game/MenuPortItem.hpp"

class LicenseNobu : public MenuParts {
public:
  static constexpr float c_baseY = 6.25f;
  float m_pos = 0, m_textS = 0, m_textH = 0;
  Transform* m_target = nullptr;
  static LicenseNobu* Create(float bias);
  void Start() override;
  void Update() override;
private:
  float m_nobuS = 0, m_scroolMax = 0, m_bias = 0; bool m_bHit = false;
};

class LicensePolicy : public MenuParts {
public:
  static LicensePolicy* Create(MenuPortItem* parent, const std::string& text);
  void Update() override;          // C# OnMouseDown/OnMouseUp/OnMouseUpAsButton: button state from the touch ray (the URL is not opened on the Vita)
private:
  bool m_down = false;
};
