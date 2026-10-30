// game/InputName.hpp -- OWNER: WP4b (New Game path).  Port of InputName.cs: the on-screen name entry keyboard of screen StartNameEntry (id 55).
// C# source: re/cs_out/Assembly-CSharp/InputName.cs.  Deviations: the Japanese / campaign-code tables (C_HIRA, C_KATA, C_CAMPAIGN_*) are dead code in the C#
// (SetLanguage always selects C_LOCALIZE = alphabet + accents) and are not ported; strings are UTF-16 (std::u16string) like the C# `string`, converted to UTF-8 for TextMesh.
#pragma once
#include <string>
#include <vector>
#include "game/MenuParts.hpp"
#include "game/MenuPortItem.hpp"
#include "miniunity.hpp"
using namespace miniunity;

class InputName : public Component {
public:
  MenuPortItem* m_port = nullptr;
  void Setup(UnityOBJList& uol);
  void SetLanguage(int lng, int resultMax);
  void SetMode(int mode);
  void Update() override;
  void ApplyAction(int state, MenuParts* hit);          // the release handler (touch) / pad actions: 1,2 mode  3 shift  4,5 accent  6 delete  7 enter  8 back  11 key `hit`
  void PadUpdate();                                      // [WP3] physical-button focus (d-pad moves, Cross presses, Square delete, Triangle shift, L/R mode, Start enter, Circle back)

  static std::string ToUtf8(const std::u16string& s);
  static std::u16string FromUtf8(const std::string& s);

private:
  UnityOBJList* m_uol = nullptr;
  std::vector<MenuParts*> m_parts, m_chars, m_etc;
  int m_mode = 0;
  Vector3 m_bpos = Vector3(-162.f, 0.f, -38.5f);
  Transform* m_root = nullptr;
  MenuParts *m_nobu = nullptr, *m_upframe = nullptr;
  TextMesh* m_text = nullptr;
  Camera* m_cam = nullptr; Camera* m_uiCam = nullptr; bool m_boot = false;     // landscape start flow: centred panel camera (UiCameras)
  float m_invScale = 0, m_tail = 0, m_scrU = 0, m_scrD = 0, m_areaM = 0;
  bool m_bPress = false, m_pushed = false;
  float m_nobuS = 0;
  MenuParts* m_hit = nullptr;
  int m_max = 4;
  std::vector<int> m_pushedIds;
  const char16_t* m_mojis = nullptr; int m_mojisLen = 0;
  int m_state = 0, m_status = 9;
  std::u16string m_result;
  int m_shiftNum = 0;
  char16_t m_blank = u' ';
  std::vector<int> m_mojiModes;
  bool m_bShift = false, m_bCampaign = false;
  std::string m_etcText[7];
  int m_padFocus = 0, m_padHold = 0;
  int m_padZone = 0;                                     // 0 = a key of the grid (m_padFocus), else the id of the focused function button (1/2 page tabs, 3 shift, 6 delete, 7 enter, 8 back)

  MenuParts* MakeNameButton(Vector3 bpos, int bid, Transform* prt);
  std::string MakeDispName(const std::u16string& str);
  void SetVisible(bool bShow);
  const char16_t* MojiSet(int mode) const;
};
