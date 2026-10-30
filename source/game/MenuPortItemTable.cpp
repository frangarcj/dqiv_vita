// game/MenuPortItemTable.cpp -- OWNER: WP4b (framework + proof screens by WP4a).  C# source: re/cs_out/Assembly-CSharp/MenuPortItemTable.cs
// Ported here: the table itself + CreateMenuPortItem, and screens TitleTouch, StartMenuFirst, StartMenuSecond, RootMenu, YesNo, Money, Chapter, ScopeBack, Pageing
// (StartMenuSelect, SaveOverwrite, + Sub_BackButton / Sub_RootMenuButton / Sub_StartMenuButtonLong). Everything else: MenuPortItemTable_unported.cpp.
#include "game/MenuPortItemTable.hpp"
#include <algorithm>
#include "utils/logger.h"

short MenuPortItemTable::s_yesNoPort = 0;

// the native screen id (MenuPort::Open type) indexes this table; ids that share a function differ by their offset from the first occurrence (the `type` argument)
static const MenuPortItemTable::Make_FUNC makeFuncList[102] = {
  MenuPortItemTable::Make_Debug, // 0
  MenuPortItemTable::Make_YesNo,
  MenuPortItemTable::Make_Money,
  MenuPortItemTable::Make_RootMenu,
  MenuPortItemTable::Make_LeftCharaList,
  MenuPortItemTable::Make_TargetCharaList,
  MenuPortItemTable::Make_Pageing,
  MenuPortItemTable::Make_MagicInfo,
  MenuPortItemTable::Make_NameButton,
  MenuPortItemTable::Make_ItemList,
  MenuPortItemTable::Make_ItemInfo, // 10
  MenuPortItemTable::Make_BCInfo,
  MenuPortItemTable::Make_Chapter,
  MenuPortItemTable::Make_Status,
  MenuPortItemTable::Make_Status,
  MenuPortItemTable::Make_Status,
  MenuPortItemTable::Make_Status,
  MenuPortItemTable::Make_Status,
  MenuPortItemTable::Make_OperationItemList,
  MenuPortItemTable::Make_Senreki,
  MenuPortItemTable::Make_MemberShift, // 20
  MenuPortItemTable::Make_MemberShiftButton,
  MenuPortItemTable::Make_MapButton,
  MenuPortItemTable::Make_MapButton,
  MenuPortItemTable::Make_ShopMap,
  MenuPortItemTable::Make_CityMap,
  MenuPortItemTable::Make_WorldLargeMap,
  MenuPortItemTable::Make_Config,
  MenuPortItemTable::Make_MonsterEncyclopedia,
  MenuPortItemTable::Make_BattleUpStatus,
  MenuPortItemTable::Make_BattleRoot, // 30
  MenuPortItemTable::Make_MonsterName,
  MenuPortItemTable::Make_BattleAction,
  MenuPortItemTable::Make_BattleTarget,
  MenuPortItemTable::Make_BattleGo,
  MenuPortItemTable::Make_BattleIM,
  MenuPortItemTable::Make_BattleIM,
  MenuPortItemTable::Make_BattleIM,
  MenuPortItemTable::Make_BattleInfo,
  MenuPortItemTable::Make_BattleEnemyTarget,
  MenuPortItemTable::Make_BattlePartyTarget, // 40
  MenuPortItemTable::Make_BattleChangeButton,
  MenuPortItemTable::Make_BattleChangeInfo,
  MenuPortItemTable::Make_BattleHoppingNumber,
  MenuPortItemTable::Make_Casino_CoinShop,
  MenuPortItemTable::Make_Casino_GiftList,
  MenuPortItemTable::Make_Casino_GiftTarget,
  MenuPortItemTable::Make_Casino_Slot,
  MenuPortItemTable::Make_Casino_Poker,
  MenuPortItemTable::Make_Casino_FightStadiumBet,
  MenuPortItemTable::Make_Casino_FightStadiumSelect, // 50
  MenuPortItemTable::Make_Casino_FightStadiumCoin,
  MenuPortItemTable::Make_StartMenuFirst,
  MenuPortItemTable::Make_StartMenuSecond,
  MenuPortItemTable::Make_StartMenuSelect,
  MenuPortItemTable::Make_StartNameEntry,
  MenuPortItemTable::Make_SaveOverwrite,
  MenuPortItemTable::Make_ShopRoot,
  MenuPortItemTable::Make_ShopBuy,
  MenuPortItemTable::Make_ShopBuy,
  MenuPortItemTable::Make_ShopBuyInfo, // 60
  MenuPortItemTable::Make_ShopBuyInfo,
  MenuPortItemTable::Make_ShopSellItemList,
  MenuPortItemTable::Make_ShopSellItemInfo,
  MenuPortItemTable::Make_BankDeposit,
  MenuPortItemTable::Make_BankDraw,
  MenuPortItemTable::Make_ChurchRoot,
  MenuPortItemTable::Make_ChurchWork,
  MenuPortItemTable::Make_MessageStatus,
  MenuPortItemTable::Make_MuseumMemo,
  MenuPortItemTable::Make_MuseumInfo, // 70
  MenuPortItemTable::Make_FightStadiumAbort,
  MenuPortItemTable::Make_ViewVolumeDecision,
  MenuPortItemTable::Make_TitleTouch,
  MenuPortItemTable::Make_PortalButton,
  MenuPortItemTable::Make_NetworkRoot,
  MenuPortItemTable::Make_NetworkRoot,
  MenuPortItemTable::Make_NetworkPLST,
  MenuPortItemTable::Make_NetworkCODE,
  MenuPortItemTable::Make_NetworkITEM,
  MenuPortItemTable::Make_License, // 80
  MenuPortItemTable::Make_MonG_Root,
  MenuPortItemTable::Make_MonG_Button,
  MenuPortItemTable::Make_MonsG_Name,
  MenuPortItemTable::Make_MonsG_MessageUpChar,
  MenuPortItemTable::Make_LuidaRoot,
  MenuPortItemTable::Make_SugorokuRoot,
  MenuPortItemTable::Make_Sugoroku_Arrow,
  MenuPortItemTable::Make_Sugoroku_Dice,
  MenuPortItemTable::Make_Sugoroku_Timer,
  MenuPortItemTable::Make_Sugoroku_Shop, // 90
  MenuPortItemTable::Make_SugorokuDrop,
  MenuPortItemTable::Make_ScopeBack,
  MenuPortItemTable::Make_SlimeTouch_Root,
  MenuPortItemTable::Make_SlimeTouch_Score,
  MenuPortItemTable::Make_SlimeRace_BetA,
  MenuPortItemTable::Make_SlimeRace_BetB,
  MenuPortItemTable::Make_SlimeRace_Coin,
  MenuPortItemTable::Make_SlimeRace_MoneyCoin,
  MenuPortItemTable::Make_SlimeRace_Entry,
  MenuPortItemTable::Make_SlimeRace_Result, // 100
  MenuPortItemTable::Make_SlimeRace_BetC,
};

MenuPortItem* MenuPortItemTable::CreateMenuPortItem(short port, short type) {
  if (102 <= type || type < 0) {
    Debug::Log("No Menu Type : %d", (int)type);
    type = 0;
  }
  int first = (int)(std::find(makeFuncList, makeFuncList + 102, makeFuncList[type]) - makeFuncList);
  l_info("MenuPortItemTable: port %d screen id %d -> table slot %d (subtype %d)", (int)port, (int)type, first, (int)(type - first));
  return makeFuncList[type](port, (short)(type - first));
}

// MenuPortItemDebug (the debug GUI) is not ported: an empty item keeps the port alive
MenuPortItem* MenuPortItemTable::Make_Debug(short port, short type) { return new MenuPortItem(port, type); }

MenuPortItem* MenuPortItemTable::Make_Chapter(short port, short type) {
  MenuPortItem* menuPortItem = new MenuPortItem(port, type);
  MenuParts* p = MenuParts::CreateFromTexture("Menu/map/hyodai1", true);
  MenuParts* parts = MenuParts::CreateBuildTextMenu(4, 0.f, 1.f, 10.f, nullptr, 10.f, TextAlignment::Center, TextAnchor::UpperCenter, 2.f);
  p->Build(4, Vector3::zero);
  menuPortItem->AddParts(parts);
  if (GetLangages::m_langagesNumber == 1) {
    parts = MenuParts::CreateBuildTextMenu(4, 0.f, 1.f, -2.5f, nullptr, 10.f, TextAlignment::Center, TextAnchor::MiddleCenter, 1.25f);
    menuPortItem->AddParts(parts);
  }
  menuPortItem->AddParts(p);
  menuPortItem->SetTableForItemParam(0, 0, 0, 0);
  if (GetLangages::m_langagesNumber == 1) menuPortItem->SetTableForItemParam(0, 1, 1, 0);
  menuPortItem->SetTableForCode(0, [p](short code) { p->gameObject->setActive(code != 0); });
  return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_YesNo(short port, short type) {
  MenuPortItem* menuPortItem = new MenuPortItem(s_yesNoPort = port, type);
  MenuParts* waku = MenuParts::Create(4);
  waku->AddSquare(Vector3::zero, 0, true, 110.f, 24.f);
  waku->AddSquare(waku->CalcBoundsPosition(2, 1.f, 0.f, 0.f), 0, true, 110.f, 24.f);
  waku->Build(1, eIndent::CalcPosition(MenuMessage::m_window->gameObject, 7, 0.f, 0.f, 1.f));
  menuPortItem->AddParts(waku);
  MenuParts* menuParts = MenuParts::CreateButton(90.f, 4.f, 0, 16);
  menuParts->Build(4, Vector3(-67.5f, 4.f, -24.5f), waku->transform());
  menuParts->AddTextMesh(Vector3(0.f, 6.f, 1.f), 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
  menuPortItem->AddParts(menuParts);
  MenuParts* menuParts2 = MenuParts::CreateButton(90.f, 4.f, 1, 16);
  menuParts2->Build(4, Vector3(67.5f, 4.f, -24.5f), waku->transform());
  menuParts2->AddTextMesh(Vector3(0.f, 6.f, 1.f), 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
  menuPortItem->AddParts(menuParts2);
  menuPortItem->SetTableForItemParam(0, 0, 1, 1);
  menuPortItem->SetTableForItemParam(1, 0, 2, 1);
  menuPortItem->SetTableForCode(0, [waku](short code) { waku->transform()->setPosition(eIndent::CalcPosition(MenuMessage::m_window->gameObject, 7, 0.f, 0.f, 1.f)); });
  return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_Money(short port, short type) {
  MenuPortItem* menuPortItem = new MenuPortItem(port, type);
  MenuParts* p = MenuParts::CreateWindow0(148.f, 35.f);
  p->Build(2, 2, 2.f, 0.f, 2.f);
  p->AddTextMesh(Vector3(-12.f, 4.f, -20.f), 9.5f, TextAlignment::Right, TextAnchor::UpperRight);
  short partsNo = menuPortItem->AddParts(p);
  menuPortItem->SetTableForItemParam(0, 0, partsNo, 1);
  menuPortItem->SetTableForCode(1, [p](short code) {
    int indent;
    float z;
    switch (code) {
      default: indent = 2; z = 2.f; break;
      case 1: indent = 8; z = 574.5f; break;
      case 2: indent = 2; z = -242.5f; break;
    }
    p->transform()->setPosition(eIndent::CalcScreenPosition(indent, 2.f, 0.f, z));
  });
  return menuPortItem;
}

MenuParts* MenuPortItemTable::Sub_BackButton(MenuPortItem* item, bool bExternalToggle) {
  MenuParts* menuParts = MenuParts::CreateButton(32.f, 23.5f, -1, (short)(!bExternalToggle ? 32 : 1024));
  menuParts->m_externalToggle = bExternalToggle;
  menuParts->ChangeParts(3);
  menuParts->AddIcon(4, 0.f, 4.f, 0.f, 11, false, 1.3f);
  menuParts->Build(4, 6, 412.5f, 4.f, 34.f);
  item->AddParts(menuParts);
  return menuParts;
}

MenuPortItem* MenuPortItemTable::Make_TitleTouch(short port, short type) {
  MenuPortItem* menuPortItem = new MenuPortItem(port, type);
  TapToStart* tap = TapToStart::Create();
  menuPortItem->AddParts(tap);
  menuPortItem->SetTableForCode(0, [tap](short code) { tap->CountReset(); });
  return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_ScopeBack(short port, short type) {
  MenuPortItem* menuPortItem = new MenuPortItem(port, type);
  MenuParts* menuParts = MenuParts::CreateWindow0(121.25f, 44.f);
  MenuParts* menuParts2 = MenuParts::CreateButton(100.f, 24.f, 0, 512);
  menuParts2->m_externalToggle = true;
  menuParts->Build(8, 8, 0.f, 0.f, 41.f);
  menuPortItem->AddParts(menuParts);
  menuParts2->Build(4, 7, 152.5f, 4.f, 75.f, menuParts->transform());
  menuParts2->AddTextMesh(Vector3(0.f, 4.f, 0.f), 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
  short partsNo = menuPortItem->AddParts(menuParts2);
  menuPortItem->SetTableForItemParam(0, 0, partsNo, 1);
  return menuPortItem;
}

void MenuPortItemTable::Sub_RootMenuButton(MenuPortItem* item, short id, short buttonId, int icon, Transform* parent, float x, float y, float z, bool bFix) {
  float size = !bFix ? 0.86f : 1.2f;
  int num = bFix ? 8 : 0;
  int num2 = !bFix ? 14 : 7;
  MenuParts* menuParts = MenuParts::CreateButton(77.f, 40.f, buttonId, 16);
  menuParts->ChangeParts(3);
  menuParts->AddIcon(4, (float)num, 4.f, (float)num2, icon, false, size);
  menuParts->Build(0, 8, x, y, z, parent);
  menuParts->AddTextMesh(eIndent::CalcLocalPosition(menuParts->gameObject, 7, 0.f, 4.f, 8.f), 9.5f, TextAlignment::Center, TextAnchor::LowerCenter);
  short partsNo = item->AddParts(menuParts);
  item->SetTableForCode(id, partsNo);
  item->SetTableForItemParam(id, 0, partsNo, 1);
  item->SetTableForItemParam(id, 1, partsNo, 2);
}

MenuPortItem* MenuPortItemTable::Make_RootMenu(short port, short type) {
  MenuPortItem* menuPortItem = new MenuPortItem(port, type);
  MenuParts* menuParts = Sub_BackButton(menuPortItem);
  Sub_RootMenuButton(menuPortItem, 0, 2, 9, menuParts->transform(), -267.f, 4.f, 235.f);
  Sub_RootMenuButton(menuPortItem, 1, 1, 12, menuParts->transform(), -137.f, 4.f, 235.f);
  Sub_RootMenuButton(menuPortItem, 2, 4, 18, menuParts->transform(), -267.f, 4.f, 151.f, true);
  Sub_RootMenuButton(menuPortItem, 3, 5, 7, menuParts->transform(), -137.f, 4.f, 151.f);
  return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_Pageing(short port, short type) {
  MenuPortItem* menuPortItem = new MenuPortItem(port, type);
  MenuParts* text = MenuParts::CreateBuildTextMenu(8, -146.f, 8.f, 99.f, nullptr, 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
  menuPortItem->AddParts(text);
  menuPortItem->SetTableForCode(0, [text](short code) { text->gameObject->SetActiveRecursively(code != 0); });
  menuPortItem->SetTableForItemParam(0, 0, 0, 0);
  MenuParts* menuParts = MenuParts::CreateButtonIcon(10, 0, 1.f, -1, 4);
  menuParts->Build(4, 8, -235.5f, 4.f, 99.f, text->transform());
  menuPortItem->AddParts(menuParts);
  menuParts = MenuParts::CreateButtonIcon(10, 1, 1.f, -1, 8);
  menuParts->Build(4, 8, -56.2f, 4.f, 99.f, text->transform());
  menuPortItem->AddParts(menuParts);
  return menuPortItem;
}

void MenuPortItemTable::Sub_StartMenuButtonLong(MenuPortItem* item, Transform* parent, short bid, float yPos) {
  MenuParts* menuParts = MenuParts::CreateButton(390.f, 18.f, bid, 16);
  menuParts->Build(4, 7, 0.f, 4.f, yPos, parent);
  menuParts->AddTextMesh(Vector3(0.f, 4.f, 0.f), 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
  short partsNo = item->AddParts(menuParts);
  item->SetTableForItemParam(bid, 0, partsNo, 0);
  item->SetTableForItemParam(bid, 1, partsNo, 1);
  menuParts->SetEnableButtonTextColorChange();
}

MenuPortItem* MenuPortItemTable::Make_StartMenuFirst(short port, short type) {
  MenuPortItem* menuPortItem = new MenuPortItem(port, type);
  MenuParts* menuParts = MenuParts::CreateBuildFrameParts(214, 6, 0.f, 0.f, -2.f);
  menuPortItem->AddParts(menuParts);
  Sub_StartMenuButtonLong(menuPortItem, menuParts->transform(), 0, 317.f);
  return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_StartMenuSecond(short port, short type) {
  MenuPortItem* item = new MenuPortItem(port, type);
  MenuParts* frame = MenuParts::CreateBuildFrameParts(-1, 6, 0.f, 0.f, -2.f, true);
  item->AddParts(frame);
  Sub_StartMenuButtonLong(item, frame->transform(), 4, 372.f);
  for (short num = 0; num < 4; num++) Sub_StartMenuButtonLong(item, frame->transform(), num, CalcStartButtonYpos(num));
  MenuParts* menuParts = MenuParts::CreateButton(32.f, 23.5f, -1, 32);
  menuParts->ChangeParts(3);
  menuParts->AddIcon(4, 0.f, 4.f, 0.f, 21, false, 1.3f);
  menuParts->Build(4, 6, 412.5f, 4.f, 34.f);
  item->AddParts(menuParts);
  MenuParts* menuParts2 = MenuParts::CreateButton(129.f, 23.5f, 8, 16);
  menuParts2->Build(4, 7, 66.5f, 4.f, 34.f, frame->transform());
  menuParts2->AddTextMesh(Vector3(0.f, 4.f, 0.f), 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
  short p2 = item->AddParts(menuParts2);
  item->SetTableForItemParam(5, 0, p2, 1);
  item->SetTableForCode(0, [item, frame](short code) {
    if (code == 0) {
      frame->m_list[0]->m_intChanger(216);
      item->m_partsList[1]->gameObject->SetActiveRecursively(false);
    } else {
      frame->m_list[0]->m_intChanger(218);
    }
  });
  MenuParts* menuParts3 = MenuParts::CreateButton(82.f, 23.5f, 11, 16);
  menuParts3->Build(4, 6, 62.5f, 4.f, 34.f, frame->transform());
  menuParts3->AddTextMesh(Vector3(0.f, 4.f, 0.f), 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
  short p3 = item->AddParts(menuParts3);
  item->SetTableForItemParam(6, 0, p3, 1);
  return item;
}

static void Sub_StartSelectButton(MenuPortItem* item, Transform* parent, short i, float y) {
  MenuParts* p = MenuParts::CreateButton(390.f, 18.f, i, 16);
  GameObject* go[2];
  p->Build(4, 7, 0.f, 4.f, y, parent);
  go[0] = p->AddTextMesh(Vector3(-205.5f, 4.f, -1.f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft);
  switch (GetLangages::m_langagesNumber) {
    case 1: go[1] = p->AddTextMesh(Vector3(-6.f, 4.f, -1.f), 8.5f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.1f); break;
    case 2: go[1] = p->AddTextMesh(Vector3(-6.f, 4.f, -1.f), 7.5f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.1f); break;
    default: go[1] = p->AddTextMesh(Vector3(-14.f, 4.f, -1.f), 7.5f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.1f); break;
  }
  short partsNo = item->AddParts(p);
  item->SetTableForItemParam(i, 0, partsNo, 0);
  item->SetTableForItemParam(i, 1, partsNo, 1);
  item->SetTableForItemParam(i, 2, partsNo, 2);
  GameObject* g0 = go[0]; GameObject* g1 = go[1];
  item->SetTableForCode(i, [p, g0, g1](short code) {
    switch (code) {
      case 30:
        p->transform()->localScale = Vector3::one;
        g0->transform->localScale = Vector3::one;
        g1->transform->localScale = Vector3::one;
        break;
      case 31: {
        Vector3 localScale(1.f, 0.6896551f, 1.f);
        p->transform()->localScale = Vector3(1.f, 1.f, 1.45f);
        g0->transform->localScale = localScale;
        g1->transform->localScale = localScale;
        break;
      }
      default: p->m_button->m_intChanger(0 + code); break;
    }
  });
  p->SetEnableButtonTextColorChange();
}

MenuPortItem* MenuPortItemTable::Make_SaveOverwrite(short port, short type) {
  MenuPortItem* menuPortItem = new MenuPortItem(port, type);
  MenuParts* menuParts = MenuParts::Create(4);
  menuParts->AddSquare(Vector3::zero, 0, true, 420.f, 44.f);
  menuParts->Build(4, 4, 0.f, 0.f, 14.f);
  menuParts->AddTextMesh(Vector3(-205.5f, 4.f, 0.5f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft);
  switch (GetLangages::m_langagesNumber) {
    case 1: menuParts->AddTextMesh(Vector3(-6.f, 4.f, 0.5f), 8.5f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.1f); break;
    case 2: menuParts->AddTextMesh(Vector3(-6.f, 4.f, 0.5f), 7.5f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.1f); break;
    default: menuParts->AddTextMesh(Vector3(-14.f, 4.f, 0.5f), 7.5f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.1f); break;
  }
  menuPortItem->AddParts(menuParts);
  menuPortItem->SetTableForItemParam(0, 0, 0, 1);
  menuPortItem->SetTableForItemParam(0, 1, 0, 2);
  return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_StartMenuSelect(short port, short type) {
  MenuPortItem* item = new MenuPortItem(port, type);
  MenuParts* frame = MenuParts::CreateBuildFrameParts(-1, 6, 0.f, 0.f, -2.f);
  Transform* title = frame->AddTextMesh(Vector3(117.5f, 4.f, 283.f), 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter)->transform;
  short framePartsNo = item->AddParts(frame);
  item->SetTableForItemParam(3, 0, framePartsNo, 1);
  for (short num = 0; num < 3; num++) Sub_StartSelectButton(item, frame->transform(), num, CalcStartButtonYpos((short)(num + 1)));
  Sub_BackButton(item)->transform()->SetParent(frame->transform());
  item->SetTableForCode(3, [item, frame, title](short code) {
    int prm = 219 + code;
    frame->m_list[0]->m_intChanger(prm);
    if (code != 2) {
      Vector3 localPosition = item->m_buttonList[0]->transform()->localPosition;
      localPosition.z = 217.f;
      item->m_buttonList[0]->transform()->localPosition = localPosition;
      for (int i = 1; i < 3; i++) item->m_buttonList[i]->gameObject->SetActiveRecursively(true);
      title->localPosition = Vector3(117.5f, 4.f, 283.f + MenuParts::BIAS_FONT_Y);
    } else {
      Vector3 localPosition2 = item->m_buttonList[0]->transform()->localPosition;
      localPosition2.z = 135.75f;
      item->m_buttonList[0]->transform()->localPosition = localPosition2;
      for (int j = 1; j < 3; j++) item->m_buttonList[j]->gameObject->SetActiveRecursively(false);
      title->localPosition = Vector3(148.f, 4.f, 226.5f + MenuParts::BIAS_FONT_Y);
    }
  });
  return item;
}
