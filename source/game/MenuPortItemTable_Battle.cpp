// game/MenuPortItemTable_Battle.cpp -- OWNER: WP4b.  C# source: re/cs_out/Assembly-CSharp/MenuPortItemTable.cs (first pass by tools/menu_cs2cpp.py, then fixed by hand)
#include "game/MenuPortItemTable.hpp"
#include "game/MeshChara.hpp"
#include "game/MainLoop.hpp"
#include "game/Boot.hpp"
#include "game/BattleUpCursor.hpp"
#include "game/HoppingNumber.hpp"
#include "game/BattleStatusLayout.hpp"
#include "game/UiCameras.hpp"
#include "game/Layout.hpp"
#include <algorithm>
#include "game/NativeManager.hpp"
#include "game/TouchPad.hpp"
#include "utils/logger.h"
#include <cstdio>

void MenuPortItemTable::Sub_BattleUpStatus(MenuPortItem* item, short no, Transform* parent) {
	float num = 0.f + 111.f * (float)no;
	ShakeFrame* frame;
	if (Boot::s_useFixedUpdate)
	{
		frame = MenuParts::CreateBuildFrameParts<ShakeFrame>(196, 0, num, 0.f, 0.f);
	}
	else
	{
		frame = MenuParts::CreateBuildFrameParts<ShakeFrame>(196, 0, num, 0.f, 0.f);
	}
	MenuParts* menuParts = MenuParts::Create(9);
	MenuParts::APPLY_HERO = 1;
	frame->AddTextMesh(Vector3(58.f, 4.f, -34.5f), 8.5f, TextAlignment::Center, TextAnchor::UpperCenter);
	frame->AddTextMesh(Vector3(17.f, 4.f, -61.f), 8.5f, TextAlignment::Left, TextAnchor::UpperLeft, 1.57f);
	if (GetLangages::m_langagesNumber == 1)
	{
		frame->AddTextMesh(Vector3(98.5f, 4.f, -61.f), 8.5f, TextAlignment::Right, TextAnchor::UpperRight, 1.57f);
	}
	else
	{
		frame->AddTextMesh(Vector3(43.5f, 4.f, -61.f), 8.5f, TextAlignment::Left, TextAnchor::UpperLeft, 1.55f);
	}
	MenuParts::APPLY_HERO = 1;
	frame->AddTextMesh(Vector3(57.5f, 4.f, -145.f), 8.f, TextAlignment::Center, TextAnchor::UpperCenter, 1.09f);
	int partsNo = item->AddParts(frame);
	item->SetTableForItemParam(no, 1, partsNo, 1);
	item->SetTableForItemParam(no, 2, partsNo, 2);
	item->SetTableForItemParam(no, 3, partsNo, 3);
	item->SetTableForItemParam(no, 4, partsNo, 4);
	MenuParts* menuParts2 = MenuParts::CreateIconTexture(0, 0.f, 0.f, 0.f, 28.800001f);
	menuParts2->Build(4, 0, 57.f + num, 0.f, -15.5f, frame->transform());
	item->SetTableForItemParam(no, 0, item->AddParts(menuParts2), 0);
	auto battleBar = std::make_shared<BattleBar>(menuParts->AddBar(0, 4, 8, 0.f, 0.f, 28.f, 66.f, 2.f));
	menuParts->AddBar(0, 6, -1, 0.f, 0.f, 0.f, 66.f, 2.f);
	frame->m_shakeEndCallback = [battleBar]() { battleBar->RedApply(); };
	menuParts->Build(4, 6, 0.f, 0.f, 0.f);
	menuParts->transform()->SetParent(frame->transform());
	menuParts->transform()->localPosition = Vector3(58.f, 4.f, -111.915f);
	partsNo = item->AddParts(menuParts);
	item->SetTableForItemParam(no, 5, battleBar);
	item->SetTableForItemParam(no, 6, partsNo, 1);
	frame->transform()->SetParent(parent);
	item->SetTableForCode(no, [=](short code) mutable
	{
		if (code == 8)
		{
			frame->m_shake = 8;
		}
		else if (code < 0)
		{
			frame->gameObject->SetActiveRecursively(false);
		}
		else
		{
			frame->gameObject->SetActiveRecursively(true);
			frame->SetTextColor(code);
		}
	});
}

MenuPortItem* MenuPortItemTable::Make_BattleUpStatus(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildDummy(1, 0.f, 0.f, 0.f);
	BattleUpCursor* cursor = MenuParts::Create<BattleUpCursor>(10, true);
	menuPortItem->SetTableForCode(4, menuPortItem->AddParts(menuParts));
	Sub_BattleUpStatus(menuPortItem, 0, menuParts->transform());
	Sub_BattleUpStatus(menuPortItem, 1, menuParts->transform());
	Sub_BattleUpStatus(menuPortItem, 2, menuParts->transform());
	Sub_BattleUpStatus(menuPortItem, 3, menuParts->transform());
	cursor->AddIcon(0, 0.f, 0.f, 0.f, 2, true, 0.5f);
	cursor->Build(4, 4, 0.f, 0.f, 0.f, menuParts->transform());
	menuPortItem->AddParts(cursor);
	if (BattleStatusLayout::Enabled()) BattleStatusLayout::Attach(menuParts->transform(), 4);    // landscape: larger status row on its own full-surface camera
	menuPortItem->SetTableForCode(5, [=](short code) mutable
	{
		if (code < 0)
		{
			cursor->gameObject->SetActiveRecursively(false);
		}
		else
		{
			cursor->gameObject->SetActiveRecursively(true);
			cursor->ForceUpdate();
			cursor->transform()->localPosition = Vector3(-200.f + 111.f * (float)code, 0.f, -15.f);
		}
	});
	return menuPortItem;
}

void MenuPortItemTable::Sub_BattleAction(MenuPortItem* item, short buttonId, int icon, Transform* parent, float x, float y, float z, int W, float H, int ix) {
	MenuParts* menuParts = MenuParts::CreateButton(W, H, buttonId, 16);
	menuParts->ChangeParts(3);
	menuParts->AddIcon(4, -49 + ix, 4.f, 0.f, icon, false, 0.8f);
	menuParts->Build(4, 7, x, y, z, parent);
	menuParts->AddTextMesh(eIndent::CalcLocalPosition(menuParts->gameObject, 7, 24 - ix, 4.f, 16.f), 9.5f, TextAlignment::Center, TextAnchor::LowerCenter);
	short partsNo = item->AddParts(menuParts);
	item->SetTableForCode(buttonId, partsNo);
	item->SetTableForItemParam(buttonId, 0, partsNo, 1);
	item->SetTableForItemParam(buttonId, 1, partsNo, 2);
}

MenuPortItem* MenuPortItemTable::Make_BattleRoot(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(197, 6, 0.f, 0.f, -2.f);
	menuPortItem->AddParts(menuParts);
	Sub_BattleAction(menuPortItem, 0, 10, menuParts->transform(), 0.f, 4.f, 182.f, 341, 20.f);
	Sub_BattleAction(menuPortItem, 1, 7, menuParts->transform(), 102.f, 4.f, 124.f, 136, 20.f);
	Sub_BattleAction(menuPortItem, 2, 16, menuParts->transform(), -102.f, 4.f, 124.f, 136, 20.f);
	Sub_BattleAction(menuPortItem, 3, 14, menuParts->transform(), 149.f, 4.f, 34.f, 109, 23.5f, 6);
	return menuPortItem;
}

Transform* MenuPortItemTable::Sub_BattleInfo(MenuPortItem* item, short buttonId, Transform* parent) {
	MenuParts* menuParts = MenuParts::CreateButton(136.f, 20.f, buttonId, 16);
	menuParts->ChangeParts(3);
	menuParts->Build(4, 7, 0.f, 0.f, 0.f, parent);
	menuParts->AddTextMesh(Vector3(0.f, 4.f, 0.f), 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
	short partsNo = item->AddParts(menuParts);
	item->SetTableForCode(buttonId, partsNo);
	item->SetTableForItemParam(buttonId, 0, partsNo, 1);
	return menuParts->transform();
}

MenuPortItem* MenuPortItemTable::Make_BattleInfo(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(200, 6, 0.f, 0.f, -2.f);
	Transform* buttons[2] = {};
	TextMesh* entext[2] = { nullptr, nullptr };
	if (GetLangages::m_langagesNumber == 1)
	{
	}
	menuPortItem->AddParts(menuParts);
	MenuParts::APPLY_HERO = 1;
	menuParts->AddTextMesh(Vector3(48.f, 4.f, 236.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuParts->AddTextMesh(Vector3(138.5f, 4.f, 236.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	float num = 9.f;
	if (GetLangages::m_langagesNumber == 1)
	{
		num *= 0.86f;
	}
	menuParts->AddTextMesh(Vector3(191.5f, 4.f, 236.f), num, TextAlignment::Left, TextAnchor::MiddleLeft);
	if (GetLangages::m_langagesNumber == 1)
	{
		entext[0] = menuParts->AddTextMesh(Vector3(11.f, 4.f, 208.f), 9.f, TextAlignment::Left, TextAnchor::UpperLeft, 1.26f)->GetComponent<TextMesh>();
		entext[1] = menuParts->AddTextMesh(Vector3(300.f, 4.f, 185.f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.26f)->GetComponent<TextMesh>();
	}
	else
	{
		menuParts->AddTextMesh(Vector3(11.f, 4.f, 208.f), 9.f, TextAlignment::Left, TextAnchor::UpperLeft, 1.26f);
		menuParts->AddTextMesh(Vector3(300.f, 4.f, 185.f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.26f);
	}
	menuPortItem->SetTableForItemParam(2, 0, 0, 1);
	menuPortItem->SetTableForItemParam(2, 1, 0, 2);
	menuPortItem->SetTableForItemParam(2, 2, 0, 3);
	menuPortItem->SetTableForItemParam(2, 3, 0, 4);
	menuPortItem->SetTableForItemParam(2, 4, 0, 5);
	buttons[0] = Sub_BattleInfo(menuPortItem, 0, menuParts->transform());
	buttons[1] = Sub_BattleInfo(menuPortItem, 1, menuParts->transform());
	buttons[1]->localPosition = Vector3(326.5f, 4.f, 108.f);
	menuPortItem->SetTableForCode(2, [=](short code) mutable
	{
		if (code == 0)
		{
			buttons[0]->localPosition = Vector3(225.f, 4.f, 108.f);
			buttons[0]->gameObject->SetActiveRecursively(true);
			buttons[1]->gameObject->SetActiveRecursively(false);
		}
		else
		{
			buttons[0]->localPosition = Vector3(123.5f, 4.f, 108.f);
			buttons[0]->gameObject->SetActiveRecursively(true);
			buttons[1]->gameObject->SetActiveRecursively(true);
		}
	});
	if (GetLangages::m_langagesNumber == 1)
	{
		menuPortItem->SetTableForCode(3, [=](short code) mutable
		{
			if (code == 0)
			{
				entext[0]->characterSize = 9.f * MenuParts::BIAS_FONT_SIZE;
				entext[1]->characterSize = 9.f * MenuParts::BIAS_FONT_SIZE;
			}
			else
			{
				entext[0]->characterSize = 8.099999f;
				entext[1]->characterSize = 8.099999f;
			}
		});
	}
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_BattleAction(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(198, 6, 0.f, 0.f, -2.f);
	MenuParts::APPLY_HERO = 1;
	menuParts->AddTextMesh(Vector3(48.f, 4.f, 236.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuPortItem->SetTableForItemParam(4, 0, menuPortItem->AddParts(menuParts), 1);
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
	Sub_BattleAction(menuPortItem, 0, 10, menuParts->transform(), -102.f, 4.f, 182.f, 136, 20.f);
	Sub_BattleAction(menuPortItem, 1, 12, menuParts->transform(), 102.f, 4.f, 182.f, 136, 20.f);
	Sub_BattleAction(menuPortItem, 2, 9, menuParts->transform(), -102.f, 4.f, 124.f, 136, 20.f);
	Sub_BattleAction(menuPortItem, 3, 8, menuParts->transform(), 102.f, 4.f, 124.f, 136, 20.f);
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_MonsterName(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MonsterNamePlate* list[4] = {};
	MonsterMarkList* mlist = new MonsterMarkList(menuPortItem);
	MonsterMarkList::CalcMarkY();
	for (int i = 0; i < 4; i++)
	{
		list[i] = new MonsterNamePlate(menuPortItem, mlist, i);
	}
	menuPortItem->SetTableForCode(4, [=](short code) mutable
	{
		mlist->Reset();
	});
	menuPortItem->SetTableForCode(5, [=](short code) mutable
	{
		float num = 69.f;
		std::vector<MonsterNamePlate*> array(code, nullptr);
		if (code < 3)
		{
			num += 36.f;
		}
		// landscape (full-surface Status camera): plates are clamped to the world area beside the panel and sit in rows BELOW the monsters' feet / number marks
		// (y 468 px, the middle ones of 3+ groups staggered 36 px lower) instead of the portrait band rows; the original overlap logic is kept
		const bool wide = Layout::Wide();
		const float ppu = 544.f / 600.f, W = (float)Layout::SurfaceW();
		const float limLo = wide ? (8.f - W * 0.5f) / ppu + 80.f : -142.f, limHi = wide ? ((W - (float)Layout::UiW() - 8.f) - W * 0.5f) / ppu - 80.f : 142.f;
		const float rowZ = wide ? (272.f - 468.f) / ppu : 0.f, stagger = wide ? -36.f / ppu : 58.f;
		if (wide) num = rowZ;
		for (int j = 0; j < code; j++)
		{
			MonsterNamePlate* monsterNamePlate = list[j];
			Vector3 center = Vector3(monsterNamePlate->m_center.x, 0.f, num);
			if (center.x < limLo)
			{
				center.x = limLo;
			}
			else if (center.x > limHi)
			{
				center.x = limHi;
			}
			monsterNamePlate->m_name.center = center;
			array[j] = monsterNamePlate;
		}
		if (wide)
		{
			// landscape: ONE row, each plate centred on its group's projected centre; neighbours are pushed apart by the minimum needed (drawn plate width 142 + borders = 160, + 6 units),
			// kept inside the world area; only if they cannot fit on one row the odd ones drop to a second row
			std::vector<MonsterNamePlate*> byX(array.begin(), array.end());
			std::sort(byX.begin(), byX.end(), [](MonsterNamePlate* a, MonsterNamePlate* b) { return a->m_center.x < b->m_center.x; });
			const float sep = 166.f;
			auto relax = [&](std::vector<MonsterNamePlate*>& v) {
				for (int it = 0; it < 60; it++)
				{
					bool moved = false;
					for (size_t k = 0; k + 1 < v.size(); k++)
					{
						float d = v[k + 1]->m_name.center.x - v[k]->m_name.center.x;
						if (d < sep - 0.01f)
						{
							float push = (sep - d) * 0.5f;
							v[k]->m_name.center.x -= push; v[k + 1]->m_name.center.x += push; moved = true;
						}
					}
					for (MonsterNamePlate* p : v) p->m_name.center.x = std::min(limHi, std::max(limLo, p->m_name.center.x));
					if (!moved) break;
				}
			};
			for (MonsterNamePlate* p : byX) p->m_name.center = Vector3(std::min(limHi, std::max(limLo, p->m_center.x)), 0.f, rowZ);
			relax(byX);
			bool fit = true;
			for (size_t k = 0; k + 1 < byX.size(); k++) if (byX[k + 1]->m_name.center.x - byX[k]->m_name.center.x < sep - 0.5f) fit = false;
			if (!fit)
			{
				std::vector<MonsterNamePlate*> rowA, rowB;
				for (size_t k = 0; k < byX.size(); k++) { byX[k]->m_name.center = Vector3(std::min(limHi, std::max(limLo, byX[k]->m_center.x)), 0.f, (k & 1) ? rowZ - 40.f / ppu : rowZ); ((k & 1) ? rowB : rowA).push_back(byX[k]); }
				relax(rowA); relax(rowB);
			}
			for (MonsterNamePlate* p : byX)
				l_info("MonsterName plate %d: group centre %d px -> plate centre %d px (row %s)", p->m_id, (int)(W * 0.5f + p->m_center.x * ppu), (int)(W * 0.5f + p->m_name.center.x * ppu), p->m_name.center.z == rowZ ? "1" : "2");
		}
		else
		{
		if (code > 2)
			{
				std::sort(array.begin(), array.end(), [](MonsterNamePlate* a, MonsterNamePlate* b) { return a->m_name.center.x < b->m_name.center.x; });
				int num2 = 1;
				for (int num3 = code - 1; num2 < num3; num2++)
				{
					Vector3 center2 = array[num2]->m_name.center;
					center2.z += stagger;
					array[num2]->m_name.center = center2;
				}
			}
			if (code > 1)
			{
				for (int num4 = 0; num4 < code; num4++)
				{
					for (int num5 = 0; num5 < code; num5++)
					{
						if (num4 != num5 && array[num4]->m_name.center.z == array[num5]->m_name.center.z && BoundsIntersects(array[num4]->m_name, array[num5]->m_name))
						{
							Vector3 center3 = array[num4]->m_name.center;
							Vector3 center4 = array[num5]->m_name.center;
							num = (center3.x + center4.x) * 0.5f;
							if (center3.x < center4.x)
							{
								center3.x = num - 83.f;
								center4.x = num + 83.f;
								if (center3.x < limLo)
								{
									center3.x = limLo;
									center4.x = center3.x + 166.f;
								}
								else if (center4.x > limHi)
								{
									center4.x = limHi;
									center3.x = center4.x - 166.f;
								}
							}
							else
							{
								center3.x = num + 83.f;
								center4.x = num - 83.f;
								if (center4.x < limLo)
								{
									center4.x = limLo;
									center3.x = center4.x + 166.f;
								}
								else if (center3.x > limHi)
								{
									center3.x = limHi;
									center4.x = center3.x - 166.f;
								}
							}
							array[num4]->m_name.center = center3;
							array[num5]->m_name.center = center4;
						}
					}
				}
			}
	}
		Vector3 vector = Vector3(0.f, 0.f, wide ? 0.f : eIndent::SCREEN_HALF_HEIGHT - 400.f);
		for (MonsterNamePlate* monsterNamePlate2 : array)
		{
			monsterNamePlate2->m_name.center = monsterNamePlate2->m_name.center + vector;
			monsterNamePlate2->m_fname->transform()->setPosition(monsterNamePlate2->m_name.center);
		}
	});
	if (Layout::Wide())      // landscape: plates + number marks are placed and drawn with the full-surface Status camera (UiCameras::PlateCamera)
	{
		UiCameras::Status();
		for (MenuParts* parts : menuPortItem->m_partsList) UiCameras::RetagFrom(parts->transform(), 8, UiCameras::kStatusLayer);
		menuPortItem->m_uiLayer = UiCameras::kStatusLayer;
	}
	return menuPortItem;
}

void MenuPortItemTable::Sub_BattleTarget(MenuPortItem* item, short buttonId, Transform* parent, float x, float y, float z) {
	MenuParts* menuParts = MenuParts::CreateButton(142.f, 20.f, buttonId, 16);
	menuParts->ChangeParts(6, true);
	menuParts->AddIcon(3, -26.f, 0.f, 0.f, 5 + buttonId, false, 0.5f);
	menuParts->Build(4, 7, x, y, z, parent);
	MenuParts::APPLY_HERO = 1;
	menuParts->AddTextMesh(Vector3(-76.f, 4.f, 5.f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft, 0.55f)->GetComponent<TextMesh>()->tabSize = 25.f;
	short partsNo = item->AddParts(menuParts);
	item->SetTableForCode(buttonId, partsNo);
	item->SetTableForItemParam(buttonId, 0, partsNo, 2);
}

MenuPortItem* MenuPortItemTable::Make_BattleTarget(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(199, 6, 0.f, 0.f, -2.f);
	MenuParts::APPLY_HERO = 1;
	menuParts->AddTextMesh(Vector3(48.f, 4.f, 236.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuParts->AddTextMesh(Vector3(138.5f, 4.f, 236.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	int partsNo = menuPortItem->AddParts(menuParts);
	menuPortItem->SetTableForItemParam(4, 0, partsNo, 1);
	menuPortItem->SetTableForItemParam(4, 1, partsNo, 2);
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
	Sub_BattleTarget(menuPortItem, 0, menuParts->transform(), -102.f, 4.f, 182.f);
	Sub_BattleTarget(menuPortItem, 1, menuParts->transform(), 102.f, 4.f, 182.f);
	Sub_BattleTarget(menuPortItem, 2, menuParts->transform(), -102.f, 4.f, 124.f);
	Sub_BattleTarget(menuPortItem, 3, menuParts->transform(), 102.f, 4.f, 124.f);
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_BattleEnemyTarget(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(201, 6, 0.f, 0.f, -2.f);
	MenuParts::APPLY_HERO = 1;
	menuParts->AddTextMesh(Vector3(48.f, 4.f, 236.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuParts->AddTextMesh(Vector3(138.5f, 4.f, 236.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuParts->AddTextMesh(Vector3(191.5f, 4.f, 236.f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft);
	int partsNo = menuPortItem->AddParts(menuParts);
	menuPortItem->SetTableForItemParam(4, 0, partsNo, 1);
	menuPortItem->SetTableForItemParam(4, 1, partsNo, 2);
	menuPortItem->SetTableForItemParam(4, 2, partsNo, 3);
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
	Sub_BattleTarget(menuPortItem, 0, menuParts->transform(), -102.f, 4.f, 182.f);
	Sub_BattleTarget(menuPortItem, 1, menuParts->transform(), 102.f, 4.f, 182.f);
	Sub_BattleTarget(menuPortItem, 2, menuParts->transform(), -102.f, 4.f, 124.f);
	Sub_BattleTarget(menuPortItem, 3, menuParts->transform(), 102.f, 4.f, 124.f);
	return menuPortItem;
}

void MenuPortItemTable::Sub_BattlePartyTarget(MenuPortItem* item, short buttonId, Transform* parent, float x, float y, float z) {
	MenuParts* menuParts = MenuParts::CreateButton(166.f, 20.f, buttonId, 16);
	menuParts->ChangeParts(3);
	menuParts->Build(4, 7, x, y, z, parent);
	MenuParts::APPLY_HERO = 2;
	if (GetLangages::m_langagesNumber == 1)
	{
		menuParts->AddTextMesh(Vector3(0.f, 4.f, -0.5f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter, 1.1f);
	}
	else
	{
		menuParts->AddTextMesh(Vector3(0.f, 4.f, -0.5f), 7.05f, TextAlignment::Center, TextAnchor::MiddleCenter, 1.1f);
	}
	short partsNo = item->AddParts(menuParts);
	item->SetTableForCode(buttonId, partsNo);
	item->SetTableForItemParam(buttonId, 0, partsNo, 1);
	item->SetTableForItemParamTextColorChanger(buttonId, 1, menuParts);
}

MenuPortItem* MenuPortItemTable::Make_BattlePartyTarget(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(201, 6, 0.f, 0.f, -2.f);
	MenuParts::APPLY_HERO = 1;
	menuParts->AddTextMesh(Vector3(48.f, 4.f, 236.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuParts->AddTextMesh(Vector3(138.5f, 4.f, 236.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuParts->AddTextMesh(Vector3(191.5f, 4.f, 236.f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft);
	int partsNo = menuPortItem->AddParts(menuParts);
	menuPortItem->SetTableForItemParam(4, 0, partsNo, 1);
	menuPortItem->SetTableForItemParam(4, 1, partsNo, 2);
	menuPortItem->SetTableForItemParam(4, 2, partsNo, 3);
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
	Sub_BattlePartyTarget(menuPortItem, 0, menuParts->transform(), -107.f, 4.f, 182.f);
	Sub_BattlePartyTarget(menuPortItem, 1, menuParts->transform(), 107.f, 4.f, 182.f);
	Sub_BattlePartyTarget(menuPortItem, 2, menuParts->transform(), -107.f, 4.f, 124.f);
	Sub_BattlePartyTarget(menuPortItem, 3, menuParts->transform(), 107.f, 4.f, 124.f);
	return menuPortItem;
}

void MenuPortItemTable::Sub_BattleItem(MenuPortItem* item, short buttonId, Transform* parent, float x, float y, float z) {
	MenuParts* menuParts = ((GetLangages::m_langagesNumber != 1) ? MenuParts::CreateButton(136.f, 20.f, buttonId, 16) : MenuParts::CreateButton(156.f, 20.f, buttonId, 16));
	menuParts->ChangeParts(0);
	menuParts->AddIcon(3, 8.f, 4.f, 0.f, 0, false, 0.8f);
	menuParts->ChangeParts(6);
	menuParts->AddIcon(6, 0.f, 6.f, 0.f, 0, false, 0.6f);
	menuParts->Build(4, 7, x, y, z, parent);
	menuParts->AddTextMesh(Vector3(-36.f, 4.f, 0.f), 8.5f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.05f);
	short partsNo = item->AddParts(menuParts);
	item->SetTableForCode(buttonId, partsNo);
	item->SetTableForItemParam(buttonId, 0, partsNo, 1);
	item->SetTableForItemParam(buttonId, 1, partsNo, 2);
	item->SetTableForItemParam(buttonId, 2, partsNo, 3);
}

void MenuPortItemTable::Sub_BattleTactics(MenuPortItem* item, short buttonId, Transform* parent, float x, float y, float z) {
	MenuParts* p = MenuParts::CreateButton(136.f, 20.f, buttonId, 16);
	p->AddChangeIconTexture(3, 41.f, 4.f, 15.5f, 26.400002f, false);
	p->ChangeParts(1, false, true);
	p->AddIcon(4, -42.f, 4.f, 4.f, 24, false);
	p->Build(4, 7, x, y, z, parent);
	MenuParts::APPLY_HERO = 1;
	GameObject* text = p->AddTextMesh(Vector3(3.f, 4.f, 9.f), 8.5f, TextAlignment::Left, TextAnchor::MiddleLeft);
	p->AddTextMesh(Vector3(-78.f, 4.f, -13.f), 7.5f, TextAlignment::Left, TextAnchor::MiddleLeft);
	short partsNo = item->AddParts(p);
	item->SetTableForCode(buttonId, [=](short code) mutable
	{
		bool flag = false;
		switch (code)
		{
		case 0:
			p->gameObject->SetActiveRecursively(false);
			return;
		case 1:
			text->transform->localPosition = Vector3(3.f, 4.f, 9.f);
			p->gameObject->SetActiveRecursively(true);
			p->m_list[2]->m_intChanger(0);
			break;
		case 2:
			p->gameObject->SetActiveRecursively(true);
			text->transform->localPosition = Vector3(-6.f, 4.f, 0.f);
			p->m_list[1]->m_intChanger(0);
			p->m_list[2]->m_intChanger(92);
			break;
		case 3:
			p->gameObject->SetActiveRecursively(true);
			text->transform->localPosition = Vector3(-6.f, 4.f, 0.f);
			p->m_list[1]->m_intChanger(0);
			p->m_list[2]->m_intChanger(94);
			break;
		case 4:
			text->transform->localPosition = Vector3(3.f, 4.f, 9.f);
			p->gameObject->SetActiveRecursively(true);
			p->m_list[2]->m_intChanger(0);
			flag = true;
			break;
		}
		p->m_button->m_intChanger(flag ? 4 : 0);
	});
	MenuParts* menuParts = MenuParts::Create(13, true);
	Material* nowMaterial = menuParts->m_maker->GetNowMaterial();
	menuParts->AddIcon(0, 0.f, 4.f, 0.f, 0, true, 0.95f);
	menuParts->AddIcon(0, 0.f, 4.f, -16.f, 0, true, 0.95f);
	menuParts->Build(4, Vector3(-53.f, 4.f, 8.f), p->transform());
	item->AddParts(menuParts);
	item->SetTableForItemParam(buttonId, 4, std::make_shared<StatusIconChanger>(nowMaterial, menuParts->m_list[0], menuParts->m_list[1]));
	item->SetTableForItemParam(buttonId, 0, partsNo, 1);
	item->SetTableForItemParam(buttonId, 1, partsNo, 3);
	item->SetTableForItemParam(buttonId, 2, partsNo, 4);
	item->SetTableForItemParamTextColorChanger(buttonId, 3, p);
}

void MenuPortItemTable::Sub_BattleMagic(MenuPortItem* item, short buttonId, Transform* parent, float x, float y, float z, int w) {
	MenuParts* p = MenuParts::CreateButton(w, 20.f, buttonId, 16);
	p->Build(4, 7, x, y, z, parent);
	Transform* trans = p->AddTextMesh(eIndent::CalcLocalPosition(p->gameObject, 4, 0.f, 4.f, 0.f), 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter)->transform;
	if (GetLangages::m_langagesNumber == 1)
	{
		GameObject* gameObject = p->AddTextMesh(Vector3(12.f, 4.f, -15.f), 6.f, TextAlignment::Left, TextAnchor::MiddleLeft, 1.05f);
		gameObject->GetComponent<TextMesh>()->tabSize = 8.f;
		MenuParts::SetTextColor(gameObject->renderer(), 6);
	}
	else
	{
		MenuParts::SetTextColor(p->AddTextMesh(Vector3(80.f, 4.f, -15.f), 6.f, TextAlignment::Right, TextAnchor::MiddleRight, 1.05f)->renderer(), 6);
	}
	short partsNo = item->AddParts(p);
	item->SetTableForCode(buttonId, [=](short code) mutable
	{
		switch (code)
		{
		case 1:
			trans->localPosition = Vector3(0.f, 4.f, 0.f);
			break;
		case 2:
			trans->localPosition = Vector3(0.f, 4.f, (GetLangages::m_langagesNumber != 1) ? 8.f : 10.f);
			break;
		}
		p->gameObject->SetActiveRecursively(code != 0);
	});
	item->SetTableForItemParam(buttonId, 0, partsNo, 1);
	item->SetTableForItemParam(buttonId, 1, partsNo, 2);
}

MenuPortItem* MenuPortItemTable::Make_BattleIM(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts((type != 2) ? 199 : 198, 6, 0.f, 0.f, -2.f, false, true);
	MenuParts* menuParts2 = MenuParts::CreateBuildTextMenu(7, 0.f, 4.f, 83.f, nullptr, 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	menuParts2->transform()->SetParent(menuParts->transform());
	MenuParts::APPLY_HERO = 1;
	menuParts->AddTextMesh(Vector3(48.f, 4.f, 236.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	MenuParts::APPLY_HERO = 1;
	menuParts->AddTextMesh(Vector3(138.5f, 4.f, 236.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	MenuParts::APPLY_HERO = 1;
	menuParts->AddTextMesh(Vector3(191.5f, 4.f, 236.f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft);
	short partsNo = menuPortItem->AddParts(menuParts);
	menuPortItem->SetTableForItemParam(4, 3, partsNo, 0);
	menuPortItem->SetTableForItemParam(4, 0, partsNo, 1);
	menuPortItem->SetTableForItemParam(4, 1, partsNo, 2);
	menuPortItem->SetTableForItemParam(4, 2, partsNo, 3);
	partsNo = menuPortItem->AddParts(menuParts2);
	menuPortItem->SetTableForItemParam(5, 0, partsNo, 0);
	menuPortItem->SetTableForCode(5, partsNo);
	MenuParts* menuParts3 = MenuParts::CreateButtonIcon(3, 34, 1.2f, -1, 4);
	menuParts3->Build(4, 7, -198.f, 4.f, 87.f, menuParts2->transform());
	menuPortItem->SetTableForCode(1, menuPortItem->AddParts(menuParts3));
	menuParts3 = MenuParts::CreateButtonIcon(3, 35, 1.2f, -1, 8);
	menuParts3->Build(4, 7, 198.f, 4.f, 87.f, menuParts2->transform());
	menuPortItem->SetTableForCode(1, menuPortItem->AddParts(menuParts3));
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
	switch (type)
	{
	case 0:
	{
		int w = ((GetLangages::m_langagesNumber != 1) ? 136 : 156);
		Sub_BattleMagic(menuPortItem, 0, menuParts->transform(), -102.f, 4.f, 182.f, w);
		Sub_BattleMagic(menuPortItem, 1, menuParts->transform(), 102.f, 4.f, 182.f, w);
		Sub_BattleMagic(menuPortItem, 2, menuParts->transform(), -102.f, 4.f, 124.f, w);
		Sub_BattleMagic(menuPortItem, 3, menuParts->transform(), 102.f, 4.f, 124.f, w);
		break;
	}
	case 1:
		Sub_BattleItem(menuPortItem, 0, menuParts->transform(), -102.f, 4.f, 182.f);
		Sub_BattleItem(menuPortItem, 1, menuParts->transform(), 102.f, 4.f, 182.f);
		Sub_BattleItem(menuPortItem, 2, menuParts->transform(), -102.f, 4.f, 124.f);
		Sub_BattleItem(menuPortItem, 3, menuParts->transform(), 102.f, 4.f, 124.f);
		break;
	case 2:
		Sub_BattleTactics(menuPortItem, 0, menuParts->transform(), -102.f, 4.f, 182.f);
		Sub_BattleTactics(menuPortItem, 1, menuParts->transform(), 102.f, 4.f, 182.f);
		Sub_BattleTactics(menuPortItem, 2, menuParts->transform(), -102.f, 4.f, 124.f);
		Sub_BattleTactics(menuPortItem, 3, menuParts->transform(), 102.f, 4.f, 124.f);
		break;
	}
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_BattleChangeButton(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateButton(120.f, 20.f, 0, 16);
	menuParts->Build(4, 7, -126.f, 4.f, 34.f);
	menuParts->AddTextMesh(eIndent::CalcLocalPosition(menuParts->gameObject, 4, 0.f, 4.f, 0.f), 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
	short partsNo = menuPortItem->AddParts(menuParts);
	menuPortItem->SetTableForCode(0, partsNo);
	menuPortItem->SetTableForItemParam(0, 0, partsNo, 1);
	MenuParts* menuParts2 = MenuParts::CreateButton(120.f, 20.f, 1, 16);
	menuParts2->Build(4, 7, 57.f, 4.f, 34.f);
	menuParts2->AddTextMesh(eIndent::CalcLocalPosition(menuParts->gameObject, 4, 0.f, 4.f, 0.f), 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
	partsNo = menuPortItem->AddParts(menuParts2);
	menuPortItem->SetTableForCode(1, partsNo);
	menuPortItem->SetTableForItemParam(1, 0, partsNo, 1);
	menuParts->transform()->SetParent(menuParts2->transform());
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_BattleChangeInfo(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(205, 6, 0.f, 0.f, -2.f, false, true);
	MenuParts::APPLY_HERO = 1;
	menuParts->AddTextMesh(Vector3(48.f, 4.f, 293.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	MenuParts::APPLY_HERO = 1;
	menuParts->AddTextMesh(Vector3(138.5f, 4.f, 293.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	MenuParts::APPLY_HERO = 1;
	menuParts->AddTextMesh(Vector3(191.5f, 4.f, 293.f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft);
	menuParts->AddTextMesh(Vector3(282.f, 4.f, 293.f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft);
	menuParts->m_textColorTargetBits = 0u;
	menuParts->AddTextMesh(Vector3(33.f, 4.f, 272.f), 9.f, TextAlignment::Left, TextAnchor::UpperLeft, 1.125f);
	menuParts->AddTextMesh(Vector3(257.f, 4.f, 272.f), 9.f, TextAlignment::Left, TextAnchor::UpperLeft, 1.125f);
	MenuParts::APPLY_HERO = 1;
	menuParts->AddTextMesh(Vector3(95.f, 4.f, 55.f), 9.f);
	menuParts->AddTextMesh(Vector3(10.f, 4.f, 30.f), 8.f);
	menuPortItem->AddParts(menuParts);
	for (short num = 0; num < 9; num++)
	{
		menuPortItem->SetTableForItemParam(0, num, 0, num);
	}
	MenuParts* menuParts2 = MenuParts::CreateIconTexture(0, 0.f, 0.f, 0.f, 33.6f);
	menuParts2->ChangeParts(13, true);
	Material* nowMaterial = menuParts2->m_maker->GetNowMaterial();
	menuParts2->AddIcon(0, -20.f, 2.f, 0.f, 0, false, 0.95f);
	menuParts2->AddIcon(0, -20.f, 2.f, -18.f, 0, false, 0.95f);
	menuParts2->Build(4, 6, 48.f, 4.f, 44.5f, menuParts->transform());
	menuPortItem->SetTableForItemParam(0, 9, menuPortItem->AddParts(menuParts2), 0);
	menuPortItem->SetTableForItemParamTextColorChanger(0, 10, menuParts);
	menuPortItem->SetTableForItemParam(0, 11, std::make_shared<StatusIconChanger>(nowMaterial, menuParts2->m_list[1], menuParts2->m_list[2]));
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_BattleGo(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(202, 6, 0.f, 0.f, -2.f);
	menuPortItem->AddParts(menuParts);
	if (GetLangages::m_langagesNumber == 1)
	{
		Sub_BattleMagic(menuPortItem, 0, menuParts->transform(), 0.f, 4.f, 144.f, 180);
	}
	else
	{
		Sub_BattleMagic(menuPortItem, 0, menuParts->transform(), 0.f, 4.f, 144.f);
	}
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_BattleHoppingNumber(short port, short type) {
	MenuPortItem* item = new MenuPortItem(port, type);
	item->SetTableForCode(0, [=](short code) mutable
	{
		MenuPort::Param param = item->Receive();
		Vector3 position = NativeManager::GetMeshChara(param[0])->m_chara->transform->position();
		float num = (float)param[1] * 1.85f;
		if (num > 160.f)
		{
			num = 160.f;
		}
		position = MainLoop::m_camera->WorldToScreenPoint(position);
		position = TouchPad::m_camera->ScreenToWorldPoint(position);
		position.z += num;
		HoppingNumber::Create(position, param[2]);
	});
	return item;
}
