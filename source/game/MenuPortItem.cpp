// game/MenuPortItem.cpp -- OWNER: WP4a.  C# source: re/cs_out/Assembly-CSharp/MenuPortItem.cs
#include "game/Layout.hpp"
#include "game/MenuPortItem.hpp"
#include "game/MenuPortSystem.hpp"
#include "game/PadFocus.hpp"
#include "game/Sound.hpp"
#include "game/TouchPad.hpp"
#include "utils/logger.h"

static const char* s_colorStr[7][2] = {
  { "@c0", "</color>" },
  { "@c1", "<color=yellow>" },
  { "@c2", "<color=red>" },
  { "@c3", "<color=gray>" },
  { "@c4", "<color=black>" },
  { "@c5", "<color=cyan>" },
  { "@c6", "<color=lime>" },
};

static void ReplaceAll(std::string& s, const std::string& from, const std::string& to) {
  for (size_t p = s.find(from); p != std::string::npos; p = s.find(from, p + to.size())) s.replace(p, from.size(), to);
}

MenuPortItem::MenuPortItem(short port, short type) : MenuPort(port), m_type(type) { m_objectActive = true; }

short MenuPortItem::AddParts(MenuParts* parts, bool bForceButton) {
  m_partsList.push_back(parts);
  if (parts->m_button != nullptr || bForceButton) m_buttonList.push_back(parts);
  return (short)(m_partsList.size() - 1);
}

void MenuPortItem::SetTableForCode(short id, short partsNo) {
  m_visCodes.push_back(id);
  m_codeControl[id] = [this, partsNo](short code) { m_partsList[partsNo]->gameObject->SetActiveRecursively(code != 0); };
}
void MenuPortItem::SetTableForCode(short id, FUNC_SetCode func) { m_codeControl[id] = func; }

void MenuPortItem::SetItemCode(short id, short code) {
  auto it = m_codeControl.find(id);
  if (it == m_codeControl.end()) { Debug::LogError("MenuPortItem(type %d): no item code %d", (int)m_type, (int)id); return; }
  it->second(code);
}

void MenuPortItem::SetTableForItemParam(short id, short no, int partsNo, int baseNo) { m_partsControl[(id << 16) | no] = m_partsList[partsNo]->m_list[baseNo]; }
void MenuPortItem::SetTableForItemParam(short id, short no, MenuParts::IBasePtr ibase) { m_partsControl[(id << 16) | no] = ibase; }
void MenuPortItem::SetTableForItemParamTextColorChanger(short id, short no, MenuParts* parts) { m_partsControl[(id << 16) | no] = std::make_shared<MenuParts::ITextColorChanger>(parts); }

void MenuPortItem::SetItemParam(short id, short no, int data) {
  auto it = m_partsControl.find((id << 16) | no);
  if (it == m_partsControl.end()) { Debug::LogError("MenuPortItem(type %d): no item param %d/%d", (int)m_type, (int)id, (int)no); return; }
  it->second->Change(data);
}

void MenuPortItem::SetItemParam(short id, short no, std::string str) {
  for (int i = 0; i < 7; i++) ReplaceAll(str, s_colorStr[i][0], s_colorStr[i][1]);
  auto it = m_partsControl.find((id << 16) | no);
  if (it == m_partsControl.end()) { Debug::LogError("MenuPortItem(type %d): no item param %d/%d (string)", (int)m_type, (int)id, (int)no); return; }
  it->second->Change(str);
}

void MenuPortItem::Close() {
  if (m_uiLayer == 8) Layout::NotePanel(this, false);
  for (MenuParts* parts : m_partsList) Object::Destroy(parts->gameObject);
}

void MenuPortItem::SetVisible(bool bShow) {
  for (MenuParts* parts : m_partsList) parts->SetVisible(bShow);
  if (m_uiLayer == 8) Layout::NotePanel(this, bShow);
}

void MenuPortItem::NoticeReceive(Param prm) {
  if (!m_objectActive && prm[0] != 7) {
    for (MenuParts* parts : m_partsList) parts->gameObject->SetActiveRecursively(true);
    m_bForuceDrawRequest = true;
    m_objectActive = true;
  }
  switch (prm[0]) {
    case 1: m_bPad = prm[1] != 0; m_bCancel = prm[2] != 0; m_bActive = prm[3] != 0; break;
    case 2: m_menuItemWidth = prm[1]; m_menuItemHeight = prm[2]; m_menuItemNum = prm[3]; break;
    case 3: m_bDirectPad = prm[1] != 0; break;
    case 4: m_active = prm[1]; break;
    case 5: m_bDrawRequest = true; break;
    case 6: m_enableInput = true; break;
    case 7:
      if (!m_objectActive) break;
      for (MenuParts* button : m_buttonList) button->SetPush(false);
      for (MenuParts* parts2 : m_partsList) parts2->gameObject->SetActiveRecursively(false);
      m_objectActive = false;
      if (m_uiLayer == 8) Layout::NotePanel(this, false);
      break;
    case 8: m_bIgnoreDrawRequset = prm[1] != 0; break;
    case 10: SetItemParam(prm[1], prm[2], prm[3] | (prm[4] << 16)); break;
    case 11: {
      auto it = m_partsControl.find((prm[1] << 16) | prm[2]);
      if (it != m_partsControl.end()) it->second->m_sprm = prm[3];
      SetItemParam(prm[1], prm[2], Receive().ConvertString());
      break;
    }
    case 12:
      SetItemCode(prm[1], prm[2]);
      m_bForuceDrawRequest = true;
      break;
    case 13: m_bDrawRequest = false; break;
    default: Debug::Log("Unknown MenuItemPortCode %d (port %d type %d) %s", (int)prm[0], (int)m_port, (int)m_type, prm.Dump().c_str()); break;
  }
}

void MenuPortItem::Update(int pushState, Ray& ray) {
  if (!m_bIgnoreDrawRequset && (m_bForuceDrawRequest || m_bDraw != m_bDrawRequest)) {
    m_bForuceDrawRequest = false;
    SetVisible(m_bDraw = m_bDrawRequest);
  }
  m_bDrawRequest = false;
  if (!m_enableInput) return;
  if (m_objectActive && m_bDraw) PadFocus::Register(this);      // [WP3] physical-button focus (PadFocus.cpp)
  switch (pushState) {
    case 1:
      for (MenuParts* button : m_buttonList) {
        if (button->m_isHit(ray)) {
          m_pushing = button;
          m_pushing->SetPush(m_bHit = true);
          if (button->m_se) Sound::PlaySE(300);
          break;
        }
      }
      break;
    case 2:
      if (m_pushing != nullptr) m_pushing->SetPush(m_bHit = m_pushing->m_isHit(ray));
      break;
    case 3:
      if (!(m_pushing != nullptr)) break;
      m_pushing->SetPush(false);
      if (m_bHit) {
        if (m_pushing->m_externalToggle) TouchPad::m_externalToggle |= m_pushing->m_gamepad;
        else Send(1, m_pushing->m_buttonId, m_pushing->m_gamepad);
        m_bHit = false;
      }
      m_pushing = nullptr;
      break;
  }
  if (TouchPad::m_bToggleESC) {
    for (MenuParts* button2 : m_buttonList) {
      if ((button2->m_gamepad == 32 || button2->m_gamepad == 1024) && button2->m_button->m_bEnable) {
        if (button2->m_externalToggle) TouchPad::m_externalToggle |= 1024;
        else Send(1, button2->m_buttonId, button2->m_gamepad);
        TouchPad::m_bToggleESC = false;
      }
    }
  }
  m_enableInput = false;
}

void MenuPortItem::DebugFill() {
  for (short id : m_visCodes) SetItemCode(id, 1);
  for (auto& kv : m_partsControl)
    if (dynamic_cast<MenuParts::IText*>(kv.second.get())) kv.second->Change(std::string("Sample 123 \xE2\x98\x85\xE2\x96\xBCN"));
}
