// game/InputName.cpp -- OWNER: WP4b.  Port of InputName.cs (see InputName.hpp).
#include "game/Layout.hpp"
#include "game/UiCameras.hpp"
#include "game/InputName.hpp"
#include "game/PadFocus.hpp"
#include "game/Sound.hpp"
#include "game/TouchPad.hpp"
#include <cmath>

static const char16_t C_NA = u'＠';        // '＠' = no key
static const char16_t C_ALPHABET[70] = {
  u'a', u'b', u'c', u'd', u'e', u'f', u'g', u'h', u'i', u'j',
  u'k', u'l', u'm', u'n', u'o', u'p', u'q', u'r', u's', u't',
  u'u', u'v', u'w', u'x', u'y', u'z', u' ', u'-', u',', u'.',
  u'!', u'?', C_NA, C_NA, C_NA, u'A', u'B', u'C', u'D', u'E',
  u'F', u'G', u'H', u'I', u'J', u'K', u'L', u'M', u'N', u'O',
  u'P', u'Q', u'R', u'S', u'T', u'U', u'V', u'W', u'X', u'Y',
  u'Z', u' ', u'-', u',', u'.', u'!', u'?', C_NA, C_NA, C_NA };
static const char16_t C_AA[70] = {
  u'à', u'á', u'â', u'ä', u'ç', u'è', u'é', u'ê', u'ë', u'ì',
  u'í', u'î', u'ï', u'ñ', u'ò', u'ó', u'ô', u'ö', u'ù', u'ú',
  u'û', u'ü', u'ß', C_NA, C_NA, C_NA, C_NA, C_NA, C_NA, C_NA,
  C_NA, C_NA, C_NA, C_NA, C_NA, u'À', u'Á', u'Â', u'Ä', u'Ç',
  u'È', u'É', u'Ê', u'Ë', u'Ì', u'Í', u'Î', u'Ï', u'Ñ', u'Ò',
  u'Ó', u'Ô', u'Ö', u'Ù', u'Ú', u'Û', u'Ü', u'ß', C_NA, C_NA,
  C_NA, C_NA, C_NA, C_NA, C_NA, C_NA, C_NA, C_NA, C_NA, C_NA };
static const char16_t* const C_LOCALIZE[2] = { C_ALPHABET, C_AA };
static const float BTN_X_SPAN = 69.f, BTN_Y_SPAN = 82.f;

std::string InputName::ToUtf8(const std::u16string& s) {
  std::string text;
  for (size_t i = 0; i < s.size(); i++) {
    uint32_t c = s[i];
    if (c >= 0xD800 && c < 0xDC00 && i + 1 < s.size() && s[i + 1] >= 0xDC00 && s[i + 1] < 0xE000) { c = 0x10000 + ((c - 0xD800) << 10) + (s[i + 1] - 0xDC00); i++; }
    if (c < 0x80) text += (char)c;
    else if (c < 0x800) { text += (char)(0xC0 | (c >> 6)); text += (char)(0x80 | (c & 0x3F)); }
    else if (c < 0x10000) { text += (char)(0xE0 | (c >> 12)); text += (char)(0x80 | ((c >> 6) & 0x3F)); text += (char)(0x80 | (c & 0x3F)); }
    else { text += (char)(0xF0 | (c >> 18)); text += (char)(0x80 | ((c >> 12) & 0x3F)); text += (char)(0x80 | ((c >> 6) & 0x3F)); text += (char)(0x80 | (c & 0x3F)); }
  }
  return text;
}
std::u16string InputName::FromUtf8(const std::string& s) {
  std::u16string r;
  for (size_t i = 0; i < s.size();) {
    uint32_t c = (unsigned char)s[i]; int n = 0;
    if (c < 0x80) n = 0; else if ((c >> 5) == 6) { c &= 0x1F; n = 1; } else if ((c >> 4) == 14) { c &= 0x0F; n = 2; } else { c &= 0x07; n = 3; }
    i++;
    for (; n > 0 && i < s.size(); n--, i++) c = (c << 6) | ((unsigned char)s[i] & 0x3F);
    if (c >= 0x10000) { c -= 0x10000; r += (char16_t)(0xD800 + (c >> 10)); r += (char16_t)(0xDC00 + (c & 0x3FF)); } else r += (char16_t)c;
  }
  return r;
}

const char16_t* InputName::MojiSet(int mode) const { return C_LOCALIZE[mode]; }

MenuParts* InputName::MakeNameButton(Vector3 bpos, int bid, Transform* prt) {
  MenuParts* menuParts = MenuParts::CreateButton(30.f, 30.f, (short)bid, 0);
  bpos.x += BTN_X_SPAN * (float)(bid % 5);
  bpos.z -= BTN_Y_SPAN * (float)(bid / 5);
  menuParts->Build(4, bpos, prt);
  menuParts->AddTextMesh(Vector3(0.f, 4.f, 0.5f), 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter)->layer = 10;
  menuParts->gameObject->layer = 10;
  return menuParts;
}

std::string InputName::MakeDispName(const std::u16string& str) {
  std::u16string text = u"\t";
  int length = (int)str.size();
  for (int i = 0; i < m_max; i++) { text += (i >= length) ? m_blank : str[i]; text += u"\t"; }
  return ToUtf8(text);
}

static bool HasVisibleChar(const std::u16string& s) {
  for (char16_t c : s) if (c != u' ' && c != u'　') return true;
  return false;
}

void InputName::ApplyAction(int state, MenuParts* hit) {
      bool flag2 = false;
      switch (state) {
        case 1: SetMode(0); hit = nullptr; break;
        case 2: SetMode(1); hit = nullptr; break;
        case 3:
          if (m_shiftNum == 1) { m_bShift = !m_bShift; SetMode(m_mode); }
          else if (!m_result.empty()) {
            int num7 = m_mojisLen >> m_shiftNum, num8 = (int)m_result.size() - 1, num9 = m_pushedIds[num8], num10 = num9 % num7, num11 = num10 + num7;
            m_mojis = MojiSet(m_mojiModes[num8]);
            if (m_mojis[num10] != m_mojis[num11]) {
              num9 = (m_pushedIds[num8] = (num9 == num11) ? num10 : num11);
              m_result = m_result.substr(0, num8) + m_mojis[num9]; flag2 = true;
            }
            m_mojis = MojiSet(m_mode);
          }
          break;
        case 4: case 5:
          if (!m_result.empty()) {
            int mul = state == 4 ? 2 : 3;
            int num12 = m_mojisLen >> 2, num13 = (int)m_result.size() - 1, num14 = m_pushedIds[num13], num15 = num14 % num12, num16 = num15 + num12 * mul;
            m_mojis = MojiSet(m_mojiModes[num13]);
            if (m_mojis[num15] != m_mojis[num16]) {
              num14 = (m_pushedIds[num13] = (num14 == num16) ? num15 : num16);
              m_result = m_result.substr(0, num13) + m_mojis[num14]; flag2 = true;
            }
            m_mojis = MojiSet(m_mode);
          }
          break;
        case 6:
          if (!m_result.empty()) { int num = (int)m_result.size() - 1; m_pushedIds[num] = -1; m_result = m_result.substr(0, num); flag2 = true; }
          break;
        case 7: {
          if (m_result.empty()) break;
          bool flag3 = HasVisibleChar(m_result);
          if (m_bCampaign) flag3 = (int)m_result.size() == m_max;
          if (flag3) m_status = 1;
          break;
        }
        case 8: m_status = -1; break;
        case 11:
          if ((int)m_result.size() < m_max) {
            m_mojiModes[m_result.size()] = m_mode;
            m_pushedIds[m_result.size()] = hit->m_buttonId;
            if (m_bShift) { m_result += m_mojis[hit->m_buttonId + (m_mojisLen >> 1)]; m_bShift = false; SetMode(m_mode); }
            else m_result += m_mojis[hit->m_buttonId];
            flag2 = true;
          }
          break;
      }
      if (flag2) {
        int num17 = 0;
        m_text->text = MakeDispName(m_result);
        if (m_bCampaign) num17 = ((int)m_result.size() != m_max) ? 3 : 0;
        else { bool flag4 = HasVisibleChar(m_result); num17 = (!flag4 || m_result.empty()) ? 3 : 0; }
        m_etc[6]->m_list[0]->m_intChanger(num17);
      }
    }


// [WP3] physical-button path.  Focus = index into m_chars (5 columns); d-pad moves (skipping unused keys), the list scrolls like the touch knob, Cross presses.
void InputName::PadUpdate() {
  PadFocus::Modal();
  int t = TouchPad::m_toggle & ~0x100, h = TouchPad::m_press;
  int dx = 0, dy = 0;
  if (t & 1) dy = -1; else if (t & 2) dy = 1; else if (t & 4) dx = -1; else if (t & 8) dx = 1;      // dy: +1 = next row (down)
  if (h & 0xF) { m_padHold++; if (!(dx | dy) && m_padHold > 16 && (m_padHold % 5) == 0) { dy = (h & 1) ? -1 : (h & 2) ? 1 : 0; dx = dy ? 0 : ((h & 4) ? -1 : 1); } } else m_padHold = 0;
  int num = m_mojisLen >> m_shiftNum;
  auto valid = [&](int i) { return i >= 0 && i < num && i < (int)m_chars.size() && m_chars[i] != nullptr && m_mojis[i] != C_NA; };
  if (!valid(m_padFocus)) { m_padFocus = 0; while (m_padFocus < num && !valid(m_padFocus)) m_padFocus++; }
  auto etcOk = [&](int id) { return id >= 1 && id <= 8 && id != 4 && id != 5 && (int)m_etc.size() >= id && m_etc[id - 1]; };
  if (m_padZone && !etcOk(m_padZone)) m_padZone = 0;
  if (dx || dy) {
    const int cols = 5, rows = (num + cols - 1) / cols;
    int c = m_padFocus % cols, r = m_padFocus / cols, zone = m_padZone;
    if (zone == 0) {
      if (dy < 0 && r == 0) zone = c <= 2 ? 1 : 2;                                  // up from the top row: the page tabs
      else if (dy > 0 && r == rows - 1) zone = c <= 1 ? 3 : 6;                      // down from the last row: Shift / Delete
      else {
        for (int guard = 0; guard < 40; guard++) {
          c = (c + dx + cols) % cols; r = (r + dy + rows) % rows;
          if (valid(r * cols + c)) { m_padFocus = r * cols + c; break; }
        }
      }
    } else {
      auto gridAt = [&](int row, int col) { int i = row * cols + col; if (valid(i)) return i; for (int k = 0; k < cols; k++) if (valid(row * cols + k)) return row * cols + k; return m_padFocus; };
      switch (zone) {
        case 1: if (dx > 0) zone = 2; else if (dy > 0) { zone = 0; m_padFocus = gridAt(0, 0); } break;
        case 2: if (dx < 0) zone = 1; else if (dy > 0) { zone = 0; m_padFocus = gridAt(0, 3); } break;
        case 3: if (dx > 0) zone = 6; else if (dy < 0) { zone = 0; m_padFocus = gridAt(rows - 1, 0); } else if (dy > 0) zone = 7; break;
        case 6: if (dx < 0) zone = 3; else if (dy < 0) { zone = 0; m_padFocus = gridAt(rows - 1, 4); } else if (dy > 0) zone = etcOk(8) ? 8 : 7; break;
        case 7: if (dx > 0 && etcOk(8)) zone = 8; else if (dy < 0) zone = 3; break;
        case 8: if (dx < 0) zone = 7; else if (dy < 0) zone = 6; break;
      }
      if (!etcOk(zone) && zone) zone = m_padZone;
    }
    if (zone != m_padZone || true) { m_padZone = zone; Sound::PlaySE(300); }
  }
  // scroll so that the focused row is visible (same maths as the knob drag in Update)
  if (m_padZone == 0) {
    int r = m_padFocus / 5;
    float rootZ = std::fmin(std::fmax(BTN_Y_SPAN * (float)(r - 1), 0.f), m_tail > 0.f ? m_tail : 0.f);
    m_root->setPosition(Vector3(0.f, 0.f, rootZ));
    if (m_nobu && m_tail > 0.f) { Vector3 lp = m_nobu->transform()->localPosition; lp.y = m_scrU - rootZ / m_tail * (m_scrU - m_scrD); m_nobu->transform()->localPosition = lp; }
  }
  MenuParts* focus = (m_padZone == 0 && valid(m_padFocus)) ? m_chars[m_padFocus] : nullptr;
  if ((t & 0x200) && focus) { m_hit = focus; ApplyAction(11, focus); Sound::PlaySE(300); m_hit = nullptr; }
  else if ((t & 0x200) && m_padZone) { ApplyAction(m_padZone, nullptr); Sound::PlaySE(300); }               // Cross on a function button
  if (t & 0x1000) { ApplyAction(6, nullptr); Sound::PlaySE(300); }                    // Square: delete
  if (t & 0x800) { ApplyAction(3, nullptr); Sound::PlaySE(300); }                     // Triangle: shift
  if (t & 0x10) ApplyAction(1, nullptr);                                              // L: alphabet page
  if (t & 0x20) ApplyAction(2, nullptr);                                              // R: accent page
  if (TouchPad::m_bToggleESC) { TouchPad::m_bToggleESC = false; ApplyAction(7, nullptr); }   // Start (= Escape key in vita_input): enter
  if (t & 0x400) { m_status = -1; Sound::PlaySE(300); }          // Circle: back
  if (m_status == 9) PadFocus::ShowMarker(nullptr);
  else if (m_padZone) PadFocus::ShowMarker(m_etc[m_padZone - 1]);                      // function buttons: MenuCamera
  else PadFocus::ShowMarker(valid(m_padFocus) ? m_chars[m_padFocus] : nullptr, m_cam);   // grid keys are drawn by the keyboard camera (layer 10), clipped to its viewport
}

void InputName::Update() {
  if (m_status == 9) return;
  if (!m_chars.empty() && m_nobu) PadUpdate();
  RawPads& pad = TouchPad::m_pad;
  if (TouchPad::m_bToggleESC) { m_status = -1; Sound::PlaySE(300); }
  if (m_status == -1) {
    m_port->Send(1, 32, 0);
    m_status = 9;
  } else if (m_status == 1) {
    std::string utf8 = ToUtf8(m_result);
    m_port->Send(2, (short)utf8.size());
    m_port->SendByteVec(std::vector<unsigned char>(utf8.begin(), utf8.end()));
    m_status = 9;
  } else if (m_bPress != pad.bIsPress()) {
    if ((m_bPress = pad.bIsPress())) {
      Ray ray = pad.GetRay(m_uiCam ? m_uiCam : TouchPad::m_camera);
      bool flag = false;
      for (MenuParts* item : m_etc) {
        if (item && item->m_isHit(ray)) {
          (m_hit = item)->SetPush(m_pushed = true);
          m_state = item->m_buttonId;
          Sound::PlaySE(300);
          flag = true;
          break;
        }
      }
      if (flag || !(Mathf::Abs(ray.origin.z) < m_areaM)) return;
      ray = pad.GetRay(m_cam);
      if (m_nobu->m_isHit(ray)) { m_nobuS = ray.origin.z - m_nobu->transform()->position().z; m_state = 10; return; }
      for (MenuParts* c : m_chars) {
        if (c && c->m_isHit(ray)) { (m_hit = c)->SetPush(m_pushed = true); m_state = 11; Sound::PlaySE(300); break; }
      }
      return;
    }
    if (m_pushed) ApplyAction(m_state, m_hit);
    if (m_hit) { m_hit->SetPush(false); m_hit = nullptr; }
    m_pushed = false;
    m_state = 0;
  } else {
    if (!m_bPress) return;
    Ray ray2 = pad.GetRay(m_cam);
    if (m_state == 0 && m_nobu->m_isHit(ray2)) { m_nobuS = ray2.origin.z - m_nobu->transform()->position().z; m_state = 10; }
    switch (m_state) {
      case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8:
        ray2 = pad.GetRay(m_uiCam ? m_uiCam : TouchPad::m_camera);
        m_hit->SetPush(m_pushed = m_hit->m_isHit(ray2));
        break;
      case 11: m_hit->SetPush(m_pushed = m_hit->m_isHit(ray2)); break;
      case 10: {
        Vector3 position = m_nobu->transform()->position();
        position.z = ray2.origin.z - m_nobuS;
        m_nobu->transform()->setPosition(position);
        position = m_nobu->transform()->localPosition;
        if (position.y > m_scrU) position.y = m_scrU - 0.5f;
        else if (position.y < m_scrD) position.y = m_scrD + 0.5f;
        m_nobu->transform()->localPosition = position;
        position = Vector3(0.f, 0.f, m_tail * (position.y - m_scrU) / (0.f - (m_scrU - m_scrD)));
        m_root->setPosition(position);
        break;
      }
      default: break;
    }
  }
}

void InputName::SetLanguage(int lng, int resultMax) {
  m_etcText[0] = "Alphabet"; m_etcText[1] = "\xC3\xA0 \xC3\x80"; m_etcText[2] = "Shift"; m_etcText[3] = ""; m_etcText[4] = ""; m_etcText[5] = "Delete"; m_etcText[6] = "Enter";
  m_mojis = MojiSet(m_mode); m_mojisLen = 70;
  m_shiftNum = 1;
  int num = m_mojisLen >> m_shiftNum;
  if (GetLangages::m_langagesNumber == 1) m_blank = u'_';
  else { m_etcText[1] = ""; m_blank = u'＿'; }
  m_max = resultMax;
  m_pushedIds.assign(m_max, -1);
  m_mojiModes.assign(m_max, 0);
  MenuParts* menuParts;
  for (int k = 0; k < num; k++) {
    menuParts = nullptr;
    if (m_mojis[k] != C_NA) { menuParts = MakeNameButton(m_bpos, k, m_root); m_uol->Add(menuParts); m_uol->Add(menuParts->gameObject); }
    m_chars.push_back(menuParts);
  }
  float num4 = 80.f * m_invScale;
  m_scrU = m_bpos.z - 85.5f + 80.f - num4;
  menuParts = (m_nobu = MenuParts::CreateButton(4.f, num4, 0, 0, 54, Color::white));
  menuParts->Build(7, Vector3(177.5f, m_scrU - 0.5f, 10.f), transform());
  BoxCollider* component = m_nobu->gameObject->GetComponent<BoxCollider>();
  Vector3 center = component->center; center.x += component->size.x * 0.5f; component->center = center;
  center = component->size; center.x *= 2.75f; component->size = center;
  menuParts->transform()->localScale = Vector3(0.68f, 1.f, 1.f);
  menuParts->gameObject->layer = 10;
  m_parts.push_back(menuParts);
  m_tail = 82.f * (float)(num / 5) - m_cam->orthographicSize * 2.f;
  m_text = m_upframe->AddTextMesh(Vector3(225.f, 4.f, 709.f), 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter)->GetComponent<TextMesh>();
  if (GetLangages::m_langagesNumber == 2) { m_text->transform()->localPosition = Vector3(214.f, 4.f, 709.f); m_text->tabSize = 4.f; }
  else { m_text->transform()->localPosition = Vector3(222.f, 4.f, 709.f); m_text->tabSize = 6.f; }
  m_text->text = MakeDispName(std::u16string());

  auto makeEtc = [&](float w, float h, short id) { MenuParts* p = MenuParts::CreateButton(w, h, id, 0); return p; };
  menuParts = makeEtc(166.f, 29.f, 1);
  menuParts->Build(4, Vector3(123.5f, 8.f, 616.f), m_upframe->transform());
  menuParts->AddTextMesh(Vector3(0.f, 4.f, 0.f), 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
  menuParts->m_list[1]->m_stringChanger(m_etcText[0]);
  m_etc.push_back(menuParts);
  menuParts = makeEtc(166.f, 29.f, 2);
  menuParts->Build(4, Vector3(328.f, 8.f, 616.f), m_upframe->transform());
  menuParts->AddTextMesh(Vector3(0.f, 4.f, 0.f), 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
  menuParts->m_list[1]->m_stringChanger(m_etcText[1]);
  m_etc.push_back(menuParts);
  const float bx[3] = { 60.f, 150.f, 240.f };
  for (int i = 0; i < 3; i++) {
    menuParts = makeEtc(36.f, 36.f, (short)(3 + i));
    menuParts->Build(4, 6, bx[i], 8.f, 123.f);
    menuParts->AddTextMesh(Vector3(0.f, 4.f, 0.f), 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
    menuParts->m_list[1]->m_stringChanger(m_etcText[2 + i]);
    m_etc.push_back(menuParts);
  }
  menuParts = makeEtc(68.f, 36.f, 6);
  menuParts->Build(4, 6, 375.f, 8.f, 123.f);
  menuParts->AddTextMesh(Vector3(0.f, 4.f, 0.f), 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
  menuParts->m_list[1]->m_stringChanger(m_etcText[5]);
  m_etc.push_back(menuParts);
  menuParts = makeEtc(68.5f, 23.5f, 7);
  menuParts->Build(4, 7, 96.f, 4.f, 34.f);
  menuParts->AddTextMesh(Vector3(0.f, 4.f, 0.f), 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
  menuParts->m_list[1]->m_stringChanger(m_etcText[6]);
  menuParts->SetEnableButtonTextColorChange();
  m_etc.push_back(menuParts);
  menuParts->m_list[0]->m_intChanger(m_result.empty() ? 3 : 0);
  menuParts = makeEtc(32.f, 23.5f, 8);
  menuParts->ChangeParts(3, true);
  Material* nowMaterial = menuParts->m_maker->GetNowMaterial();
  menuParts->AddIcon(4, 0.f, 4.f, 0.f, 11, false, 1.3f);
  menuParts->Build(4, 6, 412.5f, 4.f, 34.f);
  menuParts->m_button->AddChangeGrayMaterials({ nowMaterial });
  m_etc.push_back(menuParts);
  if (lng == 1) menuParts->m_list[0]->m_intChanger(3);
  for (MenuParts* item : m_etc) { item->transform()->SetParent(transform()); m_uol->Add(item); m_uol->Add(item->gameObject); }
  for (MenuParts* part : m_parts) { m_uol->Add(part); m_uol->Add(part->gameObject); }
  m_uol->Add(m_root->gameObject);
  m_uol->Add(gameObject);
  SetMode(0);
  if (m_boot) UiCameras::RetagFrom(transform(), 8, UiCameras::kBootLayer);     // landscape start flow: layer-8 parts follow the centred camera
}

void InputName::SetVisible(bool bShow) {
  m_status = !bShow ? 9 : 0;
  for (MenuParts* p : m_parts) p->SetVisible(bShow);
  for (MenuParts* c : m_chars) if (c) c->SetVisible(bShow);
  for (MenuParts* e : m_etc) if (e) e->SetVisible(bShow);
}

void InputName::SetMode(int mode) {
  m_mojis = MojiSet(m_mode = mode); m_mojisLen = 70;
  m_etc[m_mode]->m_list[0]->m_intChanger(2);
  m_etc[(m_mode == 0) ? 1 : 0]->m_list[0]->m_intChanger(0);
  if (GetLangages::m_langagesNumber != 1) { m_etc[0]->m_list[0]->m_intChanger(2); m_etc[1]->m_list[0]->m_intChanger(2); }
  m_etc[3]->m_list[0]->m_intChanger(2);
  m_etc[4]->m_list[0]->m_intChanger(2);
  m_upframe->m_list[0]->m_intChanger(287 + mode);
  int num = m_mojisLen >> m_shiftNum;
  int num2 = m_bShift ? num : 0;
  for (int i = 0; i < num; i++) {
    if (m_chars[i]) {
      if (m_mojis[i] == C_NA) { m_chars[i]->m_list[0]->m_intChanger(2); m_chars[i]->m_list[1]->m_stringChanger(std::string()); }
      else { m_chars[i]->m_list[0]->m_intChanger(0); m_chars[i]->m_list[1]->m_stringChanger(ToUtf8(std::u16string(1, m_mojis[num2 + i]))); }
    }
  }
  if (m_boot) UiCameras::RetagFrom(transform(), 8, UiCameras::kBootLayer);     // landscape start flow: layer-8 parts follow the centred camera
}

void InputName::Setup(UnityOBJList& uol) {
  float aspect = Layout::Aspect(Camera::main()->aspect());
  float num = 1.7777778f * aspect;
  float num2 = 578.5f - eIndent::SCREEN_HALF_HEIGHT;
  float z = 178.5f - eIndent::SCREEN_HALF_HEIGHT;
  m_uol = &uol;
  m_invScale = 1.f / num;
  m_invScale *= m_invScale;
  float num3 = -370.f + 370.f * m_invScale;
  float num4 = 196.f - 196.f * m_invScale;
  num2 += num3;
  transform()->setPosition(Vector3(0.f, 100.f, 0.f));
  transform()->Rotate(90.f, 0.f, 0.f);
  m_cam = gameObject->AddComponent<Camera>();
  m_cam->orthographic = true;
  m_boot = UiCameras::BootPhase();
  m_cam->rect = m_boot ? UiCameras::BootRect() : Layout::UiRect();
  m_uiCam = m_boot ? UiCameras::Boot() : TouchPad::m_camera;
  float num5 = (m_cam->orthographicSize = 225.f * (1.f / aspect));
  m_cam->cullingMask = 1024;
  m_cam->depth = 2.f;
  m_cam->clearFlags = CameraClearFlags::Depth;
  num2 = m_cam->WorldToScreenPoint(Vector3(0.f, 0.f, num2)).y;
  z = m_cam->WorldToScreenPoint(Vector3(0.f, 0.f, z)).y;
  Rect pixelRect = m_cam->pixelRect();
  pixelRect.y = z;
  pixelRect.height = num2 - z;
  m_cam->setPixelRect(pixelRect);
  num5 = (m_cam->orthographicSize = num5 * m_cam->rect.height);
  m_uol->Add(m_cam);
  m_bpos.z += num5;
  m_scrD = 0.f - num5 + 10.f;
  m_areaM = num5 - 5.f;
  MenuParts* menuParts = (m_upframe = MenuParts::CreateBuildFrameParts(287, 6, 0.f, 0.f, -2.f + num3));
  menuParts->transform()->SetParent(transform());
  m_parts.push_back(menuParts);
  menuParts = MenuParts::CreateBuildFrameParts(286, 6, 0.f, 0.f, -2.f + num4);
  menuParts->transform()->localScale = Vector3(1.f, 1.f, m_invScale);
  menuParts->transform()->SetParent(transform());
  m_parts.push_back(menuParts);
  menuParts = MenuParts::CreateBuildFrameParts(285, 6, 0.f, 0.f, -2.f);
  menuParts->transform()->SetParent(transform());
  m_parts.push_back(menuParts);
  m_root = (new GameObject())->transform;
  m_root->SetParent(transform());
  m_port->SetTableForCode(0, [this](short lng) { SetLanguage(lng >> 8, lng & 0xFF); });
  m_port->SetTableForCode(1, [this](short prm) {
    SetVisible(prm != 0);
    prm = (prm == 0) ? (short)1 : (short)0;
    m_port->SetItemCode(3, prm);
    m_port->SetItemCode(4, prm);
  });
  if (m_boot) UiCameras::RetagFrom(transform(), 8, UiCameras::kBootLayer);     // landscape start flow: layer-8 parts (back/OK buttons...) follow the centred camera
}
