// game/MenuPortItemTable_NewGame.cpp -- OWNER: WP4b (New Game path): Make_PortalButton (id 74), Make_StartNameEntry (id 55).
#include "game/InputName.hpp"
#include "game/MenuPortItemTable.hpp"

MenuPortItem* MenuPortItemTable::Make_PortalButton(short port, short type) { return new MenuPortItem(port, type); }

MenuPortItem* MenuPortItemTable::Make_StartNameEntry(short port, short type) {
  MenuPortItem* menuPortItem = new MenuPortItem(port, type);
  MenuParts* menuParts = MenuParts::CreateBuildFrameParts(222, 6, 0.f, 0.f, -2.f);
  menuParts->AddTextMesh(Vector3(117.5f, 4.f, 382.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
  menuParts->AddTextMesh(Vector3(342.f, 4.f, 382.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
  short partsNo = menuPortItem->AddParts(menuParts);
  menuPortItem->SetTableForItemParam(0, 0, partsNo, 1);
  menuPortItem->SetTableForItemParam(1, 0, partsNo, 2);
  Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
  MenuParts* menuParts2 = MenuParts::CreateButton(120.f, 30.f, 0, 16);
  menuParts2->Build(4, Vector3(120.4f, 4.f, 212.f), menuParts->transform());
  menuParts2->AddTextMesh(Vector3(0.f, 4.f, 0.5f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
  menuPortItem->SetTableForItemParam(2, 0, partsNo = menuPortItem->AddParts(menuParts2), 1);
  menuPortItem->SetTableForCode(3, partsNo);
  menuParts2 = MenuParts::CreateButton(120.f, 30.f, 1, 16);
  menuParts2->Build(4, Vector3(331.6f, 4.f, 212.f), menuParts->transform());
  menuParts2->AddTextMesh(Vector3(0.f, 4.f, 0.5f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
  menuPortItem->SetTableForItemParam(3, 0, partsNo = menuPortItem->AddParts(menuParts2), 1);
  menuPortItem->SetTableForCode(4, partsNo);
  InputName* inputName = (new GameObject())->AddComponent<InputName>();
  inputName->m_port = menuPortItem;
  inputName->Setup(menuParts->m_uol);
  return menuPortItem;
}
