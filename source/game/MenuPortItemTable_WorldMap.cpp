// game/MenuPortItemTable_WorldMap.cpp -- OWNER: WP4b.  Make_WorldLargeMap + LoadLargeWorldMap + MAP_DIFF (C# MenuPortItemTable.cs 276-310, 2560-2626).
#include "game/MenuPortItemTable.hpp"
#include "utils/logger.h"

namespace {
struct MAP_DIFF {
  const char* m_fname; int m_bits, m_x, m_y, m_width;
  void CheckSet(std::vector<Color32>& dest, short flag) const {
    if ((flag & (short)(1 << m_bits)) == 0) return;
    TextAsset* textAsset = Resources::Load<TextAsset>(std::string("Menu/map/") + m_fname);
    if (!textAsset) { l_error("MAP_DIFF: Menu/map/%s missing", m_fname); return; }
    const std::vector<uint8_t>& bytes = textAsset->bytes;
    int num = 1024 * m_y + m_x;
    size_t num2 = 0;
    size_t num3 = bytes.size();
    while (num2 + 3 <= num3) {
      int num4 = 0;
      int num5 = num;
      while (num4 < m_width && num2 + 3 <= num3) {
        dest[num5].r = bytes[num2++];
        dest[num5].g = bytes[num2++];
        dest[num5].b = bytes[num2++];
        num4++;
        num5++;
      }
      num += 1024;
    }
    Resources::UnloadAsset(textAsset);
  }
};
const MAP_DIFF c_mapDiff[8] = { { "P0_430_386_20", 0, 430, 386, 20 }, { "P0_437_303_23", 0, 437, 303, 23 }, { "P1_762_177_33", 1, 762, 177, 33 }, { "P2_896_547_49", 2, 896, 547, 49 },
                                { "P3_226_266_50", 3, 226, 266, 50 }, { "P4_226_266_50", 4, 226, 266, 50 }, { "P5_226_266_50", 5, 226, 266, 50 }, { "P6_226_266_50", 6, 226, 266, 50 } };

std::vector<Color32> LoadLargeWorldMap(short flag) {
  std::vector<Color32> array(1048576, Color32(0, 0, 0, 0));
  TextAsset* textAsset = Resources::Load<TextAsset>("Menu/map/dsdq4lmap0_1024");
  if (textAsset) {
    const std::vector<uint8_t>& bytes = textAsset->bytes;
    size_t num = 0;
    for (int i = 0; i < 1048576 && num + 3 <= bytes.size(); i++) {
      array[i].r = bytes[num++];
      array[i].g = bytes[num++];
      array[i].b = bytes[num++];
      array[i].a = 255;
    }
    Resources::UnloadAsset(textAsset);
  } else l_error("LoadLargeWorldMap: Menu/map/dsdq4lmap0_1024 missing");
  for (const MAP_DIFF& d : c_mapDiff) d.CheckSet(array, flag);
  return array;
}
}  // namespace

MenuPortItem* MenuPortItemTable::Make_WorldLargeMap(short port, short type) {
  struct State { Vector2 pos; MenuParts* map = nullptr; };
  auto st = std::make_shared<State>();
  MenuPortItem* item = new MenuPortItem(port, type);
  MenuParts* menuParts = MenuParts::CreateWindow0(430.f, 319.5f);
  MenuParts* menuParts2 = MenuParts::Create(3);
  item->SetTableForCode(0, [st, item](short flag) {
    Texture2D* texture2D = new Texture2D(1024, 1024, TextureFormat::RGB24, false);
    std::vector<Color32> pixels = LoadLargeWorldMap(flag);
    texture2D->SetPixels32(pixels.data(), (int)pixels.size());
    texture2D->Apply();
    st->map = MenuParts::CreateFromTextureWindow(texture2D, 256, 192);
    st->map->Build(0, 0, 2.f, 12.f, -2.f);
    st->map->transform()->localScale = Vector3(0.99f, 0.99f, 0.99f);
    st->map->renderer()->material()->SetTextureScale("_MainTex", Vector2(0.25f, 0.1875f));
    item->AddParts(st->map);
    st->map->m_uol.Add(texture2D);
  });
  item->SetTableForCode(1, [st](short posx) {
    st->pos.x = (float)posx / 1024.f - 0.125f;
    if (st->map) st->map->renderer()->material()->SetTextureOffset("_MainTex", st->pos);
  });
  item->SetTableForCode(2, [st](short posy) {
    st->pos.y = (float)(posy - 1024) / 1024.f - 3.f / 32.f;
    if (st->map) st->map->renderer()->material()->SetTextureOffset("_MainTex", st->pos);
  });
  menuParts->Build(0, 0, -2.f, 0.f, 2.f);
  item->AddParts(menuParts);
  menuParts2->AddIcon(Vector3::zero, 31, true);
  menuParts2->Build(6, 1, 0.f, 16.f, -168.75f);
  menuParts2->transform()->SetParent(menuParts->transform());
  item->AddParts(menuParts2);
  return item;
}
