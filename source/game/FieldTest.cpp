#ifdef DQIV_DEV_TOOLS
#include "game/FieldTest.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "config.h"
#include "game/Field.hpp"
#include "game/Global.hpp"
#include "game/MainLoop.hpp"
#include "miniunity.hpp"
#include "utils/logger.h"

using namespace miniunity;

namespace {
struct Entry { int col0 = 0, row0 = 0; float scale = 1.f; int frames = 120; };
std::vector<Entry> g_entries;

class FieldTestDriver : public Component {
public:
  int index = -1, frameInEntry = 0; bool inited = false; int waitFrames = 0;
  void Update() override {
    Global::Flag |= 0x10000;                                   // FLAG_PAUSE: no NativeManager.Update (the title would fight for the cameras)
    if (!MainLoop::s_ready) return;
    if (!inited) { if (++waitFrames < 5) return; Init(); }
    if (frameInEntry >= g_entries[index < 0 ? 0 : index].frames || index < 0) Next();
    frameInEntry++;
    const Entry& e = g_entries[index];
    // slow pan so the animation of the tiles / the camera is visible in successive shots
    float a = frameInEntry * 0.02f;
    Field::SetPosition(200.f + std::sin(a) * 40.f, 250.f + std::cos(a) * 40.f);
  }
  void Init() {
    inited = true;
    Field::Init();
    for (int i = 0; i < 42; i++) {
      int gx = i % 7, gy = i / 7;
      Field::SetSymbol(i, i, 80.f + gx * 40.f, 120.f + gy * 40.f, 1.f);
      Field::SetSymbolVisible(i, true);
    }
    l_info("FieldTest: Field::Init done, %d entries", (int)g_entries.size());
  }
  void Next() {
    index = (index + 1) % (int)g_entries.size();
    frameInEntry = 0;
    const Entry& e = g_entries[index];
    std::vector<int> tiles(25 * 40);
    for (int r = 0; r < 40; r++) for (int c = 0; c < 25; c++) tiles[r * 25 + c] = ((e.row0 + r) & 63) * 64 + ((e.col0 + c) & 63);
    Field::SetScale(e.scale);
    Field::SetIndexList(0, 0, 25, tiles);
    l_info("FieldTest: block (%d,%d) scale %.2f", e.col0, e.row0, e.scale);
  }
};
}  // namespace

namespace FieldTest {
bool Requested() {
  FILE* f = fopen(DATA_PATH "fieldtest" DQIVN_SLOT_STR ".on", "r");
  if (!f) return false;
  char line[256];
  while (fgets(line, sizeof(line), f)) {
    Entry e; int n = sscanf(line, "%d %d %f %d", &e.col0, &e.row0, &e.scale, &e.frames);
    if (n < 2) continue;
    if (n < 3) e.scale = 1.f;
    if (n < 4) e.frames = 120;
    g_entries.push_back(e);
  }
  fclose(f);
  return !g_entries.empty();
}
void Create() {
  GameObject* g = new GameObject("FieldTestDriver");
  g->AddComponent<FieldTestDriver>();
  Object::DontDestroyOnLoad(g);
  l_info("FieldTest: %d entries", (int)g_entries.size());
}
}  // namespace FieldTest

#endif  // DQIV_DEV_TOOLS
