// game/MenuPortItemTable_Minigames.cpp -- OWNER: WP4b.  C# source: re/cs_out/Assembly-CSharp/MenuPortItemTable.cs (first pass by tools/menu_cs2cpp.py, then fixed by hand)
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
		Sub_CasinoUD(item, bid, 1, parent->transform(), true, x, 4.f, 180.f, 40.f);
		Sub_CasinoUD(item, bid, 2, parent->transform(), false, x, 4.f, 95.f, 40.f);
	}
	parent->AddTextMesh(Vector3(411.5f - num, 4.f, 150.f), 9.f, TextAlignment::Right, TextAnchor::UpperRight);
	item->SetTableForItemParam((short)(pos + 1), 0, parentIndex, parent->m_list.size() - 1);
}

void MenuPortItemTable::Sub_CasinoBuyUD(MenuPortItem* item, short pos, short bid, MenuParts* parent, int parentIndex) {
	float num = 84.f * (float)pos;
	float x = 158.5f - num;
	Sub_CasinoUD(item, bid, 1, parent->transform(), true, x, 4.f, 180.f, 30.f);
	Sub_CasinoUD(item, bid, 2, parent->transform(), false, x, 4.f, 95.f, 30.f);
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
	MenuParts* menuParts = MenuParts::CreateWindow0(128.f, 32.f);
	MenuParts* menuParts2 = MenuParts::CreateBuildFrameParts(207, 6, 0.f, 0.f, -2.f);
	menuParts->Build(2, 5, 2.f, 0.f, 104.f);
	menuParts->AddTextMesh(Vector3(-10.f, 4.f, -28.f), 9.f, TextAlignment::Right, TextAnchor::UpperRight);
	menuParts->AddTextMesh(Vector3(-142.f, 4.f, -7.2f), 9.f);
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
	MenuParts* buttons_HL[2] = {};
	menuParts->AddTextMesh(Vector3(8.f, 4.f, -11.5f), 9.5f, TextAlignment::Left, TextAnchor::UpperLeft, 1.03f);
	menuParts->AddTextMesh(Vector3(442.f, 4.f, -11.5f), 9.5f, TextAlignment::Right, TextAnchor::UpperRight, 1.03f);
	menuParts->AddTextMesh(Vector3(223.f, 4.f, -228.f), 9.5f, TextAlignment::Left, TextAnchor::UpperLeft, 1.16f);
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
		for (short num4 = 0; num4 < 6; num4++)
		{
			buttons[num4]->gameObject->SetActiveRecursively((code & (1 << (int)num4)) != 0);
		}
	});
	for (short num3 = 0; num3 < 2; num3++)
	{
		MenuParts* menuParts4 = (buttons_HL[num3] = MenuParts::CreateButton(40.f, 16.f, (short)(6 + num3), 16));
		menuParts4->Build(4, 3, 290.f, 0.f, -150 - 60 * num3);
		menuParts4->AddTextMesh(Vector3(0.f, 4.f, 0.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
		menuPortItem->SetTableForItemParam(5, num3, menuPortItem->AddParts(menuParts4), 1);
		menuParts4->transform()->SetParent(menuParts->transform());
	}
	menuPortItem->SetTableForCode(3, [=](short code) mutable
	{
		for (short num4 = 0; num4 < 2; num4++)
		{
			buttons_HL[num4]->gameObject->SetActiveRecursively((code & (1 << (int)num4)) != 0);
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
	MenuParts* menuParts2 = MenuParts::CreateButtonIcon(10, 0, 0.9f, -1, 4);
	menuParts2->Build(4, 7, -190.f, 4.f, 91.5f, page->transform);
	menuPortItem->AddParts(menuParts2);
	menuParts2 = MenuParts::CreateButtonIcon(10, 1, 0.9f, -1, 8);
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
	menuParts->AddTextMesh(Vector3(215.f, 4.f, 239.5f), 8.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuParts->AddTextMesh(Vector3(445.f, 4.f, 238.5f), 9.f, TextAlignment::Left, TextAnchor::MiddleRight, 1.15f);
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
	frame->AddTextMesh(Vector3(442.f, 4.f, 326.5f), 9.f, TextAlignment::Right, TextAnchor::MiddleRight);
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
	menuParts->AddTextMesh(Vector3(227.5f, 4.f, -12.f), 9.5f, TextAlignment::Left, TextAnchor::UpperLeft, 1.16f);
	menuParts->AddTextMesh(Vector3(445.5f, 4.f, -12.f), 9.5f, TextAlignment::Right, TextAnchor::UpperRight, 1.16f);
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
	MenuParts* menuParts = MenuParts::CreateButton(90.f, 4.f, 0, 4096);
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

MenuPortItem* MenuPortItemTable::Make_SugorokuRoot(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(223, 6, 0.f, 0.f, -2.f);
	menuParts->AddTextMesh(Vector3(11.5f, 4.f, 186.f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft);
	menuPortItem->SetTableForItemParam(6, 0, menuPortItem->AddParts(menuParts), 1);
	Sub_ShopButton(menuPortItem, menuParts, 8.5f, 0, -145.f, 110.5f);
	Sub_ShopButton(menuPortItem, menuParts, 8.5f, 1, 0.f, 110.5f);
	Sub_ShopButton(menuPortItem, menuParts, 8.5f, 2, 145.f, 110.5f);
	MenuParts* menuParts2 = MenuParts::CreateButton(100.f, 24.f, 3, 32);
	menuParts2->Build(4, 7, 152.5f, 4.f, 33.5f, menuParts->transform());
	menuParts2->AddTextMesh(Vector3(0.f, 4.f, 0.f), 8.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuPortItem->SetTableForItemParam(3, 0, menuPortItem->AddParts(menuParts2), 1);
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_Sugoroku_Dice(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateWindow0(148.f, 35.f);
	menuParts->Build(2, 2, 2.f, 0.f, 2.f);
	menuParts->AddTextMesh(Vector3(-12.f, 4.f, -20.f), 9.5f, TextAlignment::Right, TextAnchor::UpperRight);
	menuPortItem->SetTableForItemParam(0, 0, menuPortItem->AddParts(menuParts), 1);
	menuParts->transform()->setPosition(eIndent::CalcScreenPosition(2, 2.f, 0.f, 2.f));
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_Sugoroku_Timer(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateWindow0(148.f, 35.f);
	menuParts->Build(2, 2, 2.f, 0.f, 2.f);
	menuParts->AddTextMesh(Vector3(-12.f, 4.f, -20.f), 9.5f, TextAlignment::Right, TextAnchor::UpperRight);
	menuPortItem->SetTableForItemParam(0, 0, menuPortItem->AddParts(menuParts), 1);
	menuParts->transform()->setPosition(eIndent::CalcScreenPosition(2, 2.f, 0.f, 2.f));
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_Sugoroku_Shop(short port, short type) {
	MenuPortItem* item = new MenuPortItem(port, type);
	MenuParts* image = nullptr;
	item->SetTableForCode(0, [=](short code) mutable
	{
		if (image == nullptr)
		{
			if (code == 0)
			{
				image = MenuParts::CreateFromTexture("Menu/map/yado_pic", true, 0.75f);
			}
			else
			{
				image = MenuParts::CreateFromTexture("Menu/map/yorozu_pic", true, 0.75f);
			}
			image->Build(4, Vector3::zero);
			item->AddParts(image);
		}
	});
	return item;
}

MenuPortItem* MenuPortItemTable::Make_Sugoroku_Arrow(short port, short type) {
	MenuPortItem* item = new MenuPortItem(port, type);
	MenuParts* icon[2] = {};
	MenuPort::Param prm;
	for (short num = 0; num < 2; num++)
	{
		icon[num] = MenuParts::Create(14);
		icon[num]->AddIcon(Vector3::zero, 0, true, 1.8f, 1.8f);
		icon[num]->Build(4, Vector3::zero);
		short _i = num;
		item->SetTableForCode(_i, [=](short code) mutable
		{
			int i = code & 0xFF;
			switch ((code >> 8) & 0xFF)
			{
			case 1:
				prm = item->Receive();
				icon[_i]->transform()->setPosition(Vector3(prm.GetFloat(0), prm.GetFloat(1), prm.GetFloat(2)));
				break;
			case 2:
			{
				prm = item->Receive();
				Vector3 eulerAngles = icon[_i]->transform()->rotation().eulerAngles();
				eulerAngles.y = prm.GetFloat(0);
				icon[_i]->transform()->rotation() = Quaternion::Euler(eulerAngles);
				break;
			}
			case 3:
				icon[_i]->m_list[0]->Change(i);
				break;
			}
		});
		icon[num]->gameObject->layer = 0;
		item->AddParts(icon[num]);
	}
	item->SetTableForCode(10, [=](short code) mutable
	{
		if (((code >> 8) & 0xFF) == 0)
		{
			for (int i = 0; i < 2; i++)
			{
				icon[i]->gameObject->SetActive((code & (1 << i)) != 0);
			}
		}
	});
	return item;
}

MenuPortItem* MenuPortItemTable::Make_SugorokuDrop(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* parts = MenuParts::CreateBuildTextMenu(7, -58.f, 4.f, 474.f, nullptr, 9.5f, TextAlignment::Left, TextAnchor::MiddleLeft);
	menuPortItem->AddParts(parts);
	menuPortItem->SetTableForItemParam(0, 0, 0, 0);
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_MonG_Root(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(262, 6, 0.f, 0.f, -2.f);
	menuParts->AddTextMesh(Vector3(11.5f, 4.f, 240.f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft);
	menuPortItem->SetTableForItemParam(6, 0, menuPortItem->AddParts(menuParts), 1);
	Sub_ShopButton(menuPortItem, menuParts, 8.5f, 0, -145.f, 170.75f);
	Sub_ShopButton(menuPortItem, menuParts, 8.5f, 1, 0.f, 170.75f);
	Sub_ShopButton(menuPortItem, menuParts, 8.5f, 2, 145.f, 170.75f);
	Sub_ShopButton(menuPortItem, menuParts, 8.5f, 3, -145.f, 106.25f);
	Sub_ShopButton(menuPortItem, menuParts, 8.5f, 4, 0.f, 106.25f);
	MenuParts* menuParts2 = MenuParts::CreateButton(100.f, 24.f, 5, 32);
	menuParts2->Build(4, 7, 152.5f, 4.f, 33.5f, menuParts->transform());
	menuParts2->AddTextMesh(Vector3(0.f, 4.f, 0.f), 8.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuPortItem->SetTableForItemParam(5, 0, menuPortItem->AddParts(menuParts2), 1);
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_MonG_Button(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildTextMenu(7, -144.2f, 4.f, 507.f, nullptr, 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuPortItem->SetTableForItemParam(0, 0, menuPortItem->AddParts(menuParts), 0);
	MenuParts* menuParts2 = MenuParts::CreateBuildTextMenu(7, -8.5f, 4.f, 34.f, nullptr, 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
	short partsNo = menuPortItem->AddParts(menuParts2);
	menuPortItem->SetTableForCode(0, partsNo);
	menuPortItem->SetTableForItemParam(0, 1, partsNo, 0);
	menuParts2->transform()->SetParent(menuParts->transform());
	menuParts2 = MenuParts::CreateButton(64.f, 18.f, 0, 16);
	menuParts2->Build(4, 7, 99.f, 4.f, 34.f);
	menuParts2->AddTextMesh(Vector3(0.f, 4.f, -0.15f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuParts2->SetEnableButtonTextColorChange();
	menuParts2->transform()->SetParent(menuParts->transform());
	partsNo = menuPortItem->AddParts(menuParts2);
	menuPortItem->SetTableForCode(1, partsNo);
	menuPortItem->SetTableForItemParam(0, 2, partsNo, 0);
	menuPortItem->SetTableForItemParam(0, 3, partsNo, 1);
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_LuidaRoot(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(223, 6, 0.f, 0.f, -2.f);
	menuParts->AddTextMesh(Vector3(11.5f, 4.f, 186.f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft);
	menuPortItem->AddParts(menuParts);
	menuPortItem->SetTableForItemParam(6, 0, menuPortItem->AddParts(menuParts), 1);
	Sub_ShopButton(menuPortItem, menuParts, 8.5f, 0, -145.f, 110.5f);
	Sub_ShopButton(menuPortItem, menuParts, 8.5f, 1, 0.f, 110.5f);
	Sub_ShopButton(menuPortItem, menuParts, 8.5f, 2, 145.f, 110.5f);
	MenuParts* menuParts2 = MenuParts::CreateButton(100.f, 24.f, 3, 32);
	menuParts2->Build(4, 7, 152.5f, 4.f, 33.5f, menuParts->transform());
	menuParts2->AddTextMesh(Vector3(0.f, 4.f, 0.f), 8.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuPortItem->SetTableForItemParam(3, 0, menuPortItem->AddParts(menuParts2), 1);
	return menuPortItem;
}

void MenuPortItemTable::Sub_MonsNameButton(MenuPortItem* item, MenuParts* parent, short no, float x, float y) {
	MenuParts* menuParts = MenuParts::CreateButton(160.f, 85.f, no, 16);
	menuParts->ChangeParts(1);
	menuParts->AddIcon(0, 32.f, 2.f, -4.f, 0, false, 0.85f);
	menuParts->Build(4, 7, x, 4.f, y, parent->transform());
	MenuParts::APPLY_HERO = 1;
	menuParts->AddTextMesh(Vector3(38.f, 4.f, 45.f), 9.5f, TextAlignment::Center, TextAnchor::UpperCenter);
	menuParts->AddTextMesh(Vector3(0.f, 4.f, 12.f), 9.5f, TextAlignment::Center, TextAnchor::UpperCenter);
	menuParts->AddTextMesh(Vector3(-88.f, 4.f, -12.f), 9.5f);
	menuParts->AddTextMesh(Vector3(-88.f, 4.f, -34.f), 9.5f);
	short partsNo = item->AddParts(menuParts);
	item->SetTableForCode(no, partsNo);
	item->SetTableForItemParam(no, 0, partsNo, 0);
	item->SetTableForItemParam(no, 1, partsNo, 1);
	item->SetTableForItemParam(no, 2, partsNo, 2);
	item->SetTableForItemParam(no, 3, partsNo, 3);
	item->SetTableForItemParam(no, 4, partsNo, 4);
	item->SetTableForItemParam(no, 5, partsNo, 5);
	item->SetTableForItemParamTextColorChanger(no, 6, menuParts);
}

MenuPortItem* MenuPortItemTable::Make_MonsG_Name(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(358, 6, 0.f, 0.f, -2.f);
	menuParts->AddTextMesh(Vector3(225.f, 4.f, 515.f), 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
	short num = menuPortItem->AddParts(menuParts);
	menuPortItem->SetTableForItemParam(6, 0, num, 1);
	Sub_MonsNameButton(menuPortItem, menuParts, 0, -106.f, 426.f);
	Sub_MonsNameButton(menuPortItem, menuParts, 1, 106.f, 426.f);
	Sub_MonsNameButton(menuPortItem, menuParts, 2, -106.f, 300.f);
	Sub_MonsNameButton(menuPortItem, menuParts, 3, 106.f, 300.f);
	Sub_MonsNameButton(menuPortItem, menuParts, 4, -106.f, 174.f);
	Sub_MonsNameButton(menuPortItem, menuParts, 5, 106.f, 174.f);
	Sub_ShopPageing(menuPortItem, menuParts, num, 6, 1, 6);
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_MonsG_MessageUpChar(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::Create(4);
	menuParts->AddSquare(Vector3::zero, 0, true, 56.f, 32.f);
	menuParts->ChangeParts(1);
	menuParts->AddIcon(4, 0.f, 2.f, 0.f, 0, false, 0.95f);
	menuParts->Build(0, eIndent::CalcPosition(MenuMessage::m_window->gameObject, 0, -2.f, 0.f, 54.f));
	menuPortItem->AddParts(menuParts);
	menuPortItem->SetTableForItemParam(0, 0, 0, 1);
	return menuPortItem;
}

Transform* MenuPortItemTable::Sub_SlimeTButton(MenuPortItem* item, MenuParts* parent, short bid, float x, float y) {
	MenuParts* menuParts = MenuParts::CreateButton(130.f, 24.f, bid, 16);
	menuParts->Build(4, 7, x, 4.f, y, parent->transform());
	menuParts->AddTextMesh(Vector3(0.f, 4.f, -1.5f), 9.5f, TextAlignment::Left, TextAnchor::MiddleCenter, 1.2f);
	item->SetTableForItemParam(bid, 0, item->AddParts(menuParts), 1);
	return menuParts->transform();
}

MenuPortItem* MenuPortItemTable::Make_SlimeTouch_Root(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(262, 6, 0.f, 0.f, -2.f);
	menuParts->AddTextMesh(Vector3(11.5f, 4.f, 240.f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft);
	menuPortItem->SetTableForItemParam(6, 0, menuPortItem->AddParts(menuParts), 1);
	Sub_SlimeTButton(menuPortItem, menuParts, 0, -95.700005f, 170.75f);
	Sub_SlimeTButton(menuPortItem, menuParts, 1, 95.700005f, 170.75f);
	Sub_SlimeTButton(menuPortItem, menuParts, 2, -95.700005f, 106.25f);
	MenuParts* menuParts2 = MenuParts::CreateButton(100.f, 24.f, 3, 32);
	menuParts2->Build(4, 7, 152.5f, 4.f, 33.5f, menuParts->transform());
	menuParts2->AddTextMesh(Vector3(0.f, 4.f, 0.f), 8.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuPortItem->SetTableForItemParam(3, 0, menuPortItem->AddParts(menuParts2), 1);
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_SlimeTouch_Score(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(359, 6, 0.f, 0.f, -2.f);
	short partsNo = menuPortItem->AddParts(menuParts);
	menuParts->AddTextMesh(7, 0.f, 4.f, 380.f, 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuParts->AddTextMesh(7, -160.f, 4.f, 347.f, 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuParts->AddTextMesh(7, 0.f, 4.f, 347.f, 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuParts->AddTextMesh(7, 145.f, 4.f, 347.f, 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuParts->AddTextMesh(7, -147.f, 4.f, 320.f, 9.f, TextAlignment::Right, TextAnchor::UpperRight, 1.325f);
	menuParts->AddTextMesh(7, 0.f, 4.f, 320.f, 9.f, TextAlignment::Center, TextAnchor::UpperCenter, 1.325f);
	menuParts->AddTextMesh(7, 209.5f, 4.f, 320.f, 9.f, TextAlignment::Right, TextAnchor::UpperRight, 1.325f);
	menuPortItem->SetTableForItemParam(0, 0, partsNo, 1);
	menuPortItem->SetTableForItemParam(0, 1, partsNo, 2);
	menuPortItem->SetTableForItemParam(0, 2, partsNo, 3);
	menuPortItem->SetTableForItemParam(0, 3, partsNo, 4);
	menuPortItem->SetTableForItemParam(0, 4, partsNo, 5);
	menuPortItem->SetTableForItemParam(0, 5, partsNo, 6);
	menuPortItem->SetTableForItemParam(0, 6, partsNo, 7);
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
	return menuPortItem;
}

MenuParts* MenuPortItemTable::Sub_SlimeRace_Bet(MenuPortItem* item, MenuParts* frame, short bid, short xx, short yy) {
	MenuParts* menuParts = MenuParts::CreateButton(152.f, 32.f, bid, 16);
	menuParts->Build(4, 7, (xx != 0) ? 113.f : (-113.f), 4.f, 422.f + -75.f * (float)yy, frame->transform());
	item->SetTableForItemParam(bid, 0, std::make_shared<IBetText>(menuParts->AddTextMesh(Vector3(86.5f, 4.f, -5.5f), 9.f, TextAlignment::Right, TextAnchor::MiddleRight, 1.6f)->GetComponent<TextMesh>()));
	item->AddParts(menuParts);
	return menuParts;
}

MenuPortItem* MenuPortItemTable::Make_SlimeRace_BetA(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(360, 6, 0.f, 0.f, -2.f);
	menuPortItem->AddParts(menuParts);
	menuParts->AddTextMesh(7, 0.f, 4.f, 505.f, 9.f, TextAlignment::Left, TextAnchor::UpperCenter, 1.2f);
	menuParts->AddTextMesh(6, 12.f, 4.f, 55.f, 9.f, TextAlignment::Left, TextAnchor::UpperLeft, 1.2f);
	menuPortItem->SetTableForItemParam(0, 0, 0, 1);
	menuPortItem->SetTableForItemParam(0, 5, 0, 2);
	MenuParts* p = MenuParts::CreateButton(68.5f, 23.5f, 0, 16);
	p->SetEnableButtonTextColorChange();
	p->Build(4, 7, 98.5f, 4.f, 34.f, menuParts->transform());
	menuPortItem->SetTableForCode(20, [=](short code) mutable
	{
		p->m_list[0]->m_intChanger((code == 0) ? 3 : 0);
	});
	p->AddTextMesh(Vector3(0.f, 8.f, 0.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuPortItem->SetTableForItemParam(0, 3, menuPortItem->AddParts(p), 1);
	short num = 0;
	short num2 = 1;
	while (num < 2)
	{
		short num3 = 0;
		while (num3 < 5)
		{
			Sub_SlimeRace_Bet(menuPortItem, menuParts, num2, num, num3);
			num3++;
			num2++;
		}
		num++;
	}
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_SlimeRace_BetB(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(360, 6, 0.f, 0.f, -2.f);
	menuPortItem->AddParts(menuParts);
	menuParts->AddTextMesh(7, 0.f, 4.f, 505.f, 9.f, TextAlignment::Left, TextAnchor::UpperCenter, 1.2f);
	menuParts->AddTextMesh(7, 0.f, 4.f, 95.f, 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuParts->AddTextMesh(6, 12.f, 4.f, 55.f, 9.f, TextAlignment::Left, TextAnchor::UpperLeft, 1.2f);
	menuPortItem->SetTableForItemParam(0, 0, 0, 1);
	menuPortItem->SetTableForItemParam(0, 4, 0, 2);
	menuPortItem->SetTableForItemParam(0, 5, 0, 3);
	MenuParts* _p;
	MenuParts* p = (_p = MenuParts::CreateButton(68.5f, 23.5f, 0, 16));
	p->SetEnableButtonTextColorChange();
	p->Build(4, 7, 98.5f, 4.f, 34.f, menuParts->transform());
	menuPortItem->SetTableForCode(20, [=](short code) mutable
	{
		_p->m_list[0]->m_intChanger((code == 0) ? 3 : 0);
	});
	p->AddTextMesh(Vector3(0.f, 8.f, 0.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuPortItem->SetTableForItemParam(0, 3, menuPortItem->AddParts(p), 1);
	p = MenuParts::CreateButtonIcon(10, 0, 1.f, 0, 4);
	p->Build(4, 7, -176.f, 4.f, 95.f, menuParts->transform());
	menuPortItem->AddParts(p);
	p = MenuParts::CreateButtonIcon(10, 1, 1.f, 0, 8);
	p->Build(4, 7, 176.f, 4.f, 95.f, menuParts->transform());
	menuPortItem->AddParts(p);
	short num = 0;
	short num2 = 1;
	while (num < 2)
	{
		short num3 = 0;
		while (num3 < 4)
		{
			p = Sub_SlimeRace_Bet(menuPortItem, menuParts, num2, num, num3);
			num3++;
			num2++;
		}
		num++;
	}
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
	menuPortItem->SetTableForCode(0, [=](short code) mutable
	{
		p->gameObject->SetActiveRecursively(code == 0);
	});
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_SlimeRace_BetC(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(364, 6, 0.f, 0.f, -2.f);
	short num = menuPortItem->AddParts(menuParts);
	menuParts->AddTextMesh(Vector3(12.5f, 4.f, 238.5f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.15f);
	auto ibase = std::make_shared<IBetText>(menuParts->AddTextMesh(Vector3(215.f, 4.f, 239.5f), 8.5f, TextAlignment::Center, TextAnchor::MiddleCenter)->GetComponent<TextMesh>());
	menuParts->AddTextMesh(Vector3(445.f, 4.f, 238.5f), 9.f, TextAlignment::Left, TextAnchor::MiddleRight, 1.15f);
	menuParts->AddTextMesh(Vector3(205.5f, 4.f, 150.f), 9.f, TextAlignment::Right, TextAnchor::UpperRight);
	menuParts->AddTextMesh(6, 12.f, 4.f, 55.f, 9.f, TextAlignment::Left, TextAnchor::UpperLeft, 1.2f);
	Sub_CasinoUD(menuPortItem, 0, 1, menuParts, num, false, 256.f);
	Sub_CasinoUD(menuPortItem, 1, 10, menuParts, num, false, 256.f);
	Sub_CasinoStart(menuPortItem, 0, 16, menuParts->transform(), 0, 3);
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
	menuPortItem->SetTableForItemParam(0, 0, num, 1);
	menuPortItem->SetTableForItemParam(0, 1, ibase);
	menuPortItem->SetTableForItemParam(0, 2, num, 3);
	menuPortItem->SetTableForItemParam(0, 4, num, 4);
	menuPortItem->SetTableForItemParam(0, 5, num, 5);
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_SlimeRace_Result(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(361, 4, 0.f, 0.f, -2.f);
	menuPortItem->AddParts(menuParts);
	menuParts->AddTextMesh(4, 0.f, 4.f, 57.f, 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
	MenuParts::SetTextColor(menuParts->AddTextMesh(4, 0.f, 4.f, 8.f, 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter)->renderer(), 1);
	menuPortItem->SetTableForItemParam(0, 0, 0, 1);
	menuPortItem->SetTableForItemParam(0, 1, 0, 2);
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_SlimeRace_Coin(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateWindow0(148.f, 35.f);
	menuParts->Build(2, 2, 2.f, 0.f, 2.f);
	menuParts->AddTextMesh(Vector3(-10.f, 4.f, -30.f), 9.5f, TextAlignment::Right, TextAnchor::UpperRight);
	menuParts->AddTextMesh(Vector3(-161.f, 4.f, -8.f), 9.5f);
	menuPortItem->AddParts(menuParts);
	menuPortItem->SetTableForItemParam(0, 0, 0, 1);
	menuPortItem->SetTableForItemParam(0, 1, 0, 2);
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_SlimeRace_MoneyCoin(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(362, 2, 0.f, 0.f, 0.f);
	menuParts->AddTextMesh(Vector3(-10.f, 4.f, -19.f), 9.5f, TextAlignment::Right, TextAnchor::UpperRight);
	menuParts->AddTextMesh(Vector3(-10.f, 4.f, -81.f), 9.5f, TextAlignment::Right, TextAnchor::UpperRight);
	menuParts->AddTextMesh(Vector3(-161.f, 4.f, -59.f), 9.5f);
	menuPortItem->AddParts(menuParts);
	menuPortItem->SetTableForItemParam(0, 0, 0, 1);
	menuPortItem->SetTableForItemParam(0, 1, 0, 2);
	menuPortItem->SetTableForItemParam(0, 2, 0, 3);
	return menuPortItem;
}

void MenuPortItemTable::Sub_SlimeEntryButton(MenuPortItem* item, MenuParts* parent, short no, float x, float y) {
	MenuParts* menuParts = MenuParts::CreateButton(152.f, 69.f, no, 16);
	menuParts->ChangeParts(1);
	menuParts->AddIcon(0, 10.f, 2.f, -4.f, 0, false);
	menuParts->ChangeParts(13, true);
	Material* nowMaterial = menuParts->m_maker->GetNowMaterial();
	menuParts->AddIcon(0, 46.f, 2.f, 0.f, 0, false, 1.1f);
	menuParts->AddIcon(0, 46.f, 2.f, -18.f, 0, false, 1.1f);
	menuParts->Build(4, 7, x, 4.f, y, parent->transform());
	MenuParts::APPLY_HERO = 1;
	menuParts->AddTextMesh(Vector3(28.f, 4.f, 21.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter, 1.1f);
	menuParts->AddTextMesh(Vector3(1.f, 4.f, -23.5f), 8.5f, TextAlignment::Center, TextAnchor::MiddleCenter, 1.1f);
	short partsNo = item->AddParts(menuParts);
	item->SetTableForCode(no, partsNo);
	item->SetTableForItemParam(no, 0, partsNo, 1);
	item->SetTableForItemParam(no, 1, partsNo, 4);
	item->SetTableForItemParam(no, 2, partsNo, 5);
	item->SetTableForItemParam(no, 3, partsNo, 0);
	item->SetTableForItemParamTextColorChanger(no, 4, menuParts);
	item->SetTableForItemParam(no, 5, std::make_shared<StatusIconChanger>(nowMaterial, menuParts->m_list[2], menuParts->m_list[3]));
}

MenuPortItem* MenuPortItemTable::Make_SlimeRace_Entry(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(363, 6, 0.f, 0.f, -2.f);
	menuParts->AddTextMesh(7, 0.f, 4.f, 371.5f, 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuPortItem->AddParts(menuParts);
	menuPortItem->SetTableForItemParam(4, 0, 0, 1);
	Sub_ShopPageing(menuPortItem, menuParts, 0, 4, 1, 4);
	Sub_SlimeEntryButton(menuPortItem, menuParts, 0, -104.f, 295.f);
	Sub_SlimeEntryButton(menuPortItem, menuParts, 1, 104.f, 295.f);
	Sub_SlimeEntryButton(menuPortItem, menuParts, 2, -104.f, 169.f);
	Sub_SlimeEntryButton(menuPortItem, menuParts, 3, 104.f, 169.f);
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
	return menuPortItem;
}
