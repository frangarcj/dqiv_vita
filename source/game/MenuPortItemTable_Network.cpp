// game/MenuPortItemTable_Network.cpp -- OWNER: WP4b.  Make_NetworkRoot/PLST/CODE/ITEM (ids 75-79): MenuPortItemTable.cs 3818-3963.
// The Vita build has no network service: NetConnection (WP6 stub) has no present-code list and no input code, exactly the "offline" state of the C#
// (s_presentcodeList == null / s_inputCode == null). The screens are built faithfully so native can open them; their data tables stay empty.
#include "game/MenuPortItemTable.hpp"
#include "utils/logger.h"
#include <array>

namespace {
struct PRESENT_CODE { std::string m_title, m_msg, m_code; };
const std::vector<PRESENT_CODE>& PresentList() { static const std::vector<PRESENT_CODE> v; return v; }   // NetConnection.s_presentcodeList (empty offline)
const std::string& InputCode() { static const std::string s; return s; }                                    // NetConnection.s_inputCode

void AppendUtf8(std::string& s, uint32_t c) {
  if (c < 0x80) s += (char)c;
  else if (c < 0x800) { s += (char)(0xC0 | (c >> 6)); s += (char)(0x80 | (c & 0x3F)); }
  else { s += (char)(0xE0 | (c >> 12)); s += (char)(0x80 | ((c >> 6) & 0x3F)); s += (char)(0x80 | (c & 0x3F)); }
}
}  // namespace

MenuPortItem* MenuPortItemTable::Make_NetworkRoot(short port, short type) {
  MenuPortItem* menuPortItem = new MenuPortItem(port, type);
  MenuParts* menuParts = MenuParts::CreateBuildFrameParts(291 + type, 6, 0.f, 0.f, -2.f);
  menuParts->AddTextMesh(Vector3(117.5f, 4.f, (type != 0) ? 224.f : 336.f), 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
  short framePartsNo = menuPortItem->AddParts(menuParts);
  menuPortItem->SetTableForItemParam(4, 0, framePartsNo, 1);
  if (type == 0) {
    for (short num = 0; num < 4; num++) Sub_StartMenuButtonLong(menuPortItem, menuParts->transform(), num, CalcStartButtonYpos(num));
  } else {
    for (short num2 = 0; num2 < 2; num2++) Sub_StartMenuButtonLong(menuPortItem, menuParts->transform(), num2, CalcStartButtonYpos((short)(num2 + 2)));
  }
  Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
  return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_NetworkPLST(short port, short type) {
  const std::vector<PRESENT_CODE>& plist = PresentList();
  MenuPortItem* menuPortItem = new MenuPortItem(port, type);
  MenuParts* menuParts = MenuParts::CreateBuildFrameParts(293, 6, 0.f, 0.f, -2.f);
  auto texts = std::make_shared<std::array<TextMesh*, 6>>();            // texts[page row][field]
  int pmax = (int)plist.size() / 2;
  if ((plist.size() & 1) == 0) pmax--;
  menuParts->AddTextMesh(7, 0.f, 4.f, 468.f, 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
  (*texts)[0] = menuParts->AddTextMesh(7, -203.f, 4.f, 434.f, 9.5f)->GetComponent<TextMesh>();
  (*texts)[1] = menuParts->AddTextMesh(7, -183.f, 4.f, 400.f, 9.f, TextAlignment::Left, TextAnchor::UpperLeft, 1.3f)->GetComponent<TextMesh>();
  (*texts)[2] = menuParts->AddTextMesh(7, 0.f, 4.f, 319.f, 9.5f, TextAlignment::Center, TextAnchor::UpperCenter)->GetComponent<TextMesh>();
  (*texts)[3] = menuParts->AddTextMesh(7, -203.f, 4.f, 271.f, 9.5f)->GetComponent<TextMesh>();
  (*texts)[4] = menuParts->AddTextMesh(7, -183.f, 4.f, 238.f, 9.f, TextAlignment::Left, TextAnchor::UpperLeft, 1.3f)->GetComponent<TextMesh>();
  (*texts)[5] = menuParts->AddTextMesh(7, 0.f, 4.f, 159.f, 9.5f, TextAlignment::Center, TextAnchor::UpperCenter)->GetComponent<TextMesh>();
  GameObject* txPageGo = menuParts->AddTextMesh(7, 0.f, 4.f, 95.f, 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
  TextMesh* txPage = txPageGo->GetComponent<TextMesh>();
  txPageGo->transform->SetParent(menuParts->transform());
  short partsNo = menuPortItem->AddParts(menuParts);
  menuPortItem->SetTableForItemParam(0, 0, partsNo, 1);
  MenuParts* menuParts2 = MenuParts::CreateButtonIcon(10, 0, 1.f, -1, 4);
  menuParts2->Build(4, 7, -176.f, 4.f, 95.f, txPageGo->transform);
  menuPortItem->AddParts(menuParts2);
  menuParts2 = MenuParts::CreateButtonIcon(10, 1, 1.f, -1, 8);
  menuParts2->Build(4, 7, 176.f, 4.f, 95.f, txPageGo->transform);
  menuPortItem->AddParts(menuParts2);
  if (pmax == 0) txPageGo->SetActiveRecursively(false);
  auto pageNo = std::make_shared<int>(0);
  menuPortItem->SetTableForCode(0, [texts, txPage, pageNo, pmax](short code) {
    const std::vector<PRESENT_CODE>& pl = PresentList();
    if (!pl.empty()) {
      if (code < 0) { (*pageNo)--; if (*pageNo < 0) *pageNo = pmax; }
      else if (code > 0) { (*pageNo)++; if (*pageNo > pmax) *pageNo = 0; }
      int num = 0, num2 = *pageNo * 2;
      while (num < 2) {
        if (num2 < (int)pl.size()) {
          (*texts)[num * 3 + 0]->text = pl[num2].m_title;
          (*texts)[num * 3 + 1]->text = pl[num2].m_msg;
          (*texts)[num * 3 + 2]->text = pl[num2].m_code;
        } else {
          (*texts)[num * 3 + 0]->text = "";
          (*texts)[num * 3 + 1]->text = "";
          (*texts)[num * 3 + 2]->text = "";
        }
        num++; num2++;
      }
      if (pmax > 0) txPage->text = std::to_string(*pageNo + 1) + "/" + std::to_string(pmax + 1);
    }
  });
  Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
  return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_NetworkCODE(short port, short type) {
  MenuPortItem* menuPortItem = new MenuPortItem(port, type);
  MenuParts* menuParts = MenuParts::CreateBuildFrameParts(295, 4, 0.f, 0.f, 120.f);
  short num = 0;
  std::string text;
  for (char c : InputCode()) {
    int num2 = c;
    if (num == 4) { text += "\t-\t"; num = 0; }
    else if (!text.empty()) text += "\t\t";
    AppendUtf8(text, (uint32_t)(num2 - 32 + 65280));        // full-width form of the ASCII char
    num++;
  }
  TextMesh* component = menuParts->AddTextMesh(Vector3(-3.f, 4.f, 0.f), 8.5f, TextAlignment::Center, TextAnchor::MiddleCenter)->GetComponent<TextMesh>();
  component->text = text;
  component->tabSize = 1.f;
  menuPortItem->AddParts(menuParts);
  return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_NetworkITEM(short port, short type) {
  MenuPortItem* menuPortItem = new MenuPortItem(port, type);
  MenuParts* menuParts = MenuParts::CreateBuildFrameParts(294, 1, 0.f, 0.f, 0.f);
  menuParts->AddTextMesh(1, 0.f, 4.f, -132.5f, 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
  menuParts->AddTextMesh(1, 0.f, 4.f, -160.f, 9.5f, TextAlignment::Center, TextAnchor::UpperCenter, 1.5f);
  short partsNo = menuPortItem->AddParts(menuParts);
  menuPortItem->SetTableForItemParam(0, 0, partsNo, 1);
  menuPortItem->SetTableForItemParam(0, 1, partsNo, 2);
  return menuPortItem;
}
