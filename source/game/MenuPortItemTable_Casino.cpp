// game/MenuPortItemTable_Minigames.cpp -- OWNER: WP4b.  C# source: re/cs_out/Assembly-CSharp/MenuPortItemTable.cs (first pass by tools/menu_cs2cpp.py, then fixed by hand).  Casino + coliseum (fight stadium) screens; the DQ5 minigames were removed for DQ4.
#include "game/MenuPortItemTable.hpp"
#include "game/MeshChara.hpp"
#include "game/MainLoop.hpp"
#include "utils/logger.h"
#include <cstdio>

MenuParts* MenuPortItemTable::Sub_CasinoUD(MenuPortItem* item, short bid, short pad, Transform* parent, bool bUp, float x, float y, float z, float size) {
	MenuParts* menuParts = MenuParts::CreateButton(size, 10.f, bid, pad);
	menuParts->ChangeParts(8, false, true);
	menuParts->AddIcon(4, 0.f, 4.f, 0.f, (!bUp) ? 1 : 0, false, 0.3f);
	menuParts->Build(4, 7, x, y, z);
	item->AddParts(menuParts);
	menuParts->transform()->SetParent(parent);
	return menuParts;
}

void MenuPortItemTable::Sub_CasinoUD(MenuPortItem* item, short pos, short bid, MenuParts* parent, int parentIndex, bool bNoButton, float xdif) {
	float num = 88.f * (float)pos + xdif;
	float x = 177.f - num;
	if (!bNoButton)
	{
		Sub_CasinoUD(item, bid, 2, parent->transform(), true, x, 4.f, 180.f, 40.f);
		Sub_CasinoUD(item, bid, 1, parent->transform(), false, x, 4.f, 95.f, 40.f);
	}
	parent->AddTextMesh(Vector3(411.5f - num, 4.f, 150.f), 9.f, TextAlignment::Right, TextAnchor::UpperRight);
	item->SetTableForItemParam((short)(pos + 1), 0, parentIndex, parent->m_list.size() - 1);
}

void MenuPortItemTable::Sub_CasinoBuyUD(MenuPortItem* item, short pos, short bid, MenuParts* parent, int parentIndex) {
	float num = 84.f * (float)pos;
	float x = 158.5f - num;
	Sub_CasinoUD(item, bid, 2, parent->transform(), true, x, 4.f, 180.f, 30.f);
	Sub_CasinoUD(item, bid, 1, parent->transform(), false, x, 4.f, 95.f, 30.f);
	parent->AddTextMesh(Vector3(393.f - num, 4.f, 150.f), 9.f, TextAlignment::Right, TextAnchor::UpperRight);
	item->SetTableForItemParam((short)(pos + 1), 0, parentIndex, parent->m_list.size() - 1);
}

MenuParts* MenuPortItemTable::Sub_CasinoEnter(MenuPortItem* item, short bid, short pad, Transform* parent, short cid, short did) {
	MenuParts* menuParts = MenuParts::CreateButton(68.5f, 23.5f, bid, pad);
	menuParts->Build(4, 7, 96.f, 4.f, 34.f);
	menuParts->AddTextMesh(Vector3(0.f, 8.f, 0.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	item->SetTableForItemParam(cid, did, item->AddParts(menuParts), 1);
	menuParts->transform()->SetParent(parent);
	return menuParts;
}

MenuParts* MenuPortItemTable::Sub_CasinoStart(MenuPortItem* item, short bid, short pad, Transform* parent, short cid, short did) {
	MenuParts* menuParts = MenuParts::CreateButton(96.5f, 26.f, bid, pad);
	menuParts->Build(4, 7, 91.f, 4.f, 138.f);
	menuParts->AddTextMesh(Vector3(0.f, 8.f, 0.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	item->SetTableForItemParam(cid, did, item->AddParts(menuParts), 1);
	menuParts->transform()->SetParent(parent);
	return menuParts;
}

MenuPortItem* MenuPortItemTable::Make_Casino_Slot(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateWindow0(116.f, 32.f);
	MenuParts* menuParts2 = MenuParts::CreateBuildFrameParts(207, 6, 0.f, 0.f, -2.f);
	menuParts->Build(2, 5, 2.f, 0.f, 77.f);
	menuParts->AddTextMesh(Vector3(-12.f, 4.f, -28.f), 9.f, TextAlignment::Right, TextAnchor::UpperRight);
	menuParts->AddTextMesh(Vector3(-128.f, 4.f, -7.2f), 9.f);
	menuPortItem->AddParts(menuParts);
	menuPortItem->SetTableForItemParam(0, 0, 0, 1);
	menuPortItem->SetTableForItemParam(0, 1, 0, 2);
	menuParts2->AddTextMesh(Vector3(11.5f, 4.f, 237.f), 9.5f);
	menuParts2->AddTextMesh(Vector3(205.5f, 4.f, 150.f), 9.f, TextAlignment::Right, TextAnchor::UpperRight);
	menuPortItem->AddParts(menuParts2);
	menuPortItem->SetTableForCode(0, 1);
	menuPortItem->SetTableForItemParam(0, 2, 1, 1);
	menuPortItem->SetTableForItemParam(0, 4, 1, 2);
	menuParts2->transform()->SetParent(menuParts->transform());
	Sub_CasinoStart(menuPortItem, 0, 16, menuParts2->transform(), 0, 3);
	Sub_CasinoUD(menuPortItem, 0, 1, menuParts2, 1, false, 256.f);
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts2->transform());
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_Casino_CoinShop(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(213, 6, 0.f, 0.f, -2.f);
	menuParts->AddTextMesh(Vector3(11.5f, 4.f, 240.f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.1f);
	menuParts->AddTextMesh(Vector3(260.f, 4.f, 240.f), 9.f, TextAlignment::Right, TextAnchor::MiddleRight);
	menuParts->AddTextMesh(Vector3(443.f, 4.f, 240.f), 9.f, TextAlignment::Left, TextAnchor::MiddleRight);
	menuParts->AddTextMesh(Vector3(439.f, 4.f, 150.f), 9.f, TextAlignment::Right, TextAnchor::UpperRight);
	menuPortItem->AddParts(menuParts);
	menuPortItem->SetTableForItemParam(0, 0, 0, 1);
	menuPortItem->SetTableForItemParam(0, 1, 0, 2);
	menuPortItem->SetTableForItemParam(0, 2, 0, 3);
	menuPortItem->SetTableForItemParam(0, 4, 0, 4);
	Sub_CasinoEnter(menuPortItem, 0, 16, menuParts->transform(), 0, 3);
	short num = 0;
	short num2 = 1;
	while (num < 5)
	{
		Sub_CasinoBuyUD(menuPortItem, num, num2, menuParts, 0);
		num++;
		num2 *= 10;
	}
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_Casino_Poker(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(208, 0, 0.f, 0.f, 4.f);
	MenuParts* menuParts2 = MenuParts::CreateBuildFrameParts(207, 6, 0.f, 0.f, -2.f);
	MenuParts* iframe = MenuParts::CreateBuildVariableWindow(0, 170.f, 14.f, 0, false);
	MenuParts* buttons[6] = {};
	menuParts->AddTextMesh(Vector3(8.f, 4.f, -11.5f), 9.5f, TextAlignment::Left, TextAnchor::UpperLeft, 1.03f);
	menuParts->AddTextMesh(Vector3(442.f, 4.f, -11.5f), 9.5f, TextAlignment::Right, TextAnchor::UpperRight, 1.03f);
	menuParts->AddTextMesh(Vector3(253.f, 4.f, -228.f), 9.5f, TextAlignment::Left, TextAnchor::UpperLeft, 1.16f);
	menuParts->AddTextMesh(Vector3(442.f, 4.f, -228.f), 9.5f, TextAlignment::Right, TextAnchor::UpperRight, 1.16f);
	menuPortItem->AddParts(menuParts);
	menuPortItem->SetTableForItemParam(0, 0, 0, 1);
	menuPortItem->SetTableForItemParam(0, 1, 0, 2);
	menuPortItem->SetTableForItemParam(0, 2, 0, 3);
	menuPortItem->SetTableForItemParam(0, 3, 0, 4);
	menuParts2->AddTextMesh(Vector3(11.5f, 4.f, 237.f), 9.5f);
	menuParts2->AddTextMesh(Vector3(205.5f, 4.f, 150.f), 9.f, TextAlignment::Right, TextAnchor::UpperRight);
	short num = menuPortItem->AddParts(menuParts2);
	menuPortItem->SetTableForCode(0, num);
	menuPortItem->SetTableForItemParam(0, 4, num, 1);
	menuPortItem->SetTableForItemParam(0, 7, num, 2);
	menuParts2->transform()->SetParent(menuParts->transform());
	Sub_CasinoStart(menuPortItem, 0, 16, menuParts2->transform(), 0, 5);
	Sub_CasinoUD(menuPortItem, 0, 1, menuParts2, num, false, 256.f);
	Sub_CasinoUD(menuPortItem, 1, 10, menuParts2, num, false, 256.f);
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts2->transform());
	iframe->transform()->setPosition(eIndent::CalcScreenPosition(3, -2.f, 0.f, 62.5f));
	iframe->AddTextMesh(Vector3(12.f, 4.f, -8.5f), 9.5f, TextAlignment::Left, TextAnchor::UpperLeft, 1.16f);
	iframe->transform()->SetParent(menuParts->transform());
	menuPortItem->SetTableForItemParam(0, 6, menuPortItem->AddParts(iframe), 1);
	menuPortItem->SetTableForCode(1, [=](short code) mutable
	{
		iframe->gameObject->SetActiveRecursively(code != 0);
		switch (code)
		{
		case 1:
			iframe->transform()->setPosition(eIndent::CalcScreenPosition(3, -2.f, 0.f, 62.5f));
			iframe->VariableWindow(0, 170.f, 14.f, 0);
			break;
		case 2:
			iframe->transform()->setPosition(eIndent::CalcScreenPosition(3, -2.f, 8.f, 85.f));
			iframe->VariableWindow(0, 170.f, 37.5f, 0);
			break;
		}
	});
	MenuParts* menuParts3;
	for (short num2 = 0; num2 < 5; num2++)
	{
		menuParts3 = (buttons[num2] = MenuParts::CreateButton(40.f, 16.f, num2, 16));
		menuParts3->Build(4, 3, 52 + 86 * num2, 0.f, -150.f);
		menuParts3->AddTextMesh(Vector3(0.f, 4.f, 0.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
		menuPortItem->SetTableForItemParam(4, num2, menuPortItem->AddParts(menuParts3), 1);
		menuParts3->transform()->SetParent(menuParts->transform());
	}
	menuParts3 = (buttons[5] = MenuParts::CreateButton(40.f, 16.f, 5, 16));
	menuParts3->Build(4, 3, 224.f, 0.f, -210.f);
	menuParts3->AddTextMesh(Vector3(0.f, 4.f, 0.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuPortItem->SetTableForItemParam(4, 5, menuPortItem->AddParts(menuParts3), 1);
	menuParts3->transform()->SetParent(menuParts->transform());
	menuPortItem->SetTableForCode(2, [=](short code) mutable
	{
		for (short num3 = 0; num3 < 6; num3++)
		{
			buttons[num3]->gameObject->SetActiveRecursively((code & (1 << (int)num3)) != 0);
		}
	});
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_Casino_GiftList(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(227, 6, 0.f, 0.f, -2.f);
	menuParts->AddTextMesh(Vector3(442.5f, 4.f, 404.5f), 9.f, TextAlignment::Right, TextAnchor::UpperRight, 3.07f);
	menuPortItem->SetTableForItemParam(6, 0, menuPortItem->AddParts(menuParts), 1);
	for (short num = 0; num < 6; num++)
	{
		Sub_ShopBuyButton(menuPortItem, menuParts, 393 - 58 * num, num, false);
	}
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_Casino_GiftTarget(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(230, 6, 0.f, 0.f, -2.f);
	menuParts->AddTextMesh(Vector3(88.f, 4.f, 497.5f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter, 1.25f);
	menuParts->AddTextMesh(Vector3(52.5f, 4.f, 458.5f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.1f);
	menuParts->AddTextMesh(Vector3(269.f, 4.f, 472.f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.25f);
	menuParts->AddTextMesh(Vector3(11.5f, 4.f, 58.f), 9.f, TextAlignment::Left, TextAnchor::UpperLeft, 1.25f);
	GameObject* page = menuParts->AddTextMesh(Vector3(225.f, 4.f, 91.5f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	MenuParts* menuParts2 = MenuParts::CreateButtonIcon(10, 0, 0.9f, -1, 8);
	menuParts2->Build(4, 7, -190.f, 4.f, 91.5f, page->transform);
	menuPortItem->AddParts(menuParts2);
	menuParts2 = MenuParts::CreateButtonIcon(10, 1, 0.9f, -1, 4);
	menuParts2->Build(4, 7, 190.f, 4.f, 91.5f, page->transform);
	menuPortItem->AddParts(menuParts2);
	menuPortItem->SetTableForCode(4, [=](short code) mutable
	{
		page->SetActiveRecursively(code != 0);
	});
	short partsNo = menuPortItem->AddParts(menuParts);
	for (short num = 0; num < 5; num++)
	{
		menuPortItem->SetTableForItemParam(4, num, partsNo, 1 + num);
	}
	menuParts2 = MenuParts::Create(0);
	menuParts2->AddIcon(0, 0.f, 0.f, 0.f, 0, true, 0.9f);
	menuParts2->Build(4, 6, 29.f, 4.f, 458.5f, menuParts->transform());
	menuPortItem->SetTableForItemParam(4, 5, menuPortItem->AddParts(menuParts2), 0);
	Sub_ShopCharaButton(menuPortItem, menuParts, 0, -104.f, 345.f);
	Sub_ShopCharaButton(menuPortItem, menuParts, 1, 104.f, 345.f);
	Sub_ShopCharaButton(menuPortItem, menuParts, 2, -104.f, 205.f);
	Sub_ShopCharaButton(menuPortItem, menuParts, 3, 104.f, 205.f);
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_Casino_FightStadiumBet(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(209, 6, 0.f, 0.f, -2.f);
	short num = menuPortItem->AddParts(menuParts);
	menuParts->AddTextMesh(Vector3(12.5f, 4.f, 238.5f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.15f);
	menuParts->AddTextMesh(Vector3(221.5f, 4.f, 239.5f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuParts->AddTextMesh(Vector3(444.f, 4.f, 238.5f), 9.f, TextAlignment::Left, TextAnchor::MiddleRight, 1.15f);
	menuParts->AddTextMesh(Vector3(205.5f, 4.f, 150.f), 9.f, TextAlignment::Right, TextAnchor::UpperRight);
	Sub_CasinoUD(menuPortItem, 0, 1, menuParts, num, false, 256.f);
	Sub_CasinoUD(menuPortItem, 1, 10, menuParts, num, false, 256.f);
	Sub_CasinoStart(menuPortItem, 0, 16, menuParts->transform(), 0, 3);
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
	menuPortItem->SetTableForItemParam(0, 0, num, 1);
	menuPortItem->SetTableForItemParam(0, 1, num, 2);
	menuPortItem->SetTableForItemParam(0, 2, num, 3);
	menuPortItem->SetTableForItemParam(0, 4, num, 4);
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_Casino_FightStadiumSelect(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* frame = MenuParts::CreateBuildFrameParts(210, 6, 0.f, 0.f, -2.f, false, true);
	short partsNo = menuPortItem->AddParts(frame);
	frame->AddTextMesh(Vector3(10.5f, 4.f, 326.5f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft);
	frame->AddTextMesh(Vector3(440.f, 4.f, 326.5f), 9.f, TextAlignment::Right, TextAnchor::MiddleRight);
	menuPortItem->SetTableForItemParam(4, 0, partsNo, 1);
	menuPortItem->SetTableForItemParam(4, 1, partsNo, 2);
	menuPortItem->SetTableForCode(4, [=](short code) mutable
	{
		frame->m_list[0]->m_intChanger((code != 0) ? 211 : 210);
	});
	for (short num = 0; num < 4; num++)
	{
		float z = 264 - 55 * num;
		MenuParts* menuParts = MenuParts::CreateButton(190.f, 14.f, num, 16);
		menuParts->Build(4, 6, 118.f, 4.f, z, frame->transform());
		menuParts->AddTextMesh(Vector3(0.f, 4.f, 0.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
		menuParts->AddTextMesh(Vector3(180.f, 4.f, 0.f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft);
		partsNo = menuPortItem->AddParts(menuParts);
		menuPortItem->SetTableForCode(num, partsNo);
		menuPortItem->SetTableForItemParam(num, 0, partsNo, 1);
		menuPortItem->SetTableForItemParam(num, 1, partsNo, 2);
	}
	Sub_BackButton(menuPortItem)->transform()->SetParent(frame->transform());
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_Casino_FightStadiumCoin(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(212, 6, 0.f, 0.f, 588.5f);
	menuParts->AddTextMesh(Vector3(253.f, 4.f, -12.f), 9.5f, TextAlignment::Left, TextAnchor::UpperLeft, 1.16f);
	menuParts->AddTextMesh(Vector3(442.f, 4.f, -12.f), 9.5f, TextAlignment::Right, TextAnchor::UpperRight, 1.16f);
	menuPortItem->AddParts(menuParts);
	menuPortItem->SetTableForItemParam(0, 0, 0, 1);
	menuPortItem->SetTableForItemParam(0, 1, 0, 2);
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_FightStadiumAbort(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* waku = MenuParts::Create(4);
	waku->AddSquare(Vector3::zero, 0, true, 110.f, 24.f);
	waku->Build(1, eIndent::CalcPosition(MenuMessage::m_window->gameObject, 7, 0.f, 0.f, 1.f));
	menuPortItem->AddParts(waku);
	MenuParts* menuParts = MenuParts::CreateButton(90.f, 4.f, 0, 1024);
	menuParts->Build(4, Vector3(0.f, 4.f, -24.5f), waku->transform());
	menuParts->AddTextMesh(Vector3(0.f, 6.f, 1.f), 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuParts->m_externalToggle = true;
	menuPortItem->AddParts(menuParts);
	menuPortItem->SetTableForItemParam(0, 0, 1, 1);
	menuPortItem->SetTableForItemParam(0, 1, 1, 0);
	menuPortItem->SetTableForCode(0, [=](short code) mutable
	{
		waku->transform()->setPosition(eIndent::CalcPosition(MenuMessage::m_window->gameObject, 7, 0.f, 0.f, 1.f));
	});
	return menuPortItem;
}
