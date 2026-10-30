// game/MenuPortItemTable_License.cpp -- OWNER: WP4b.  Make_License (id 80): MenuPortItemTable.cs 3996-4065.
// The licence text is split in two TextMeshes at its 244th line (a TextMesh mesh limit); offsets are counted in UTF-8 bytes here (UTF-16 chars in the C#).
#include "game/LicenseNobu.hpp"
#include "game/MenuPortItemTable.hpp"
#include "utils/logger.h"

MenuPortItem* MenuPortItemTable::Make_License(short port, short type) {
  MenuPortItem* menuPortItem = new MenuPortItem(port, type);
  double num = (double)Screen::height / (double)Screen::width;
  num /= 1.777777777777778;
  MenuParts* menuParts = MenuParts::CreateBuildVariableWindow(0, 14.f, 600.f * (float)num, 8, false);
  MenuParts* menuParts2 = MenuParts::CreateWindow0(54.f, 47.f);
  LicenseNobu* licenseNobu = LicenseNobu::Create((float)num);
  TextAsset* textAsset = Resources::Load<TextAsset>("Menu/License_list_A");
  std::string text = textAsset ? textAsset->text() : std::string();
  int num2 = 0, num3 = 0, num4 = 0;
  float num5 = 0.f, num6 = 4.525f;
  switch (GetLangages::m_langagesNumber) {
    case 1: num5 = 13.912499f; num6 *= 1.05f; break;
    case 2: num5 = 11.6375f; num6 *= 0.875f; break;
    case 3:
    case 4: num5 = 13.3f; break;
  }
  if (textAsset) Resources::UnloadAsset(textAsset);
  for (char c : text) {
    num3++;
    if (c == '\n') {
      num2++;
      if (num2 == 244) num4 = num3;
    }
  }
  Debug::Log("license lines %d", num2);
  menuParts2->Build(8, 8, 2.f, 0.f, -2.f);
  licenseNobu->Build(8, Vector3(-6.75f, 4.f, 6.25f), menuParts->transform());
  licenseNobu->transform()->localScale = Vector3(0.8f, 1.f, 1.f);
  menuParts->transform()->setPosition(eIndent::CalcScreenPosition(8, 0.f, 0.f, 80.f));
  menuParts->transform()->SetParent(menuParts2->transform());
  GameObject* gameObject = menuParts->AddTextMesh(0, 0.f, -8.f, 0.f, num6, TextAlignment::Left, TextAnchor::UpperLeft, 1.4f);
  GameObject* gameObject2 = menuParts->AddTextMesh(0, 0.f, -8.f, 0.f, num6, TextAlignment::Left, TextAnchor::UpperLeft, 1.4f);
  gameObject->GetComponent<TextMesh>()->text = text.substr(0, (size_t)num4);
  gameObject2->GetComponent<TextMesh>()->text = (size_t)num4 <= text.size() ? text.substr((size_t)num4) : std::string();
  gameObject2->transform->SetParent(gameObject->transform);
  gameObject2->transform->localPosition = Vector3(0.f, (0.f - num5) * 244.f, 0.f);
  licenseNobu->m_target = gameObject->transform;
  licenseNobu->m_textS = gameObject->transform->position().z;
  licenseNobu->m_textH = (float)num2 * num5 - licenseNobu->m_textS;
  menuPortItem->AddParts(menuParts2);
  menuPortItem->AddParts(menuParts);
  menuPortItem->AddParts(licenseNobu);
  Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts2->transform());
  static const char* array[5] = { "プライバシーポリシー", "Privacy Policy", "개인 정보 보호 정책", "隱私條款", "隐私条款" };
  LicensePolicy::Create(menuPortItem, array[GetLangages::m_langagesNumber]);
  return menuPortItem;
}
