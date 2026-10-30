// game/MenuPortItemTable_Main.cpp -- OWNER: WP4b.  C# source: re/cs_out/Assembly-CSharp/MenuPortItemTable.cs (first pass by tools/menu_cs2cpp.py, then fixed by hand)
#include "game/MenuPortItemTable.hpp"
#include "game/MeshChara.hpp"
#include "game/MainLoop.hpp"
#include "utils/logger.h"
#include <cstdio>

MenuPortItem* MenuPortItemTable::Make_LeftCharaList(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildDummy(6, 0.f, 0.f, 0.f);
	menuPortItem->AddParts(menuParts);
	for (short num = 0; num < 4; num++)
	{
		Sub_CharaButton(menuPortItem, num, menuParts->transform(), 6, 5.f, 4.f, 429 - 92 * num);
	}
	MenuParts* up = MenuParts::CreateButton(120.f, 24.f, -1, 1);
	up->ChangeParts(8, false, true);
	up->AddIcon(4, 0.f, 4.f, 0.f, 0, false, 0.5f);
	up->Build(4, 6, 80.f, 4.f, 462.5f, menuParts->transform());
	menuPortItem->AddParts(up);
	MenuParts* dn = MenuParts::CreateButton(120.f, 24.f, -1, 2);
	dn->ChangeParts(8, false, true);
	dn->AddIcon(4, 0.f, 4.f, 0.f, 1, false, 0.5f);
	dn->Build(4, 6, 80.f, 4.f, 34.f, menuParts->transform());
	menuPortItem->AddParts(dn);
	menuPortItem->SetTableForCode(4, [=](short code) mutable
	{
		if (code != 0)
		{
			up->transform()->setPosition(eIndent::CalcScreenPosition(6, 80.f, 4.f, 462.5f - (float)(92 * (code - 1))));
			up->gameObject->SetActiveRecursively(true);
			dn->gameObject->SetActiveRecursively(true);
		}
		else
		{
			up->gameObject->SetActiveRecursively(false);
			dn->gameObject->SetActiveRecursively(false);
		}
	});
	MenuParts* menuParts2 = MenuParts::CreateBuildFrameParts(-1, 6, 0.f, 0.f, -2.f, true, true);
	menuParts2->SetButtonColliderOut(-1, 128);
	menuPortItem->SetTableForItemParam(4, 0, menuPortItem->AddParts(menuParts2), 0);
	menuPortItem->AddParts(menuParts2);
	return menuPortItem;
}

MenuParts* MenuPortItemTable::Sub_CharaButton(MenuPortItem* item, short no, Transform* parent, int indent, float x, float y, float z) {
	MenuParts* menuParts = MenuParts::CreateButton(120.f, 53.f, no, 16);
	menuParts->ChangeParts(1);
	menuParts->AddIcon(0, 12.f, 2.f, 0.f, 0, false, 0.95f);
	menuParts->ChangeParts(13, true);
	Material* nowMaterial = menuParts->m_maker->GetNowMaterial();
	menuParts->AddIcon(0, 45.f, 2.f, 0.f, 0, false, 1.1f);
	menuParts->AddIcon(0, 45.f, 2.f, -18.f, 0, false, 1.1f);
	menuParts->Build(0, indent, x, y, z, parent);
	MenuParts::APPLY_HERO = 1;
	menuParts->AddTextMesh(Vector3(102.f, 4.f, -15.f), 9.5f, TextAlignment::Center, TextAnchor::UpperCenter);
	menuParts->AddTextMesh(Vector3(8.f, 4.f, -43.f), 9.f);
	menuParts->AddTextMesh(Vector3(8.f, 4.f, -62.f), 9.f);
	menuParts->AddTextMesh(Vector3(2.f, 4.f, -2.f), 8.5f);
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
	return menuParts;
}

MenuPortItem* MenuPortItemTable::Make_TargetCharaList(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = Sub_BackButton(menuPortItem);
	for (short num = 0; num < 3; num++)
	{
		Sub_CharaButton(menuPortItem, num, menuParts->transform(), 8, -222.25f, 4.f, 414 - 98 * num);
	}
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_NameButton(short port, short type) {
	MenuPortItem* item = new MenuPortItem(port, type);
	MenuParts* menuParts = Sub_BackButton(item);
	Renderer* textList[6] = {};
	for (short num = 0; num < 6; num++)
	{
		textList[num] = Sub_NameButton(item, num, menuParts->transform(), 382, 67, num < 4);
	}
	menuParts = nullptr;
	item->SetTableForCode(6, [=](short code) mutable
	{
		short num2 = name_button_param[code].num;
		float num3 = name_button_param[code].start;
		float diffy = name_button_param[code].diffy;
		for (code = 0; code < num2; code++)
		{
			MenuParts* menuParts2 = item->m_partsList[1 + code];
			menuParts2->gameObject->SetActiveRecursively(true);
			menuParts2->transform()->setPosition(eIndent::CalcScreenPosition(8, -146.f, 4.f, num3));
			menuParts2->m_button->m_intChanger(0);
			MenuParts::SetTextColor(textList[code], 0);
			num3 -= diffy;
		}
		while (code < 6)
		{
			item->m_partsList[1 + code]->gameObject->SetActiveRecursively(false);
			code++;
		}
	});
	item->SetTableForCode(7, [=](short code) mutable
	{
		for (short num2 = 0; num2 < 6; num2++)
		{
			MenuParts* menuParts2 = item->m_partsList[1 + num2];
			if (menuParts2->gameObject->active())
			{
				if (num2 == code)
				{
					menuParts2->m_button->m_intChanger(4);
					MenuParts::SetTextColor(textList[num2], 4);
				}
				else
				{
					menuParts2->m_button->m_intChanger(0);
					MenuParts::SetTextColor(textList[num2], 0);
				}
			}
		}
	});
	item->SetTableForCode(8, [=](short code) mutable
	{
		bool se = code != 0;
		for (int i = 1; i < 7; i++)
		{
			item->m_partsList[i]->m_se = se;
		}
	});
	return item;
}

Renderer* MenuPortItemTable::Sub_NameButton(MenuPortItem* item, short no, Transform* parent, int start, int diffy, bool bSubText) {
	MenuParts* p = MenuParts::CreateButton(166.f, 18.f, no, 16);
	p->Build(4, 8, -146.f, 4.f, start - diffy * no, parent);
	MenuParts::APPLY_HERO = 1;
	GameObject* text = p->AddTextMesh(Vector3(0.35f, 4.f, -0.15f), 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter, 1.05f);
	if (bSubText)
	{
		if (GetLangages::m_langagesNumber == 1)
		{
			GameObject* gameObject = p->AddTextMesh(Vector3(23.f, 4.f, -15.f), 6.f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.05f);
			gameObject->GetComponent<TextMesh>()->tabSize = 8.f;
			MenuParts::SetTextColor(gameObject->renderer(), 6);
		}
		else
		{
			MenuParts::SetTextColor(p->AddTextMesh(Vector3(94.f, 4.f, -15.f), 6.f, TextAlignment::Right, TextAnchor::MiddleRight, 1.05f)->renderer(), 6);
		}
	}
	short partsNo = item->AddParts(p);
	item->SetTableForCode(no, [=](short code) mutable
	{
		if (code != 0)
		{
			text->transform->localPosition = Vector3(0.35f, 4.f, MenuParts::BIAS_FONT_Y + ((code != 2) ? (-0.15f) : 5.f));
		}
		p->gameObject->SetActiveRecursively(code != 0);
	});
	item->SetTableForItemParam(no, 0, partsNo, 1);
	if (bSubText)
	{
		item->SetTableForItemParam(no, 1, partsNo, 2);
	}
	return text->renderer();
}

MenuPortItem* MenuPortItemTable::Make_MagicInfo(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildDummy(6, -2.f, 8.f, -2.f);
	menuParts->AddTextMesh(Vector3(82.5f, 4.f, 465.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuParts->AddTextMesh(Vector3(14.f, 4.f, 58.f), 9.f, TextAlignment::Left, TextAnchor::UpperLeft, 1.2f);
	menuParts->AddTextMesh(Vector3(302.f, 4.f, 485.f), 9.f, TextAlignment::Left, TextAnchor::UpperLeft, 1.1f);
	menuParts->AddTextMesh(Vector3(226.f, 4.f, 465.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuPortItem->AddParts(menuParts);
	menuPortItem->SetTableForItemParam(0, 0, 0, 0);
	menuPortItem->SetTableForItemParam(0, 1, 0, 1);
	menuPortItem->SetTableForItemParam(0, 2, 0, 2);
	menuPortItem->SetTableForItemParam(0, 3, 0, 3);
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_ItemList(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = Sub_BackButton(menuPortItem);
	Sub_ItemButton(menuPortItem, 0, menuParts->transform());
	Sub_ItemButton(menuPortItem, 1, menuParts->transform());
	Sub_ItemButton(menuPortItem, 2, menuParts->transform());
	Sub_ItemButton(menuPortItem, 3, menuParts->transform());
	return menuPortItem;
}

void MenuPortItemTable::Sub_ItemButton(MenuPortItem* item, short no, Transform* parent) {
	MenuParts* p = MenuParts::CreateButton(170.f, 22.f, no, 16);
	p->ChangeParts(0);
	p->AddIcon(3, 18.f, 4.f, 0.f, 0, false, 0.8f);
	p->ChangeParts(6);
	p->AddIcon(6, 10.f, 6.f, 0.f, 0, false, 0.6f);
	p->ChangeParts(12);
	p->AddIcon(6, 0.f, 6.f, 0.f, 1, false, 0.6f);
	p->AddIcon(6, 10.f, 6.f, 0.f, 1, false, 0.6f);
	p->Build(4, 8, -146.f, 4.f, 382 - 67 * no, parent);
	Transform* txtrns = p->AddTextMesh(Vector3(-38.f, 4.f, 0.f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.05f)->transform;
	p->SetEnableButtonTextColorChange();
	short partsNo = item->AddParts(p);
	item->SetTableForCode(no, [=](short code) mutable
	{
		if (code > 0)
		{
			p->gameObject->SetActiveRecursively(true);
			p->m_button->m_intChanger(0 + code - 1);
		}
		else
		{
			switch (code)
			{
			case -2:
				txtrns->localPosition = Vector3(-38.f, 4.f, MenuParts::BIAS_FONT_Y);
				txtrns->GetComponent<TextMesh>()->anchor = (int)TextAnchor::MiddleLeft;
				break;
			case -3:
				txtrns->localPosition = Vector3(0.f, 4.f, MenuParts::BIAS_FONT_Y);
				txtrns->GetComponent<TextMesh>()->anchor = (int)TextAnchor::MiddleCenter;
				break;
			default:
				p->gameObject->SetActiveRecursively(false);
				break;
			}
		}
	});
	item->SetTableForItemParam(no, 0, partsNo, 1);
	item->SetTableForItemParam(no, 1, partsNo, 2);
	item->SetTableForItemParam(no, 2, partsNo, 5);
	item->SetTableForItemParam(no, 3, std::make_shared<ItemNumCTRL>(p->m_list[3]->m_intChanger, p->m_list[4]->m_intChanger));
}

MenuPortItem* MenuPortItemTable::Make_ItemInfo(short port, short type) {
	float num = 458.f;
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::Create(0);
	menuParts->AddIcon(0, 0.f, 0.f, 0.f, 4, true, 0.8f);
	menuParts->Build(4, 6, 26.f, 8.f, 458.f);
	MenuParts::APPLY_HERO = 2;
	menuParts->AddTextMesh(Vector3(62.75f, 0.f, 500.f - num), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuParts->AddTextMesh(Vector3(21.f, 0.f, 457.5f - num), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.15f);
	menuParts->AddTextMesh(Vector3(240.5f, 0.f, 471.5f - num), 8.75f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.25f);
	menuParts->AddTextMesh(Vector3(-12.f, 0.f, 56.f - num), 9.f, TextAlignment::Left, TextAnchor::UpperLeft, 1.2f);
	GameObject* useText = menuParts->AddTextMesh(Vector3(190.f, 0.f, 471.5f - num), 8.75f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuPortItem->AddParts(menuParts);
	menuPortItem->SetTableForItemParam(0, 0, 0, 0);
	menuPortItem->SetTableForItemParam(0, 1, 0, 1);
	menuPortItem->SetTableForItemParam(0, 2, 0, 2);
	menuPortItem->SetTableForItemParam(0, 3, 0, 3);
	menuPortItem->SetTableForItemParam(0, 4, 0, 4);
	menuPortItem->SetTableForItemParam(0, 5, 0, 5);
	menuPortItem->SetTableForCode(0, [=](short code) mutable
	{
		useText->setActive(code != 0);
	});
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_BCInfo(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateButton(156.f, 12.f, 0, 16);
	menuParts->Build(4, 8, -181.f, 4.f, 34.f);
	menuParts->AddTextMesh(Vector3(0.f, 4.f, -0.15f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuParts->SetEnableButtonTextColorChange();
	menuPortItem->AddParts(menuParts);
	menuPortItem->SetTableForItemParam(0, 0, 0, 0);
	menuPortItem->SetTableForItemParam(0, 1, 0, 1);
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_Status(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* modoru = Sub_BackButton(menuPortItem);
	short num = (short)modoru->m_list.size();
	switch (type)
	{
	case 0:
		if (GetLangages::m_langagesNumber == 2)
		{
			MenuParts::APPLY_HERO = 1;
			modoru->AddTextMesh(Vector3(-236.f, 4.f, 466.f), 9.5f, TextAlignment::Left, TextAnchor::MiddleLeft);
		}
		else
		{
			modoru->AddTextMesh(Vector3(-236.f, 4.f, 476.f), 9.5f);
		}
		modoru->AddTextMesh(Vector3(19.f, 4.f, 476.f), 9.5f, TextAlignment::Right, TextAnchor::UpperRight);
		modoru->AddTextMesh(Vector3(-236.f, 4.f, 435.f), 9.f, TextAlignment::Left, TextAnchor::UpperLeft, 1.51f);
		modoru->AddTextMesh(Vector3(-141.f, 4.f, 435.f), 9.f, TextAlignment::Left, TextAnchor::UpperLeft, 1.51f);
		break;
	case 1:
		modoru->AddTextMesh(Vector3(-108.5f, 4.f, 476.f), 9.5f, TextAlignment::Center, TextAnchor::UpperCenter);
		modoru->AddTextMesh(Vector3(-236.f, 4.f, 435.f), 9.f, TextAlignment::Left, TextAnchor::UpperLeft, 1.13f);
		break;
	case 2:
		modoru->AddTextMesh(Vector3(-108.5f, 4.f, 476.f), 9.5f, TextAlignment::Center, TextAnchor::UpperCenter);
		modoru->AddTextMesh(Vector3(-241.f, 4.f, 435.f), 8.5f, TextAlignment::Left, TextAnchor::UpperLeft, 1.24f);
		modoru->AddTextMesh(Vector3(-105.f, 4.f, 435.f), 8.5f, TextAlignment::Left, TextAnchor::UpperLeft, 1.24f);
		break;
	case 3:
		modoru->AddTextMesh(Vector3(-236.f, 4.f, 543.f), 9.5f, TextAlignment::Left, TextAnchor::UpperLeft, 2.76f);
		modoru->AddTextMesh(Vector3(9.5f, 4.f, 520.f), 9.5f, TextAlignment::Right, TextAnchor::UpperRight, 2.76f);
		break;
	case 4:
		modoru->AddTextMesh(Vector3(-244.f, 4.f, 544.f), 8.5f, TextAlignment::Left, TextAnchor::UpperLeft, 4.88f);
		MenuParts::APPLY_HERO = 2;
		modoru->AddTextMesh(Vector3(-203.5f, 4.f, 544.f), 8.5f, TextAlignment::Left, TextAnchor::UpperLeft, 4.88f);
		if (GetLangages::m_langagesNumber < 3)
		{
			modoru->AddTextMesh(Vector3(-52.f, 4.f, 544.f), 8.5f, TextAlignment::Left, TextAnchor::UpperLeft, 4.88f);
		}
		else
		{
			modoru->AddTextMesh(Vector3(-62.f, 4.f, 544.f), 8.5f, TextAlignment::Left, TextAnchor::UpperLeft, 4.88f);
		}
		modoru->AddTextMesh(Vector3(-239.f, 4.f, 521.5f), 8.5f, TextAlignment::Left, TextAnchor::UpperLeft, 1.22f);
		break;
	}
	num = (short)(modoru->m_list.size() - num);
	for (short num2 = 0; num2 < num; num2++)
	{
		menuPortItem->SetTableForItemParam(0, num2, 0, (short)(2 + num2));
	}
	menuPortItem->SetTableForItemParamTextColorChanger(1, 0, modoru);
	menuPortItem->SetTableForCode(0, [=](short code) mutable
	{
		modoru->m_list[0]->m_intChanger(2);
		modoru->m_list[1]->m_intChanger(29);
	});
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_OperationItemList(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* parts = MenuParts::CreateBuildTextMenu(7, -55.5f, 4.f, 422.f, nullptr, 9.5f, TextAlignment::Left, TextAnchor::UpperLeft, 1.49f);
	menuPortItem->AddParts(parts);
	menuPortItem->SetTableForItemParam(0, 0, 0, 0);
	menuPortItem->SetTableForItemParamTextColorChanger(0, 1, parts);
	Sub_BackButton(menuPortItem);
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_Senreki(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildTextMenu(6, 17.5f, 4.f, 457.f, nullptr, 9.f, TextAlignment::Left, TextAnchor::UpperLeft, 1.7f);
	menuParts->AddTextMesh(6, 414.f, 4.f, 457.f, 9.f, TextAlignment::Right, TextAnchor::UpperRight, 1.7f);
	menuParts->AddTextMesh(6, 116.5f, 4.f, 152.f, 11.f);
	menuPortItem->AddParts(menuParts);
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
	GameObject* page = menuParts->AddTextMesh(7, 0.f, 4.f, 95.f, 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
	page->transform->SetParent(menuParts->transform());
	menuPortItem->SetTableForCode(0, [=](short code) mutable
	{
		page->SetActiveRecursively(code != 0);
	});
	MenuParts* menuParts2 = MenuParts::CreateButtonIcon(10, 0, 1.f, -1, 4);
	menuParts2->Build(4, 7, -176.f, 4.f, 95.f, page->transform);
	menuPortItem->AddParts(menuParts2);
	menuParts2 = MenuParts::CreateButtonIcon(10, 1, 1.f, -1, 8);
	menuParts2->Build(4, 7, 176.f, 4.f, 95.f, page->transform);
	menuPortItem->AddParts(menuParts2);
	menuPortItem->SetTableForItemParam(0, 0, 0, 0);
	menuPortItem->SetTableForItemParam(0, 1, 0, 1);
	menuPortItem->SetTableForItemParam(0, 2, 0, 2);
	menuPortItem->SetTableForItemParam(0, 3, 0, 3);
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_Config(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildTextMenu(8, -368.5f, 8.f, 462.5f, nullptr, 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuParts->AddTextMesh(8, -146.f, 8.f, 461.5f, 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuParts->AddTextMesh(8, -146.f, 8.f, 96.f, 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuPortItem->AddParts(menuParts);
	menuPortItem->SetTableForItemParam(0, 0, 0, 0);
	menuPortItem->SetTableForItemParam(0, 1, 0, 1);
	menuPortItem->SetTableForItemParam(0, 2, 0, 2);
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_MemberShiftButton(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildDummy(6, 0.f, 0.f, 0.f);
	menuPortItem->AddParts(menuParts);
	GameObject* text = menuParts->AddTextMesh(Vector3(0.f, 4.f, 0.f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.2f);
	menuPortItem->SetTableForItemParam(4, 0, 0, 0);
	menuPortItem->SetTableForCode(4, [=](short code) mutable
	{
		text->transform->localPosition = Vector3(20.f, 4.f, 462 - 92 * code);
	});
	for (short num = 0; num < 3; num++)
	{
		MenuParts* menuParts2 = MenuParts::CreateButton(234.f, 85.f, num, 16);
		menuParts2->ChangeParts(1);
		menuParts2->AddIcon(0, 10.f, 2.f, 0.f, 0, false, 0.95f);
		menuParts2->ChangeParts(13, true);
		Material* nowMaterial = menuParts2->m_maker->GetNowMaterial();
		menuParts2->AddIcon(0, 43.f, 2.f, 0.f, 0, false, 1.1f);
		menuParts2->AddIcon(0, 43.f, 2.f, -18.f, 0, false, 1.1f);
		menuParts2->Build(0, 6, 171.f, 4.f, 516 - 130 * num, menuParts->transform());
		MenuParts::APPLY_HERO = 1;
		menuParts2->AddTextMesh(Vector3(58.f, 4.f, -15.f), 9.5f);
		menuParts2->AddTextMesh(Vector3(11.f, 4.f, -45.5f), 9.f, TextAlignment::Left, TextAnchor::UpperLeft, 1.2f);
		menuParts2->AddTextMesh(Vector3(2.f, 4.f, -2.f), 8.5f);
		menuParts2->AddTextMesh(Vector3(159.f, 4.f, -15.f), 9.5f);
		short partsNo = menuPortItem->AddParts(menuParts2);
		menuPortItem->SetTableForCode(num, partsNo);
		menuPortItem->SetTableForItemParam(num, 0, partsNo, 1);
		menuPortItem->SetTableForItemParam(num, 1, partsNo, 4);
		menuPortItem->SetTableForItemParam(num, 2, partsNo, 5);
		menuPortItem->SetTableForItemParam(num, 3, partsNo, 6);
		menuPortItem->SetTableForItemParamTextColorChanger(num, 4, menuParts2);
		menuPortItem->SetTableForItemParam(num, 5, std::make_shared<StatusIconChanger>(nowMaterial, menuParts2->m_list[2], menuParts2->m_list[3]));
		menuPortItem->SetTableForItemParam(num, 6, partsNo, 7);
	}
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_MemberShift(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(268, 6, 0.f, 0.f, -2.f, true);
	menuParts->SetButtonColliderOut(-1, 128);
	menuPortItem->AddParts(menuParts);
	for (short num = 0; num < 4; num++)
	{
		MenuParts* menuParts2 = MenuParts::Create(1);
		menuParts2->AddIcon(0, 10.f, 0.f, 0.f, 0, false, 0.95f);
		menuParts2->ChangeParts(13, true);
		Material* nowMaterial = menuParts2->m_maker->GetNowMaterial();
		menuParts2->AddIcon(0, 44.f, 2.f, 0.f, 0, false, 1.1f);
		menuParts2->AddIcon(0, 44.f, 2.f, -18.f, 0, false, 1.1f);
		menuParts2->Build(0, 6, 5.f, 4.f, 519.f - 130.f * (float)num, menuParts->transform());
		MenuParts::APPLY_HERO = 1;
		menuParts2->AddTextMesh(Vector3(63.f, 0.f, -2.f), 9.5f);
		menuParts2->AddTextMesh(Vector3(14.f, 0.f, -24.f), 9.f, TextAlignment::Left, TextAnchor::UpperLeft, 1.02f)->GetComponent<TextMesh>()->tabSize = ((GetLangages::m_langagesNumber != 2) ? 8 : 5);
		menuParts2->AddTextMesh(Vector3(1.f, 4.f, -1.f), 8.5f);
		short partsNo = menuPortItem->AddParts(menuParts2);
		menuPortItem->SetTableForCode(num, partsNo);
		menuPortItem->SetTableForItemParam(num, 0, partsNo, 0);
		menuPortItem->SetTableForItemParam(num, 1, partsNo, 3);
		menuPortItem->SetTableForItemParam(num, 2, partsNo, 4);
		menuPortItem->SetTableForItemParam(num, 3, partsNo, 5);
		menuPortItem->SetTableForItemParamTextColorChanger(num, 4, menuParts2);
		menuPortItem->SetTableForItemParam(num, 5, std::make_shared<StatusIconChanger>(nowMaterial, menuParts2->m_list[1], menuParts2->m_list[2]));
	}
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_MapButton(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateWindow0(54.f, (type != 0) ? 47 : 112);
	menuParts->Build(8, 8, 2.f, 0.f, -2.f);
	menuPortItem->AddParts(menuParts);
	Sub_BackButton(menuPortItem, true)->transform()->SetParent(menuParts->transform());
	if (type == 0)
	{
		MenuParts* menuParts2 = MenuParts::CreateButton(32.f, 23.5f, -1, 1);
		menuParts2->ChangeParts(3);
		menuParts2->AddIcon(4, 0.f, 4.f, 0.f, 2, false, 1.2f);
		menuParts2->Build(4, 6, 412.5f, 4.f, 100.f, menuParts->transform());
		menuPortItem->SetTableForItemParam(0, 0, menuPortItem->AddParts(menuParts2), 1);
	}
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_MessageStatus(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::Create(4);
	menuParts->AddSquare(Vector3::zero, 0, true, 140.f, 72.f);
	menuParts->ChangeParts(1);
	menuParts->AddIcon(0, 12.f, 2.f, -6.f, 3, false, 0.95f);
	menuParts->Build(0, eIndent::CalcPosition(MenuMessage::m_window->gameObject, 0, -2.f, 0.f, 94.f));
	menuParts->AddTextMesh(Vector3(105.f, 4.f, -21.f), 9.5f, TextAlignment::Center, TextAnchor::UpperCenter);
	menuParts->AddTextMesh(Vector3(15.f, 4.f, -49.f), 9.f);
	menuPortItem->AddParts(menuParts);
	menuPortItem->SetTableForItemParam(0, 0, 0, 1);
	menuPortItem->SetTableForItemParam(0, 1, 0, 2);
	menuPortItem->SetTableForItemParam(0, 2, 0, 3);
	menuPortItem->SetTableForItemParamTextColorChanger(0, 3, menuParts);
	return menuPortItem;
}
