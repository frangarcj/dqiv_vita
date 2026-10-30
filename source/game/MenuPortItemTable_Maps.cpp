// game/MenuPortItemTable_Maps.cpp -- OWNER: WP4b.  C# source: re/cs_out/Assembly-CSharp/MenuPortItemTable.cs (first pass by tools/menu_cs2cpp.py, then fixed by hand)
#include "game/Layout.hpp"
#include "game/MenuPortItemTable.hpp"
#include "game/MeshChara.hpp"
#include "game/MainLoop.hpp"
#include "game/Boot.hpp"
#include "game/BlinkMapSymbol.hpp"
#include "game/BackColor.hpp"
#include "game/NativeManager.hpp"
#include "game/TouchPad.hpp"
#include "utils/logger.h"
#include <cstdio>

MenuPortItem* MenuPortItemTable::Make_ShopMap(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateWindow0(430.f, 319.5f);
	menuParts->Build(0, 0, -2.f, 0.f, 2.f);
	menuPortItem->AddParts(menuParts);
	MenuParts* menuParts2 = MenuParts::CreateFromTexture("Menu/map/shop", true, 0.75f);
	menuParts2->Build(0, 0, 2.f, 2.f, -2.f);
	menuParts2->transform()->localScale = Vector3(0.99f, 0.99f, 0.99f);
	menuParts2->transform()->SetParent(menuParts->transform());
	menuPortItem->AddParts(menuParts2);
	menuParts2 = MenuParts::CreateBuildFrameParts(190, 1, 0.f, 4.f, 0.f);
	menuParts2->transform()->SetParent(menuParts->transform());
	menuParts2->AddTextMesh(1, 0.f, 8.f, -48.f, 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter)->transform->SetParent(menuParts2->transform());
	menuParts2->AddTextMesh(0, 52.f, 8.f, -80.f, 9.f, TextAlignment::Left, TextAnchor::UpperLeft, 1.8f)->transform->SetParent(menuParts2->transform());
	menuParts2->AddTextMesh(0, 379.f, 8.f, -80.f, 9.f, TextAlignment::Right, TextAnchor::UpperRight, 1.8f)->transform->SetParent(menuParts2->transform());
	GameObject* page = menuParts2->AddTextMesh(1, 0.f, 9.f, -315.5f, 9.5f, TextAlignment::Center, TextAnchor::MiddleCenter);
	short partsNo = menuPortItem->AddParts(menuParts2);
	menuPortItem->SetTableForCode(0, partsNo);
	menuPortItem->SetTableForCode(1, [=](short code) mutable
	{
		page->SetActiveRecursively(code != 0);
	});
	menuPortItem->SetTableForItemParam(0, 0, partsNo, 1);
	menuPortItem->SetTableForItemParam(0, 1, partsNo, 2);
	menuPortItem->SetTableForItemParam(0, 2, partsNo, 3);
	menuPortItem->SetTableForItemParam(0, 3, partsNo, 4);
	menuParts2 = MenuParts::CreateButtonIcon(10, 0, 1.f, -1, 8);
	menuParts2->Build(4, 1, -190.f, 9.f, -311.5f, page->transform);
	menuPortItem->AddParts(menuParts2);
	menuParts2 = MenuParts::CreateButtonIcon(10, 1, 1.f, -1, 4);
	menuParts2->Build(4, 1, 190.f, 9.f, -311.5f, page->transform);
	menuPortItem->AddParts(menuParts2);
	return menuPortItem;
}

MenuPortItem* MenuPortItemTable::Make_CityMap(short port, short type) {
	MenuPortItem* item = new MenuPortItem(port, type);
	MenuParts* root = MenuParts::CreateWindow0(430.f, 319.5f);
	MenuParts* posf = MenuParts::Create(3);
	root->SetButton(-1, 2);
	root->m_isHit = [=](Ray& ray) mutable
	{
		bool result = false;
		if (root->m_button->m_bEnable)
		{
			result = true;
			root->m_gamepad = (short)((!root->defaultIsHit(ray)) ? 1 : 2);
		}
		return result;
	};
	root->Build(0, 0, -2.f, 0.f, 2.f);
	item->AddParts(root);
	posf->AddIcon(Vector3::zero, 31, true, 0.5f);
	posf->Build(6, 0, 0.f, 12.f, 0.f);
	item->AddParts(posf);
	item->SetTableForItemParam(0, 0, std::make_shared<ICityMap>(item, posf));
	item->SetTableForCode(1, [=](short posx) mutable
	{
		Vector3 localPosition = posf->transform()->localPosition;
		localPosition.x = (float)posx * 450.f / 256.f;
		localPosition.y = 4.f;
		posf->transform()->localPosition = localPosition;
	});
	item->SetTableForCode(2, [=](short posy) mutable
	{
		Vector3 localPosition = posf->transform()->localPosition;
		localPosition.y = 4.f;
		localPosition.z = (float)(-posy) * 450.f * 0.75f / 192.f;
		posf->transform()->localPosition = localPosition;
	});
	item->SetTableForCode(3, [=](short nouse) mutable
	{
		GameObject* gameObject = new GameObject();
		gameObject->transform->SetParent(root->transform());
		gameObject->transform->localPosition = Vector3(0.f, 14.f, 0.f);
		gameObject->AddComponent<BlinkMapSymbol>()->SetParameter(item->Receive());
	});
	item->SetTableForCode(4, [=](short posxy) mutable
	{
		float x = (float)(posxy & 0xFF) * 1.7578125f;
		float z = (0.f - (float)((posxy >> 8) & 0xFF)) * 1.7578125f;
		MenuParts* menuParts = MenuParts::Create(5);
		menuParts->AddIcon(Vector3::zero, 3, true, 1.2f);
		menuParts->Build(4, Vector3(x, 13.f, z), root->transform());
	});
	item->SetTableForCode(5, [=](short dummy) mutable
	{
		MenuPort::Param param = item->Receive();
		MenuParts* menuParts = MenuParts::CreateBuildDummy(0, 2.f, 12.5f, -2.f);
		MeshFilter* meshFilter = menuParts->gameObject->AddComponent<MeshFilter>();
		Mesh* mesh = OwnMesh(meshFilter);
		MeshRenderer* meshRenderer = menuParts->gameObject->AddComponent<MeshRenderer>();
		Texture2D* texture2D = new Texture2D(256, 192, TextureFormat::ARGB32, false);
		Material* material = new Material(MenuParts::l_partsShader);
		texture2D->filterMode = FilterMode::Point;
		TextAsset* textAsset = Resources::Load<TextAsset>("Menu/map/dsdq4wmap01m");
		static const std::vector<uint8_t> noBytes;
		const std::vector<uint8_t>& bytes = textAsset ? textAsset->bytes : noBytes;
		std::vector<Color32> array(49152, Color32(0, 0, 0, 0));
		int num = 0;
		int num2 = 16;
		while (num < 16)
		{
			uint16_t num3 = (uint16_t)param[num];
			int num4 = 0;
			int num5 = 48;
			while (num4 < 16)
			{
				if ((num3 & (1 << num4)) != 0)
				{
					for (int i = 0; i < 10; i++)
					{
						int num6 = num5 + (num2 + i << 8);
						int num7 = 0;
						int num8 = num6 << 2;
						while (num7 < 10 && (size_t)num8 + 3 < bytes.size() && num6 < 49152)
						{
							array[num6].r = bytes[num8++];
							array[num6].g = bytes[num8++];
							array[num6].b = bytes[num8++];
							array[num6].a = bytes[num8++];
							num7++;
							num6++;
						}
					}
				}
				num4++;
				num5 += 10;
			}
			num++;
			num2 += 10;
		}
		texture2D->SetPixels32(array.data(), (int)array.size());
		texture2D->Apply();
		if (textAsset) Resources::UnloadAsset(textAsset);
		menuParts->gameObject->layer = 8;
		material->mainTexture = texture2D;
		meshRenderer->setMaterial(material);
		mesh->vertices = { Vector3(0.f, 0.f, 0.f), Vector3(450.f, 0.f, 0.f), Vector3(450.f, 0.f, -337.5f), Vector3(0.f, 0.f, -337.5f) };
		mesh->uv = { Vector2(0.f, 0.f), Vector2(1.f, 0.f), Vector2(1.f, 1.f), Vector2(0.f, 1.f) };
		mesh->SetSubMeshCount(1);
		mesh->SetTriangles(std::vector<int>{ 0, 1, 2, 2, 3, 0 }, 0);
		mesh->RecalculateBounds();
		menuParts->transform()->SetParent(root->transform());
		menuParts->transform()->localScale = Vector3(0.99f, 0.99f, 0.99f);
		menuParts->m_uol.Adds(meshFilter, mesh, meshRenderer, texture2D, material);
		item->AddParts(menuParts);
	});
	return item;
}

GameObject* MenuPortItemTable::Sub_MonsterEncyclopediaButton(MenuPortItem* item, Transform* parent, short bid, float x, float y) {
	MenuParts* menuParts;
	if (GetLangages::m_langagesNumber == 1)
	{
		menuParts = MenuParts::CreateButton(168.f, 14.f, bid, 16);
		menuParts->Build(4, 4, x, 4.f, y, parent);
		menuParts->AddTextMesh(Vector3(-93.5f, 4.f, 0.5f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft);
	}
	else
	{
		menuParts = MenuParts::CreateButton(160.f, 14.f, bid, 16);
		menuParts->Build(4, 4, x, 4.f, y, parent);
		menuParts->AddTextMesh(Vector3(-85.5f, 4.f, 0.5f), 9.f, TextAlignment::Left, TextAnchor::MiddleLeft);
	}
	short partsNo = item->AddParts(menuParts);
	item->SetTableForCode(bid, partsNo);
	item->SetTableForItemParam(bid, 0, partsNo, 0);
	item->SetTableForItemParam(bid, 1, partsNo, 1);
	return menuParts->gameObject;
}

MenuPortItem* MenuPortItemTable::Make_MonsterEncyclopedia(short port, short type) {
	MenuPortItem* menuPortItem = new MenuPortItem(port, type);
	MenuParts* menuParts = MenuParts::CreateBuildFrameParts(265, 6, 0.f, 0.f, -2.f);
	MenuParts* menuParts2 = MenuParts::CreateButton(120.f, 14.f, 12, 2048);
	GameObject* blist[15] = {};
	menuParts2->m_externalToggle = true;
	short partsNo = menuPortItem->AddParts(menuParts);
	MenuParts::SetTextColor((blist[12] = menuParts->AddTextMesh(4, -192.f, 8.f, 151.5f, 9.f, TextAlignment::Left, TextAnchor::UpperLeft, 1.78f))->renderer(), 4);
	MenuParts::SetTextColor((blist[13] = menuParts->AddTextMesh(4, 8.5f, 8.f, 151.5f, 9.f, TextAlignment::Left, TextAnchor::UpperLeft, 1.78f))->renderer(), 4);
	MenuParts::SetTextColor((blist[14] = menuParts->AddTextMesh(4, 198.f, 8.f, 151.5f, 9.f, TextAlignment::Right, TextAnchor::UpperRight, 1.78f))->renderer(), 4);
	menuPortItem->SetTableForItemParam(12, 2, partsNo, 1);
	menuPortItem->SetTableForItemParam(12, 3, partsNo, 2);
	menuPortItem->SetTableForItemParam(12, 4, partsNo, 3);
	menuParts2->Build(4, 6, 189.f, 4.f, 32.5f, menuParts->transform());
	menuParts2->AddTextMesh(Vector3(0.f, 4.f, 0.f), 9.f, TextAlignment::Center, TextAnchor::MiddleCenter);
	partsNo = menuPortItem->AddParts(menuParts2);
	menuPortItem->SetTableForCode(12, partsNo);
	menuPortItem->SetTableForItemParam(12, 0, partsNo, 0);
	menuPortItem->SetTableForItemParam(12, 1, partsNo, 1);
	MenuParts* menuParts3 = MenuParts::CreateButtonIcon(10, 0, 0.9f, 0, 8);
	menuParts3->Build(4, Vector3(-152.f, 4.f, 0.f), menuParts2->transform());
	menuPortItem->AddParts(menuParts3);
	menuParts3 = MenuParts::CreateButtonIcon(10, 1, 0.9f, 0, 4);
	menuParts3->Build(4, Vector3(152.f, 4.f, 0.f), menuParts2->transform());
	menuPortItem->AddParts(menuParts3);
	short num = 0;
	short num2 = 0;
	while (num < 6)
	{
		float y = 140.f - 56.f * (float)num;
		blist[num2] = Sub_MonsterEncyclopediaButton(menuPortItem, menuParts->transform(), num2, -103.5f, y);
		num2++;
		blist[num2] = Sub_MonsterEncyclopediaButton(menuPortItem, menuParts->transform(), num2, 103.5f, y);
		num2++;
		num++;
	}
	Sub_BackButton(menuPortItem)->transform()->SetParent(menuParts->transform());
	float num3 = 0.89f * Layout::Aspect(Camera::main()->aspect());
	GameObject* gameObject = new GameObject();
	MeshFilter* meshFilter = gameObject->AddComponent<MeshFilter>();
	Mesh* mesh = OwnMesh(meshFilter);
	MeshRenderer* meshRenderer = gameObject->AddComponent<MeshRenderer>();
	Texture2D* mainTexture = Resources::Load<Texture2D>("Menu/map/encyclopedia");
	Material* material = new Material(Shader::Find("Custom/BackOrtho"));
	material->mainTexture = mainTexture;
	meshRenderer->setMaterial(material);
	mesh->vertices = { Vector3(-1.f, 0.f - num3, 0.f), Vector3(1.f, 0.f - num3, 0.f), Vector3(1.f, num3, 0.f), Vector3(-1.f, num3, 0.f) };
	mesh->uv = { Vector2(0.f, 1.f), Vector2(1.f, 1.f), Vector2(1.f, 0.f), Vector2(0.f, 0.f) };
	mesh->SetSubMeshCount(1);
	mesh->SetTriangles(std::vector<int>{ 0, 1, 2, 2, 3, 0 }, 0);
	mesh->RecalculateBounds();
	menuParts3 = MenuParts::AttachMenuParts(gameObject, 0, 0.f, 0.f, 0.f);
	menuParts3->m_uol.Adds(material, meshRenderer, mesh, meshFilter);
	menuParts3->transform()->SetParent(Camera::main()->transform());
	menuParts3->transform()->localPosition = Vector3::zero;
	menuParts3->transform()->localRotation = Quaternion::identity;
	menuParts3->transform()->Translate(0.f, 0.f, 20.f);
	menuPortItem->AddParts(menuParts3);
	BackColor::SetColor(Color::black);
	menuPortItem->SetTableForCode(13, [=](short code) mutable
	{
		bool flag = code != 0;
		int i;
		for (i = 0; i < 12; i++)
		{
			blist[i]->SetActiveRecursively(flag);
		}
		flag = !flag;
		for (; i < 15; i++)
		{
			blist[i]->SetActiveRecursively(flag);
		}
	});
	return menuPortItem;
}
