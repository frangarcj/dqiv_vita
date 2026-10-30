// game/MenuPortItemTable.hpp -- OWNER: WP4b (framework + a few proof screens by WP4a).  Port of the DQ4 MenuPortItemTable.cs: the 81 screens (ids 0..80, category-1 ports).
//
// DQ4 status: every screen of the DQ4 table is ported (no UNPORTED stubs).  Files: MenuPortItemTable.cpp (table, start/title/root/YesNo/Money/Chapter/Pageing),
// _Main (lists, status, items, members), _Battle, _Casino (casino + coliseum), _Maps, _WorldMap, _Shop (shop/bank/church/Toruneko/NENE), _Network, _License, _NewGame, _helpers.
// The table index order (makeFuncList) is the native screen id: do not reorder.  To port another C# screen: translate it into the matching file as
// `MenuPortItem* MenuPortItemTable::Make_X(short port, short type) { ... }` (tools/menu_cs2cpp.py does a first pass), declare it below, add it to makeFuncList.
//   Translation rules: `new MenuPortItem(port, type)` -> `MenuPortItem* item = new ...`, `.`->`->` on pointers, `item.SetTableForCode(id, (short code) => {..})` ->
//      `item->SetTableForCode(id, [=](short code) {..})` (capture the C# locals by value: they are pointers), `gameObject.active = b` -> `gameObject->setActive(b)`,
//      `item.Receive().ConvertString()` -> `item->Receive().ConvertString()` (UTF-8 std::string), enums `TextAlignment.Center` -> `TextAlignment::Center`,
//      `FRAME_ENUM.X` -> `FRAME_ENUM::X`, `MenuParts.s_info[n]` -> `MenuParts::s_info[n]`, `new Vector3(..)` -> `Vector3(..)`, `eIndent.CalcScreenPosition` -> `eIndent::CalcScreenPosition`.
//   Pad bits of the arrow / +- buttons are the DQ4 ones (MenuPortItem::PAD_D=1, PAD_U=2, PAD_R=4, PAD_L=8): copy them from the C#, they differ from DQ5.
#pragma once
#include "game/MenuMessage.hpp"
#include "game/MenuParts.hpp"
#include "game/MenuPortItem.hpp"
#include "game/ShakeFrame.hpp"
#include "game/TapToStart.hpp"
#include "miniunity.hpp"
using namespace miniunity;

class MenuPortItemTable {
public:
  typedef MenuPortItem* (*Make_FUNC)(short port, short type);
  static short s_yesNoPort;
  static MenuPortItem* CreateMenuPortItem(short port, short type);


  // ---- nested helper classes of the C# table (ported as encountered) ----
  class StatusIconChanger : public MenuParts::IBase {
  public:
    StatusIconChanger(Material* target, MenuParts::IBasePtr sinidoku, MenuParts::IBasePtr noroi);
  };
  struct NAME_BUTTON_PARAM { short num; float start; float diffy; };
  static const NAME_BUTTON_PARAM name_button_param[5];
  class ItemNumCTRL : public MenuParts::IBase {
  public:
    ItemNumCTRL(IntChanger num2, IntChanger num1);
  private:
    IntChanger m_num2, m_num1;
    void Changer(int value);
  };
  class ICityMap : public MenuParts::IBase {
  public:
    ICityMap(MenuPortItem* item, MenuParts* posf);
  private:
    MenuParts* m_map = nullptr;
  };


  // ---- battle helpers ----
  class BattleBar : public MenuParts::IBase {
  public:
    explicit BattleBar(std::shared_ptr<MenuParts::IBar> bar);
    void RedApply();
  private:
    std::shared_ptr<MenuParts::IBar> m_bar; int m_value = 0;
  };
  class MonsterMarkList {
  public:
    Vector3 m_pos = Vector3::zero;
    uint32_t m_used = 0;
    MenuParts* m_list[12];
    explicit MonsterMarkList(MenuPortItem* item);
    void Reset();
    void SetMark(int id, float x);
  };
  class MonsterNamePlate {
  public:
    int m_id; MonsterMarkList* m_markList; MenuPortItem* m_item; MenuParts* m_fname; TextMesh* m_text;
    Bounds m_name; float m_size[4] = { 0, 0, 0, 0 }; float m_nameYpos = 0; Vector3 m_center = Vector3::zero; float m_yPos = 0; float m_frameHeight = 0;
    MonsterNamePlate(MenuPortItem* item, MonsterMarkList* marks, int id);
  private:
    void ReceiveParameters(short code);
  };
  static bool BoundsIntersects(const Bounds& a, const Bounds& b);

  // shared helpers
  static MenuParts* Sub_BackButton(MenuPortItem* item, bool bExternalToggle = false);
  static void Sub_RootMenuButton(MenuPortItem* item, short id, short buttonId, int icon, Transform* parent, float x, float y, float z, bool bFix = false);
  static void Sub_StartMenuButtonLong(MenuPortItem* item, Transform* parent, short bid, float yPos);
  static float CalcStartButtonYpos(short no) { return 272.5f - 57.5f * (float)no; }

  static MenuParts* Sub_CharaButton(MenuPortItem* item, short no, Transform* parent, int indent, float x, float y, float z);
  static Renderer* Sub_NameButton(MenuPortItem* item, short no, Transform* parent, int start, int diffy, bool bSubText);
  static void Sub_ItemButton(MenuPortItem* item, short no, Transform* parent);
  static void Sub_ShopBuyButton(MenuPortItem* item, MenuParts* parent, float y, short bid, bool bNumButton = true);
  static void Sub_ShopCharaButton(MenuPortItem* item, MenuParts* parent, short no, float x, float y);
  static void Sub_ShopPageing(MenuPortItem* item, MenuParts* parent, short idex, short prmNo, short prmSub, short visNo);
  static void Sub_ShopItemButton(MenuPortItem* item, short no, Transform* parent);
  static Transform* Sub_ShopButton(MenuPortItem* item, MenuParts* parent, float fontSize, short bid, float x, float y, float w = 110.f);
  static void Sub_ShopAbort(MenuPortItem* item, MenuParts* parent, float textSize);
  static MenuParts* Sub_CasinoUD(MenuPortItem* item, short bid, short pad, Transform* parent, bool bUp, float x, float y, float z, float size);
  static void Sub_CasinoUD(MenuPortItem* item, short pos, short bid, MenuParts* parent, int parentIndex, bool bNoButton = false, float xdif = 0.f);
  static void Sub_CasinoBuyUD(MenuPortItem* item, short pos, short bid, MenuParts* parent, int parentIndex);
  static MenuParts* Sub_CasinoEnter(MenuPortItem* item, short bid, short pad, Transform* parent, short cid, short did);
  static MenuParts* Sub_CasinoStart(MenuPortItem* item, short bid, short pad, Transform* parent, short cid, short did);
  static void Sub_BattleUpStatus(MenuPortItem* item, short no, Transform* parent);
  static void Sub_BattleAction(MenuPortItem* item, short buttonId, int icon, Transform* parent, float x, float y, float z, int W = 136, float H = 20.f, int ix = 0);
  static Transform* Sub_BattleInfo(MenuPortItem* item, short buttonId, Transform* parent);
  static void Sub_BattleTarget(MenuPortItem* item, short buttonId, Transform* parent, float x, float y, float z);
  static void Sub_BattlePartyTarget(MenuPortItem* item, short buttonId, Transform* parent, float x, float y, float z);
  static void Sub_BattleItem(MenuPortItem* item, short buttonId, Transform* parent, float x, float y, float z);
  static void Sub_BattleTactics(MenuPortItem* item, short buttonId, Transform* parent, float x, float y, float z);
  static void Sub_BattleMagic(MenuPortItem* item, short buttonId, Transform* parent, float x, float y, float z, int w = 136);
  static GameObject* Sub_MonsterEncyclopediaButton(MenuPortItem* item, Transform* parent, short bid, float x, float y);
  // the screens, in makeFuncList order of first appearance
  static MenuPortItem* Make_Debug(short port, short type);
  static MenuPortItem* Make_YesNo(short port, short type);
  static MenuPortItem* Make_Money(short port, short type);
  static MenuPortItem* Make_RootMenu(short port, short type);
  static MenuPortItem* Make_LeftCharaList(short port, short type);
  static MenuPortItem* Make_TargetCharaList(short port, short type);
  static MenuPortItem* Make_Pageing(short port, short type);
  static MenuPortItem* Make_MagicInfo(short port, short type);
  static MenuPortItem* Make_NameButton(short port, short type);
  static MenuPortItem* Make_ItemList(short port, short type);
  static MenuPortItem* Make_ItemInfo(short port, short type);
  static MenuPortItem* Make_BCInfo(short port, short type);
  static MenuPortItem* Make_Chapter(short port, short type);
  static MenuPortItem* Make_Status(short port, short type);
  static MenuPortItem* Make_OperationItemList(short port, short type);
  static MenuPortItem* Make_Senreki(short port, short type);
  static MenuPortItem* Make_MemberShift(short port, short type);
  static MenuPortItem* Make_MemberShiftButton(short port, short type);
  static MenuPortItem* Make_MapButton(short port, short type);
  static MenuPortItem* Make_ShopMap(short port, short type);
  static MenuPortItem* Make_CityMap(short port, short type);
  static MenuPortItem* Make_WorldLargeMap(short port, short type);
  static MenuPortItem* Make_Config(short port, short type);
  static MenuPortItem* Make_MonsterEncyclopedia(short port, short type);
  static MenuPortItem* Make_BattleUpStatus(short port, short type);
  static MenuPortItem* Make_BattleRoot(short port, short type);
  static MenuPortItem* Make_MonsterName(short port, short type);
  static MenuPortItem* Make_BattleAction(short port, short type);
  static MenuPortItem* Make_BattleTarget(short port, short type);
  static MenuPortItem* Make_BattleGo(short port, short type);
  static MenuPortItem* Make_BattleIM(short port, short type);
  static MenuPortItem* Make_BattleInfo(short port, short type);
  static MenuPortItem* Make_BattleEnemyTarget(short port, short type);
  static MenuPortItem* Make_BattlePartyTarget(short port, short type);
  static MenuPortItem* Make_BattleChangeButton(short port, short type);
  static MenuPortItem* Make_BattleChangeInfo(short port, short type);
  static MenuPortItem* Make_BattleHoppingNumber(short port, short type);
  static MenuPortItem* Make_Casino_CoinShop(short port, short type);
  static MenuPortItem* Make_Casino_GiftList(short port, short type);
  static MenuPortItem* Make_Casino_GiftTarget(short port, short type);
  static MenuPortItem* Make_Casino_Slot(short port, short type);
  static MenuPortItem* Make_Casino_Poker(short port, short type);
  static MenuPortItem* Make_Casino_FightStadiumBet(short port, short type);
  static MenuPortItem* Make_Casino_FightStadiumSelect(short port, short type);
  static MenuPortItem* Make_Casino_FightStadiumCoin(short port, short type);
  static MenuPortItem* Make_StartMenuFirst(short port, short type);
  static MenuPortItem* Make_StartMenuSecond(short port, short type);
  static MenuPortItem* Make_StartMenuSelect(short port, short type);
  static MenuPortItem* Make_StartNameEntry(short port, short type);
  static MenuPortItem* Make_SaveOverwrite(short port, short type);
  static MenuPortItem* Make_ShopRoot(short port, short type);
  static MenuPortItem* Make_ShopBuy(short port, short type);
  static MenuPortItem* Make_ShopBuyInfo(short port, short type);
  static MenuPortItem* Make_ShopSellItemList(short port, short type);
  static MenuPortItem* Make_ShopSellItemInfo(short port, short type);
  static MenuPortItem* Make_BankDeposit(short port, short type);
  static MenuPortItem* Make_BankDraw(short port, short type);
  static MenuPortItem* Make_ChurchRoot(short port, short type);
  static MenuPortItem* Make_ChurchWork(short port, short type);
  static MenuPortItem* Make_Chapter2Status(short port, short type);
  static MenuPortItem* Make_TorunekoMiseban(short port, short type);
  static MenuPortItem* Make_NENE_ItemView(short port, short type);
  static MenuPortItem* Make_FightStadiumAbort(short port, short type);
  static MenuPortItem* Make_ViewVolumeDecision(short port, short type);
  static MenuPortItem* Make_TitleTouch(short port, short type);
  static MenuPortItem* Make_PortalButton(short port, short type);
  static MenuPortItem* Make_NetworkRoot(short port, short type);
  static MenuPortItem* Make_NetworkPLST(short port, short type);
  static MenuPortItem* Make_NetworkCODE(short port, short type);
  static MenuPortItem* Make_NetworkITEM(short port, short type);
  static MenuPortItem* Make_License(short port, short type);
};
