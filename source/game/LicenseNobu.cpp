// game/LicenseNobu.cpp -- OWNER: WP4a/b.  C# sources: LicenseNobu.cs, LicensePolicy.cs
#include "game/LicenseNobu.hpp"
#include <cfloat>
#include "game/TouchPad.hpp"

LicenseNobu* LicenseNobu::Create(float bias) {
  LicenseNobu* licenseNobu = MenuParts::Create<LicenseNobu>(7);
  licenseNobu->AddButton(Vector3::zero, 0.f, 80.f * bias, 0, 0, 54, Color::white);
  licenseNobu->m_bias = bias;
  licenseNobu->m_scroolMax = 500.f * bias + 6.25f;
  licenseNobu->m_isHit = [](Ray&) { return false; };
  return licenseNobu;
}

void LicenseNobu::Start() {
  Vector3 localPosition = transform()->localPosition;
  localPosition.z = m_scroolMax;
  transform()->localPosition = localPosition;
}

void LicenseNobu::Update() {
  if (TouchPad::m_pad.bIsPress()) {
    Ray ray = TouchPad::m_pad.GetRay(TouchPad::m_camera);
    if (!m_bHit) {
      RaycastHit hit;
      Collider* c = collider();
      m_bHit = c && c->Raycast(ray, &hit, FLT_MAX);
      if (m_bHit) m_nobuS = ray.origin.z - transform()->position().z;
    }
    if (m_bHit) {
      Vector3 position = transform()->position();
      position.z = ray.origin.z - m_nobuS;
      transform()->setPosition(position);
      position = transform()->localPosition;
      if (position.z < 6.25f) { position.z = 6.25f; transform()->localPosition = position; }
      else if (position.z > m_scroolMax) { position.z = m_scroolMax; transform()->localPosition = position; }
      m_pos = 1.f - (position.z - 6.25f) / (m_scroolMax - 6.25f);
      position = m_target->position();
      position.z = m_textS + m_textH * m_pos;
      m_target->setPosition(position);
    }
  } else {
    m_bHit = false;
  }
}

LicensePolicy* LicensePolicy::Create(MenuPortItem* parent, const std::string& text) {
  MenuParts* menuParts = MenuParts::CreateBuildVariableWindow(0, 360.f, 46.f, 7, false);
  LicensePolicy* licensePolicy = MenuParts::Create<LicensePolicy>(7);
  licensePolicy->m_maker = new MeshMaker(MenuParts::s_info[licensePolicy->m_nowParts = 7].GetMaterial(true));
  licensePolicy->m_maker->m_owned.push_back(licensePolicy->m_maker->m_materials[0]);
  licensePolicy->AddButton(Vector3::zero, 334.f, 24.f, 0, 0, 0, Color::white);
  licensePolicy->Build(7, 7, -35.f, 10.f, 6.f);
  licensePolicy->AddTextMesh(Vector3(0.f, 11.f, 26.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter)->GetComponent<TextMesh>()->text = text;
  licensePolicy->m_isHit = [](Ray&) { return false; };
  menuParts->transform()->setPosition(eIndent::CalcScreenPosition(7, -34.f, 0.f, -1.f));
  parent->AddParts(menuParts);
  parent->AddParts(licensePolicy);
  return licensePolicy;
}

void LicensePolicy::Update() {
  bool press = TouchPad::m_pad.bIsPress();
  bool hit = false;
  if (press) {
    Ray ray = TouchPad::m_pad.GetRay(TouchPad::m_camera);
    RaycastHit h;
    Collider* c = collider();
    hit = c && c->Raycast(ray, &h, FLT_MAX);
  }
  if (hit && !m_down) { m_down = true; m_button->m_intChanger(1); }          // OnMouseDown
  else if (!hit && m_down) { m_down = false; m_button->m_intChanger(0); }    // OnMouseUp (OnMouseUpAsButton would open the privacy URL)
}
