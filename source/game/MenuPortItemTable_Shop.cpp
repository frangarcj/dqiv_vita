// game/MenuPortItemTable_Shop.cpp -- OWNER: WP4b.  C# source: re/cs_out/Assembly-CSharp/MenuPortItemTable.cs (first pass by tools/menu_cs2cpp.py, then fixed by hand)
#include "game/MenuPortItemTable.hpp"
#include "game/MeshChara.hpp"
#include "game/MainLoop.hpp"
#include "utils/logger.h"
#include <cstdio>

MenuPortItem* MenuPortItemTable::Make_ShopRoot(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts;
	if (GetLangages::m_langagesNumber == 2)
	{
		menuParts = MenuParts::CreateBuildFrameParts(224, 6, 0.f, 0.f, -2.f);
		menuParts->AddTextMesh(Vector3(11.5f, 4.f, 184.f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.1f);
	}
	else
	{
		menuParts = MenuParts::CreateBuildFrameParts(223, 6, 0.f, 0.f, -2.f);
		menuParts->AddTextMesh(Vector3(61.5f, 4.f, 186.f), 9.f, TextAlignment::Left, TextAnchor::MiddleCenter);
	}
	menuPortItem->AddParts(menuParts);
	menuPortItem->SetTableForItemParam(6, 0, menuPortItem->AddParts(menuParts), 1);
	Sub_ShopButton(menuPortItem, menuParts, 9.5f, 0, -95.700005f, 110.5f);
	Sub_ShopButton(menuPortItem, menuParts, 9.5f, 1, 95.700005f, 110.5f);
	Sub_ShopAbort(menuPortItem, menuParts, 9.5f);
	return menuPortItem;
}

void MenuPortItemTable::Sub_ShopBuyButton(MenuPortItem* item, MenuParts* parent, float y, short bid, bool bNumButton) {
	MenuParts* menuParts = MenuParts::CreateButton(140.f, 14.f, bid, 16);
	menuParts->ChangeParts(0);
	menuParts->AddIcon(3, 6.f, 4.f, 0.f, 0, false, 0.75f);
	menuParts->Build(4, 6, 94.f, 4.f, y, parent->transform());
	if (GetLangages::m_langagesNumber == 1)
	{
		menuParts->AddTextMesh(Vector3(-42.f, 4.f, 0.f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft);
	}
	else
	{
		menuParts->AddTextMesh(Vector3(-34.f, 4.f, 0.f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft);
	}
	short partsNo = item->AddParts(menuParts);
	item->SetTableForCode(bid, partsNo);
	item->SetTableForItemParam(bid, 0, partsNo, 1);
	item->SetTableForItemParam(bid, 1, partsNo, 2);
	item->SetTableForItemParam(bid, 3, partsNo, 0);
	if (bNumButton)
	{
		MenuParts* menuParts2 = MenuParts::CreateButton(2.f, 14.f, bid, 8);
		menuParts2->ChangeParts(8, false, true);
		menuParts2->AddIcon(4, 0.f, 4.f, 0.f, 2, false, 0.3f);
		menuParts2->Build(4, Vector3(127.f, 4.f, 0.f), menuParts->transform());
		item->AddParts(menuParts2);
		menuParts2 = MenuParts::CreateButton(2.f, 14.f, bid, 4);
		menuParts2->ChangeParts(8, false, true);
		menuParts2->AddIcon(4, 0.f, 4.f, 0.f, 3, false, 0.3f);
		menuParts2->Build(4, Vector3(190.f, 4.f, 0.f), menuParts->transform());
		item->AddParts(menuParts2);
	}
}

MenuPortItem* MenuPortItemTable::Make_ShopBuy(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts;
	float z;
	float num;
	float num2;
	float z2;
	float lineSpace;
	if (type == 0)
	{
		menuParts = MenuParts::CreateBuildFrameParts(225, 6, 0.f, 0.f, -2.f);
		z = 472.f;
		num = 393.f;
		num2 = 58.f;
		z2 = 404.5f;
		lineSpace = 3.07f;
	}
	else
	{
		menuParts = MenuParts::CreateBuildFrameParts(226, 6, 0.f, 0.f, -2.f);
		z = 321.f;
		num = 252.f;
		num2 = 52.f;
		z2 = 263.5f;
		lineSpace = 3.07f * num2 / 58.f;
	}
	if (GetLangages::m_langagesNumber <= 2)
	{
		menuParts->AddTextMesh(Vector3(48.f, 4.f, z), 9.f, TextAlignment::Left, TextAnchor::MiddleCenter, 1.25f);
	}
	else
	{
		menuParts->AddTextMesh(Vector3(18.5f, 4.f, z), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.25f);
	}
	menuParts->AddTextMesh(Vector3(192.f, 4.f, z2), 9.f, TextAlignment::Center, TextAnchor::UpperCenter, lineSpace);
	menuParts->AddTextMesh(Vector3(252.f, 4.f, z2), 9.f, TextAlignment::Center, TextAnchor::UpperCenter, lineSpace);
	menuParts->AddTextMesh(Vector3(442.5f, 4.f, z2), 9.f, TextAlignment::Right, TextAnchor::UpperRight, lineSpace);
	short partsNo = menuPortItem->AddParts(menuParts);
	menuPortItem->SetTableForItemParam(6, 0, partsNo, 1);
	menuPortItem->SetTableForItemParam(6, 1, partsNo, 2);
	menuPortItem->SetTableForItemParam(6, 2, partsNo, 3);
	menuPortItem->SetTableForItemParam(6, 3, partsNo, 4);
	for (short num3 = 0; num3 < 6; num3++)
	{
		Sub_ShopBuyButton(menuPortItem, menuParts, num - num2 * (float)num3, num3);
	}
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
	return menuPortItem;
}

void MenuPortItemTable::Sub_ShopCharaButton(MenuPortItem* item, MenuParts* parent, short no, float x, float y) {
	MenuParts* menuParts = MenuParts::CreateButton(128.f, 69.f, no, 16);
	menuParts->ChangeParts(1);
	menuParts->AddIcon(0, 10.f, 2.f, -4.f, 0, false);
	menuParts->ChangeParts(13, true);
	Material* nowMaterial = menuParts->m_maker->GetNowMaterial();
	menuParts->AddIcon(0, 46.f, 2.f, 0.f, 0, false, 1.1f);
	menuParts->AddIcon(0, 46.f, 2.f, -18.f, 0, false, 1.1f);
	menuParts->Build(4, 7, x, 4.f, y, parent->transform());
	MenuParts::APPLY_HERO = 1;
	menuParts->AddTextMesh(Vector3(24.f, 4.f, 31.f), 9.5f, TextAlignment::Center, TextAnchor::UpperCenter);
	menuParts->AddTextMesh(Vector3(-64.f, 4.f, -3.5f), 9.f, TextAlignment::Left, TextAnchor::UpperLeft, 1.1f);
	menuParts->AddTextMesh(Vector3(-64.f, 4.f, -24.5f), 9.f, TextAlignment::Left, TextAnchor::UpperLeft, 1.1f);
	menuParts->AddTextMesh(Vector3(-75.5f, 4.f, 46.f), 9.f);
	short partsNo = item->AddParts(menuParts);
	item->SetTableForCode(no, partsNo);
	item->SetTableForItemParam(no, 0, partsNo, 1);
	item->SetTableForItemParam(no, 1, partsNo, 4);
	item->SetTableForItemParam(no, 2, partsNo, 5);
	item->SetTableForItemParam(no, 3, partsNo, 6);
	item->SetTableForItemParam(no, 4, partsNo, 0);
	item->SetTableForItemParam(no, 5, partsNo, 7);
	item->SetTableForItemParamTextColorChanger(no, 6, menuParts);
	item->SetTableForItemParam(no, 7, std::make_shared<StatusIconChanger>(nowMaterial, menuParts->m_list[2], menuParts->m_list[3]));
}

void MenuPortItemTable::Sub_ShopPageing(MenuPortItem* item, MenuParts* parent, short idex, short prmNo, short prmSub, short visNo) {
	int count = parent->m_list.size();
	GameObject* page = parent->AddTextMesh(Vector3(225.f, 4.f, 91.5f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	MenuParts* menuParts = MenuParts::CreateButtonIcon(10, 0, 0.9f, -1, 8);
	menuParts->Build(4, 7, -190.f, 4.f, 91.5f, page->transform);
	item->AddParts(menuParts);
	menuParts = MenuParts::CreateButtonIcon(10, 1, 0.9f, -1, 4);
	menuParts->Build(4, 7, 190.f, 4.f, 91.5f, page->transform);
	item->AddParts(menuParts);
	item->SetTableForItemParam(prmNo, prmSub, idex, count);
	item->SetTableForCode(visNo, [=](short code) mutable
	{
		page->SetActiveRecursively(code != 0);
	});
}

MenuPortItem* MenuPortItemTable::Make_ShopBuyInfo(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	float num = 0.f;
	float num2 = 0.f;
	MenuParts* menuParts;
	if (type == 0)
	{
		menuParts = MenuParts::CreateBuildFrameParts(228, 6, 0.f, 0.f, -2.f);
	}
	else
	{
		menuParts = MenuParts::CreateBuildFrameParts(229, 6, 0.f, 0.f, -2.f);
		num = 247.f;
		num2 = 220.f;
	}
	if (GetLangages::m_langagesNumber <= 2)
	{
		menuParts->AddTextMesh(Vector3(48.5f, 4.f, 472.f - num), (GetLangages::m_langagesNumber != 1) ? 9.f : 8.5f, TextAlignment::Left, TextAnchor::MiddleCenter, 1.25f);
	}
	else
	{
		menuParts->AddTextMesh(Vector3(18.5f, 4.f, 472.f - num), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.25f);
	}
	menuParts->AddTextMesh(Vector3(176.f, 4.f, 497.5f - num), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter, 1.25f);
	menuParts->AddTextMesh(Vector3(140.5f, 4.f, 458.5f - num), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.1f);
	menuParts->AddTextMesh(Vector3(269.f, 4.f, 472.f - num), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.25f);
	menuParts->AddTextMesh(Vector3(11.5f, 4.f, 58.f), 9.f, TextAlignment::Left, TextAnchor::UpperLeft, 1.25f);
	short num3 = menuPortItem->AddParts(menuParts);
	for (short num4 = 0; num4 < 5; num4++)
	{
		menuPortItem->SetTableForItemParam(4, num4, num3, 1 + num4);
	}
	Sub_ShopPageing(menuPortItem, menuParts, num3, 4, 5, 4);
	MenuParts* menuParts2 = MenuParts::Create(0);
	menuParts2->AddIcon(0, 0.f, 0.f, 0.f, 0, true, 0.9f);
	menuParts2->Build(4, 6, 117.f, 4.f, 458.5f - num, menuParts->transform());
	menuPortItem->SetTableForItemParam(4, 6, menuPortItem->AddParts(menuParts2), 0);
	Sub_ShopCharaButton(menuPortItem, menuParts, 0, -104.f, 345.f - num2);
	Sub_ShopCharaButton(menuPortItem, menuParts, 1, 104.f, 345.f - num2);
	if (type == 0)
	{
		Sub_ShopCharaButton(menuPortItem, menuParts, 2, -104.f, 205.f - num2);
		Sub_ShopCharaButton(menuPortItem, menuParts, 3, 104.f, 205.f - num2);
	}
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
	return menuPortItem;
}

void MenuPortItemTable::Sub_ShopItemButton(MenuPortItem* item, short no, Transform* parent) {
	MenuParts* menuParts = MenuParts::CreateButton(170.f, 38.f, no, 16);
	menuParts->ChangeParts(0);
	menuParts->AddIcon(3, 18.f, 4.f, 13.f, 0, false, 0.8f);
	menuParts->ChangeParts(6);
	menuParts->AddIcon(6, 10.f, 6.f, 24.f, 0, false, 0.6f);
	menuParts->AddIcon(Vector3(5.5f, 2.f, -33.f), 3, false, 192.f, 24.f);
	menuParts->Build(4, 8, -146.f, 4.f, 386 - 77 * no, parent);
	menuParts->AddTextMesh(Vector3(-38.f, 4.f, 11.5f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.05f);
	menuParts->AddTextMesh(Vector3(-78.f, 4.f, 2.5f), 9.f, TextAlignment::Left, TextAnchor::MiddleCenter);
	menuParts->AddTextMesh(Vector3(0.f, 4.f, -21.5f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	short partsNo = item->AddParts(menuParts);
	item->SetTableForCode(no, partsNo);
	item->SetTableForItemParam(no, 0, partsNo, 1);
	item->SetTableForItemParam(no, 1, partsNo, 2);
	item->SetTableForItemParam(no, 2, partsNo, 4);
	item->SetTableForItemParam(no, 3, partsNo, 5);
	item->SetTableForItemParam(no, 4, partsNo, 6);
}

MenuPortItem* MenuPortItemTable::Make_ShopSellItemList(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = Sub_BackButton(menuPortItem);
	float xpos;
	float xpos2;
	TextAnchor anchor;
	float size;
	if (GetLangages::m_langagesNumber <= 2)
	{
		xpos = 204.f;
		xpos2 = 48.f;
		anchor = TextAnchor::MiddleCenter;
		size = ((GetLangages::m_langagesNumber != 1) ? 9.f : 8.5f);
	}
	else
	{
		xpos = 175.f;
		xpos2 = 18.5f;
		size = 9.f;
		anchor = TextAnchor::MiddleLeft;
	}
	Sub_ShopItemButton(menuPortItem, 0, menuParts->transform());
	Sub_ShopItemButton(menuPortItem, 1, menuParts->transform());
	Sub_ShopItemButton(menuPortItem, 2, menuParts->transform());
	Sub_ShopItemButton(menuPortItem, 3, menuParts->transform());
	MenuParts* title = MenuParts::CreateBuildTextMenu(6, xpos, 4.f, 472.f, menuParts->transform(), size, TextAlignment::Left, anchor, 1.25f);
	menuPortItem->SetTableForItemParam(4, 0, menuPortItem->AddParts(title), 0);
	menuPortItem->SetTableForCode(4, [=](short code) mutable
	{
		Vector3 position = title->transform()->position();
		position = ((code != 0) ? eIndent::CalcScreenPosition(6, xpos2, position.y, 282.5f) : eIndent::CalcScreenPosition(6, xpos, position.y, 472.f));
		title->transform()->setPosition(position);
	});
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_ShopSellItemInfo(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(251, 6, 0.f, 0.f, -2.f);
	menuParts->AddTextMesh(Vector3(48.5f, 4.f, 181.5f), 9.f, TextAlignment::Left, TextAnchor::MiddleCenter, 1.25f);
	menuParts->AddTextMesh(Vector3(176.f, 4.f, 207.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter, 1.25f);
	menuParts->AddTextMesh(Vector3(140.5f, 4.f, 168.f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.1f);
	menuParts->AddTextMesh(Vector3(269.f, 4.f, 181.5f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.25f);
	menuParts->AddTextMesh(Vector3(11.5f, 4.f, 58.5f), 7.5f, TextAlignment::Left, TextAnchor::UpperLeft, 1.25f);
	menuParts->AddTextMesh(Vector3(112.5f, 4.f, 102.5f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.25f);
	menuParts->AddTextMesh(Vector3(227.5f, 4.f, 102.5f), 9.f, TextAlignment::Right, TextAnchor::MiddleRight, 1.25f);
	menuParts->AddTextMesh(Vector3(433.5f, 4.f, 102.5f), 9.f, TextAlignment::Right, TextAnchor::MiddleRight, 1.25f);
	short partsNo = menuPortItem->AddParts(menuParts);
	for (short num = 0; num < 8; num++)
	{
		menuPortItem->SetTableForItemParam(0, num, partsNo, 1 + num);
	}
	MenuParts* menuParts2 = MenuParts::Create(0);
	menuParts2->AddIcon(0, 0.f, 0.f, 0.f, 0, true, 0.9f);
	menuParts2->Build(4, 6, 117.f, 4.f, 168.f, menuParts->transform());
	menuPortItem->SetTableForItemParam(0, 8, menuPortItem->AddParts(menuParts2), 0);
	menuParts2 = MenuParts::CreateButton(48.5f, 23.5f, 0, 16);
	menuParts2->Build(4, 7, 105.5f, 4.f, 34.f);
	menuParts2->AddTextMesh(Vector3(0.f, 8.f, 0.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuPortItem->SetTableForItemParam(1, 0, menuPortItem->AddParts(menuParts2), 1);
	menuParts2->transform()->SetParent(menuParts->transform());
	menuParts2 = MenuParts::CreateButton(2.f, 14.f, 0, 8);
	menuParts2->ChangeParts(8, false, true);
	menuParts2->AddIcon(4, 0.f, 4.f, 0.f, 2, false, 0.3f);
	menuParts2->Build(4, Vector3(160.f, 4.f, 105.f), menuParts->transform());
	menuPortItem->AddParts(menuParts2);
	menuParts2 = MenuParts::CreateButton(2.f, 14.f, 0, 4);
	menuParts2->ChangeParts(8, false, true);
	menuParts2->AddIcon(4, 0.f, 4.f, 0.f, 3, false, 0.3f);
	menuParts2->Build(4, Vector3(254.f, 4.f, 105.f), menuParts->transform());
	menuPortItem->AddParts(menuParts2);
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_BankDeposit(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(260, 6, 0.f, 0.f, -2.f);
	menuParts->AddTextMesh(Vector3(11.5f, 4.f, 240.f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.1f);
	menuParts->AddTextMesh(Vector3(260.f, 4.f, 240.f), 9.f, TextAlignment::Right, TextAnchor::MiddleRight);
	menuParts->AddTextMesh(Vector3(345.5f, 4.f, 150.f), 9.f, TextAlignment::Right, TextAnchor::UpperRight);
	menuParts->AddTextMesh(Vector3(433.5f, 4.f, 150.f), 9.f, TextAlignment::Right, TextAnchor::UpperRight);
	menuPortItem->AddParts(menuParts);
	menuPortItem->SetTableForItemParam(0, 0, 0, 1);
	menuPortItem->SetTableForItemParam(0, 1, 0, 2);
	menuPortItem->SetTableForItemParam(0, 4, 0, 3);
	menuPortItem->SetTableForItemParam(0, 5, 0, 4);
	Sub_CasinoEnter(menuPortItem, 0, 16, menuParts->transform(), 0, 3);
	short num = 0;
	short num2 = 1;
	while (num < 5)
	{
		if (num < 2)
		{
			Sub_CasinoUD(menuPortItem, num, num2, menuParts, 0, true, 22.f);
		}
		else
		{
			Sub_CasinoUD(menuPortItem, num, num2, menuParts, 0, false, 0.f);
		}
		num++;
		num2 *= 10;
	}
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_BankDraw(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(261, 6, 0.f, 0.f, -2.f);
	menuParts->AddTextMesh(Vector3(11.5f, 4.f, 240.f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.1f);
	menuParts->AddTextMesh(Vector3(260.f, 4.f, 240.f), 9.f, TextAlignment::Right, TextAnchor::MiddleRight);
	menuParts->AddTextMesh(Vector3(443.f, 4.f, 240.f), 9.f, TextAlignment::Left, TextAnchor::MiddleRight);
	menuParts->AddTextMesh(Vector3(345.5f, 4.f, 150.f), 9.f, TextAlignment::Right, TextAnchor::UpperRight);
	menuParts->AddTextMesh(Vector3(433.5f, 4.f, 150.f), 9.f, TextAlignment::Right, TextAnchor::UpperRight);
	menuPortItem->AddParts(menuParts);
	menuPortItem->SetTableForItemParam(0, 0, 0, 1);
	menuPortItem->SetTableForItemParam(0, 1, 0, 2);
	menuPortItem->SetTableForItemParam(0, 2, 0, 3);
	menuPortItem->SetTableForItemParam(0, 4, 0, 4);
	menuPortItem->SetTableForItemParam(0, 5, 0, 5);
	Sub_CasinoEnter(menuPortItem, 0, 16, menuParts->transform(), 0, 3);
	short num = 0;
	short num2 = 1;
	while (num < 5)
	{
		if (num < 2)
		{
			Sub_CasinoUD(menuPortItem, num, num2, menuParts, 0, true, 22.f);
		}
		else
		{
			Sub_CasinoUD(menuPortItem, num, num2, menuParts, 0, false, 0.f);
		}
		num++;
		num2 *= 10;
	}
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
	return menuPortItem;
}

Transform* MenuPortItemTable::Sub_ShopButton(MenuPortItem* item, MenuParts* parent, float fontSize, short bid, float x, float y, float w) {
	MenuParts* menuParts = MenuParts::CreateButton(w, 24.f, bid, 16);
	menuParts->Build(4, 7, x, 4.f, y, parent->transform());
	menuParts->AddTextMesh(Vector3(0.f, 4.f, 0.f), fontSize, TextAlignment::Center, TextAnchor::MiddleCenter);
	item->SetTableForItemParam(bid, 0, item->AddParts(menuParts), 1);
	return menuParts->transform();
}

void MenuPortItemTable::Sub_ShopAbort(MenuPortItem* item, MenuParts* parent, float textSize) {
	MenuParts* menuParts = MenuParts::CreateButton(100.f, 24.f, 0, 32);
	menuParts->Build(4, 7, 152.5f, 4.f, 33.5f, parent->transform());
	menuParts->AddTextMesh(Vector3(0.f, 4.f, 0.f), textSize, TextAlignment::Center, TextAnchor::MiddleCenter);
	item->SetTableForItemParam(5, 0, item->AddParts(menuParts), 1);
}

MenuPortItem* MenuPortItemTable::Make_ChurchRoot(short port, short type) {
	MenuPortItem* item = new MenuPortItem(port, type);
	MenuParts* frame;
	if (GetLangages::m_langagesNumber == 2)
	{
		frame = MenuParts::CreateBuildFrameParts(263, 6, 0.f, 0.f, -2.f);
		frame->AddTextMesh(Vector3(11.5f, 4.f, 238.f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.1f);
	}
	else
	{
		frame = MenuParts::CreateBuildFrameParts(262, 6, 0.f, 0.f, -2.f);
		frame->AddTextMesh(Vector3(11.5f, 4.f, 240.f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft);
	}
	item->SetTableForItemParam(6, 0, item->AddParts(frame), 1);
	item->SetTableForCode(0, [=](short code) mutable
	{
		if (code == 0)
		{
			Sub_ShopButton(item, frame, 8.5f, 0, -111.5f, 170.75f, 177.1f);
			Sub_ShopButton(item, frame, 8.5f, 1, 111.5f, 170.75f, 177.1f);
			Sub_ShopButton(item, frame, 8.5f, 2, -145.f, 106.25f);
			Sub_ShopButton(item, frame, 8.5f, 3, 0.f, 106.25f);
			Sub_ShopButton(item, frame, 8.5f, 4, 145.f, 106.25f);
		}
		else
		{
			Sub_ShopButton(item, frame, 8.5f, 0, 0.f, 170.75f, 177.1f);
			Sub_ShopButton(item, frame, 8.5f, 1, -145.f, 106.25f);
			Sub_ShopButton(item, frame, 8.5f, 2, 0.f, 106.25f);
			Sub_ShopButton(item, frame, 8.5f, 3, 145.f, 106.25f);
		}
	});
	Sub_ShopAbort(item, frame, 8.5f);
	return item;
}

MenuPortItem* MenuPortItemTable::Make_ChurchWork(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(264, 6, 0.f, 0.f, -2.f);
	menuParts->AddTextMesh(Vector3(12.f, 4.f, 398.5f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft);
	menuParts->AddTextMesh(Vector3(442.5f, 4.f, 398.5f), 9.5f, TextAlignment::Right, TextAnchor::MiddleRight);
	short num = menuPortItem->AddParts(menuParts);
	menuPortItem->SetTableForItemParam(4, 0, num, 1);
	menuPortItem->SetTableForItemParam(4, 1, num, 2);
	Sub_ShopPageing(menuPortItem, menuParts, num, 4, 2, 4);
	Sub_ShopCharaButton(menuPortItem, menuParts, 0, -104.f, 295.f);
	Sub_ShopCharaButton(menuPortItem, menuParts, 1, 104.f, 295.f);
	Sub_ShopCharaButton(menuPortItem, menuParts, 2, -104.f, 169.f);
	Sub_ShopCharaButton(menuPortItem, menuParts, 3, 104.f, 169.f);
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_TorunekoMiseban(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(284, 6, 0.f, 0.f, -2.f);
	short partsNo = menuPortItem->AddParts(menuParts);
	menuParts->AddTextMesh(Vector3(442.5f, 4.f, 404.5f), 9.f, TextAlignment::Right, TextAnchor::UpperRight, 3.07f);
	menuPortItem->SetTableForItemParam(6, 0, partsNo, 1);
	for (short num = 0; num < 6; num++)
	{
		Sub_ShopBuyButton(menuPortItem, menuParts, 393 - 58 * num, num, false);
	}
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_NENE_ItemView(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(-1, 6, 0.f, 0.f, -2.f, false, true);
	menuParts->AddTextMesh(Vector3(45.f, 4.f, 114.f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft);
	menuParts->AddTextMesh(Vector3(269.f, 4.f, 471.5f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.25f);
	menuParts->AddTextMesh(Vector3(11.5f, 4.f, 58.5f), 9.f, TextAlignment::Left, TextAnchor::UpperLeft, 1.25f);
	menuPortItem->AddParts(menuParts);
	menuPortItem->SetTableForItemParam(0, 0, 0, 0);
	menuPortItem->SetTableForItemParam(0, 1, 0, 1);
	menuPortItem->SetTableForItemParam(0, 2, 0, 2);
	menuPortItem->SetTableForItemParam(0, 3, 0, 3);
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_ViewVolumeDecision(short port, short type) {
	MenuPortItem* item = new MenuPortItem(port, type);
	item->SetTableForCode(0, [=](short num) mutable
	{
		MenuPort::Param param = item->Receive();
		uint32_t num2 = 0u;
		Vector3 position = Vector3::zero;
		Camera* camera = MainLoop::m_camera;
		int i = 0;
		int num3 = 0;
		for (; i < num; i++)
		{
			position.Set(param.GetFloat(num3++), param.GetFloat(num3++), 0.f - param.GetFloat(num3++));
			position = camera->WorldToViewportPoint(position);
			if (0.f < position.x && position.x < 1.f && 0.f < position.y && position.y < 1.f)
			{
				num2 |= (uint)(1 << i);
			}
		}
		item->Send(2, (short)(num2 & 0xFFFF), (short)(num2 >> 16));
	});
	return item;
}
