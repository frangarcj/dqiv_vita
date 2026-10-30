// game/MenuPortItemTable_helpers.cpp -- OWNER: WP4b.  Nested helper classes of MenuPortItemTable.cs (StatusIconChanger, ItemNumCTRL, ICityMap, IBetText, ...).
#include "game/MenuPortItemTable.hpp"
#include "utils/logger.h"

MenuPortItemTable::StatusIconChanger::StatusIconChanger(Material* target, MenuParts::IBasePtr sinidoku, MenuParts::IBasePtr noroi) {
  m_intChanger = [target, sinidoku, noroi](int value) {
    sinidoku->m_intChanger(value & 3);
    noroi->m_intChanger(((value >> 2) & 1) * 3);
    target->color = MenuParts::s_colorList[value >> 16];
  };
}

MenuPortItemTable::ItemNumCTRL::ItemNumCTRL(IntChanger num2, IntChanger num1) : m_num2(num2), m_num1(num1) { m_intChanger = [this](int v) { Changer(v); }; }

void MenuPortItemTable::ItemNumCTRL::Changer(int value) {
  if (value > 1) {
    int num = value / 10 % 10;
    int num2 = value % 10;
    m_num2((num == 0) ? 1 : (num * 16));
    m_num1(num2 * 16);
  } else {
    m_num2(1);
    m_num1(1);
  }
}

MenuPortItemTable::ICityMap::ICityMap(MenuPortItem* item, MenuParts* posf, Transform* root) {
  m_stringChanger = [this, item, posf, root](const std::string& value) {
    MenuParts* menuParts = MenuParts::CreateFromTexture("Menu/map/" + value, true, 0.75f);
    menuParts->Build(0, 0, 2.f, 12.f, -2.f);
    menuParts->transform()->localScale = Vector3(0.99f, 0.99f, 0.99f);
    menuParts->transform()->SetParent(root);
    posf->transform()->SetParent(menuParts->transform());
    item->AddParts(menuParts);
    m_self = menuParts->transform();
  };
}

MenuPortItemTable::IBetText::IBetText(TextMesh* textMesh) {
  static const char* s_replace[8][2] = { { "@0", "</color>" }, { "@1", "<color=#ff84ffff>" }, { "@2", "<color=#31ceffff>" }, { "@3", "<color=#84ff84ff>" },
                                         { "@4", "<color=#ffc542ff>" }, { "@5", "<color=#ff4242ff>" }, { "@6", "<color=#4280ffff>" }, { "@7", "<color=#ffff00ff>" } };
  m_stringChanger = [textMesh](const std::string& text) {
    std::string s = text;
    for (int i = 0; i < 8; i++) {
      std::string from = s_replace[i][0], to = s_replace[i][1];
      for (size_t p = s.find(from); p != std::string::npos; p = s.find(from, p + to.size())) s.replace(p, from.size(), to);
    }
    textMesh->text = s;
  };
}

const MenuPortItemTable::NAME_BUTTON_PARAM MenuPortItemTable::name_button_param[5] = { { 4, 382.f, 67.f }, { 5, 382.f, 67.f }, { 5, 434.f, 67.f }, { 6, 434.f, 67.f }, { 5, 412.f, 67.f } };

// ---- battle helpers ----
#include "game/MainLoop.hpp"
#include "game/MeshChara.hpp"
#include "game/NativeManager.hpp"
#include "game/TouchPad.hpp"
#include <algorithm>
#include <cfloat>
#include "game/UiCameras.hpp"
#include "game/Layout.hpp"

MenuPortItemTable::BattleBar::BattleBar(std::shared_ptr<MenuParts::IBar> bar) : m_bar(bar) {
  m_intChanger = [this](int value) { m_bar->m_intChanger(m_value = value); };
  m_stringChanger = [this](const std::string&) { RedApply(); };
}
void MenuPortItemTable::BattleBar::RedApply() { m_bar->m_intChanger(-(m_value + 1)); }

float MenuPortItemTable::MonsterMarkList::s_posY = 0.f;

MenuPortItemTable::MonsterMarkList::MonsterMarkList(MenuPortItem* item) {
  for (int i = 0; i < 12; i++) {
    MenuParts* menuParts = MenuParts::Create(6);
    menuParts->AddIcon(0, 0.f, 0.f, 0.f, 0, true, 0.5f);
    menuParts->Build(4, 4, 0.f, 0.f, 0.f);
    menuParts->gameObject->SetActive(false);
    item->AddParts(m_list[i] = menuParts);
  }
}
void MenuPortItemTable::MonsterMarkList::CalcMarkY() { s_posY = eIndent::CalcScreenPosition(1, 0.f, 0.f, -474.f).z; }
void MenuPortItemTable::MonsterMarkList::Reset() {
  for (MenuParts* menuParts : m_list) menuParts->gameObject->SetActive(false);
  m_used = 0u;
}
void MenuPortItemTable::MonsterMarkList::SetMark(int id, float x, float y) {
  for (int i = 0; i < 12; i++) {
    uint32_t num = (uint32_t)(1 << i);
    if ((m_used & num) == 0) {
      m_used |= num;
      m_list[i]->gameObject->SetActive(true);
      m_list[i]->m_list[0]->m_intChanger(5 + id);
      m_list[i]->transform()->setPosition(Vector3(x, 0.f, s_posY + y));
      break;
    }
  }
}

bool MenuPortItemTable::BoundsIntersects(const Bounds& a, const Bounds& b) {
  return a.center.x - a.extents.x <= b.center.x + b.extents.x && a.center.x + a.extents.x >= b.center.x - b.extents.x &&
         a.center.y - a.extents.y <= b.center.y + b.extents.y && a.center.y + a.extents.y >= b.center.y - b.extents.y &&
         a.center.z - a.extents.z <= b.center.z + b.extents.z && a.center.z + a.extents.z >= b.center.z - b.extents.z;
}

MenuPortItemTable::MonsterNamePlate::MonsterNamePlate(MenuPortItem* item, MonsterMarkList* marks, int id) {
  m_id = id;
  m_item = item;
  m_markList = marks;
  m_name.center = Vector3::zero; m_name.extents = Vector3(166.f * 0.5f, 1.f * 0.5f, 60.f * 0.5f);    // new Bounds(Vector3.zero, new Vector3(166, 1, 60)): (center, size)
  m_fname = MenuParts::CreateBuildVariableWindow(0, 142.f, 36.f, 4, false);
  m_text = m_fname->AddTextMesh(Vector3(-74.f, 4.f, 4.f), 8.5f, TextAlignment::Left, TextAnchor::MiddleLeft, 0.6f)->GetComponent<TextMesh>();
  m_text->tabSize = 25.f;
  item->AddParts(m_fname);
  item->SetTableForCode((short)id, [this](short code) { ReceiveParameters(code); });
}

void MenuPortItemTable::MonsterNamePlate::ReceiveParameters(short code) {
  if (code > 0) {
    float num = -FLT_MAX;
    float num2 = FLT_MAX;
    float num3 = 0.f;
    MenuPort::Param param = m_item->Receive();
    std::string text = param.ConvertString();
    m_fname->gameObject->SetActiveRecursively(true);
    if (!text.empty() && text[0] == '^') {
      text.erase(0, 1);
      m_text->characterSize = (GetLangages::m_langagesNumber >= 2 ? 7.5f : 8.5f) * MenuParts::BIAS_FONT_SIZE;
    } else {
      m_text->characterSize = 8.5f * MenuParts::BIAS_FONT_SIZE;
    }
    m_text->text = text;
    if (text.find('\n') != std::string::npos) m_fname->VariableWindow(0, 142.f, 36.f, 4);
    else m_fname->VariableWindow(0, 142.f, 18.f, 4);
    param = m_item->Receive();
    for (int i = 0; i < 4; i++) m_size[i] = (float)param[i] * 1.85f;
    uint32_t num4 = (uint32_t)(param[4] | (param[5] << 16));
    uint32_t num5 = 1u;
    uint32_t num6 = 0u;
    while (num5 != 0) {
      if ((num4 & num5) != 0) {
        Vector3 position = NativeManager::GetMeshChara((int)num6)->m_chara->transform->position();
        position = MainLoop::m_camera->WorldToScreenPoint(position);
        position = UiCameras::PlateCamera()->ScreenToWorldPoint(position);      // landscape: the full-surface camera the plates are drawn with
        if (num2 > position.x) num2 = position.x;
        if (num < position.x) num = position.x;
        m_yPos = position.z;
        switch (code) {
          case 2: m_markList->SetMark(m_id, position.x, 0.f); break;
          case 3: m_markList->SetMark(m_id, position.x, 20.f); break;
        }
      }
      num5 <<= 1;
      num6++;
    }
    num3 = m_yPos + 42.7f;
    m_center.Set(num2 + (num - num2) * 0.5f, 0.f, num3);
    m_frameHeight = std::fabs(m_size[1] - m_size[2]);
    m_nameYpos = num3 + m_frameHeight;
  } else {
    m_fname->gameObject->SetActiveRecursively(false);
  }
}
