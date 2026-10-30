// game/MenuPortItem.hpp -- OWNER: WP4a.  Port of MenuPortItem.cs: a "screen" (category-1 port) = a list of MenuParts + the id tables that map native item
// ids onto them.  The 68 concrete screens are built by MenuPortItemTable (WP4b) through AddParts / SetTableFor*.
#pragma once
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "game/MenuParts.hpp"
#include "game/MenuPort.hpp"
#include "miniunity.hpp"
using namespace miniunity;

class MenuPortItem : public MenuPort {
public:
  using FUNC_SetCode = std::function<void(short)>;

  static constexpr short IUC_None = 0, IUC_setup = 1, IUC_setItemMenu = 2, IUC_setDirectButton = 3, IUC_setActive = 4, IUC_drawRequest = 5, IUC_inputRequest = 6, IUC_Stop = 7,
                         IUC_ignoreDrawRequset = 8, IUC_setItemParamInt = 10, IUC_setItemParamString = 11, IUC_setItemCode = 12, IUC_drawCancelRequest = 13,
                         IUC_MenuDebugButtonName = 100, IUC_MenuDebugMisc = 101, OUC_None = 0, OUC_Input = 1;
  static constexpr short PAD_NONE = 0, PAD_U = 1, PAD_D = 2, PAD_L = 4, PAD_R = 8, PAD_A = 16, PAD_B = 32, PAD_X = 64, PAD_Y = 128;

  short m_type;
  short m_menuItemWidth = 0, m_menuItemHeight = 0, m_menuItemNum = 0;
  bool m_bPad = false, m_bCancel = false, m_bDirectPad = false, m_bActive = false;
  short m_active = -1;
  std::vector<MenuParts*> m_partsList;
  std::vector<MenuParts*> m_buttonList;
  bool m_objectActive = false, m_enableInput = false, m_bHit = false, m_bForuceDrawRequest = false, m_bDraw = false, m_bDrawRequest = false, m_bIgnoreDrawRequset = false;

  MenuPortItem(short port, short type);

  short AddParts(MenuParts* parts, bool bForceButton = false);
  void SetTableForCode(short id, short partsNo);                    // visibility toggle of parts #partsNo
  void SetTableForCode(short id, FUNC_SetCode func);
  virtual void SetItemCode(short id, short code);
  void SetTableForItemParam(short id, short no, int partsNo, int baseNo);
  void SetTableForItemParam(short id, short no, MenuParts::IBasePtr ibase);
  void SetTableForItemParamTextColorChanger(short id, short no, MenuParts* parts);
  virtual void SetItemParam(short id, short no, int data);
  virtual void SetItemParam(short id, short no, std::string str);

  void DebugFill();                                  // dev aid (UiSelfTest tour): show every visibility-code part and put sample text in every text item
  void Close() override;
  void SetVisible(bool bShow) override;
  void OnGUI() override {}
  void NoticeReceive(Param prm) override;
  void Update(int pushState, Ray& ray) override;

protected:
  std::vector<short> m_visCodes;                     // item codes registered through SetTableForCode(id, partsNo) (pure visibility toggles)
  std::map<short, FUNC_SetCode> m_codeControl;
  std::map<int, MenuParts::IBasePtr> m_partsControl;
private:
  MenuParts* m_pushing = nullptr;
};
