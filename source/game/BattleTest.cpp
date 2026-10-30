#include "game/BattleTest.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "config.h"
#include "game/MainLoop.hpp"
#include "game/WarpTest.hpp"
#include "miniunity.hpp"
#include "native/native_bridge.hpp"
#include "utils/logger.h"

using namespace miniunity;

namespace {
std::string g_map = "btl_pl_d";          // stage file name (map/btl/<btl_pl>/stage/<btl_pl_d>.fld)
struct Mon { int id, count; };
std::vector<Mon> g_mons; int g_event = 1, g_female = 0, g_delay = 60, g_repeat = 0;

class BattleTestDriver : public Component {
public:
  int waited = 0, sinceStart = 0, phase = 0; bool started = false;
  void Update() override {
    if (!MainLoop::s_ready) return;
    if (waited == 0) l_info("BattleTest: native ready at frame %d (delay %d)", Time::frameCount, g_delay);
    if (!started) { if (++waited < g_delay) return; StartBattle(); return; }
    if (g_repeat > 0 && ++sinceStart >= g_repeat) StartBattle();
  }
  void StartBattle() {
    started = true; sinceStart = 0;
    if (!NativeCall::DebugSetValue) { l_error("BattleTest: debug hooks not exported"); return; }
    if (phase++ == 0) DevHooks::EventInit(g_event, g_female != 0);         // a game state is needed (empty party at the title); the map is not applied: startBattle works from the title backdrop
    int mon[4], cnt[4]; int n = 0;
    for (auto& m : g_mons) if (n < 4) { mon[n] = m.id; cnt[n] = m.count; n++; }
    l_info("BattleTest: starting battle on %s (%d monster groups, event %d)", g_map.c_str(), n, g_event);
    DevHooks::StartBattle(g_map.c_str(), mon, cnt, n);
  }
};
}  // namespace

namespace BattleTest {
bool Requested() {
  FILE* f = fopen(DATA_PATH "battletest" DQIVN_SLOT_STR ".on", "r");
  if (!f) return false;
  char line[300], a[260]; int x, y;
  while (fgets(line, sizeof(line), f)) {
    if (sscanf(line, "map %259s", a) == 1) g_map = a;
    else if (sscanf(line, "mon %d %d", &x, &y) == 2) g_mons.push_back({x, y});
    else if (sscanf(line, "event %d %d", &x, &y) == 2) { g_event = x; g_female = y; }
    else if (sscanf(line, "event %d", &x) == 1) g_event = x;
    else if (sscanf(line, "chapter %d", &x) == 1) g_event = DevHooks::ChapterEvent(x);
    else if (sscanf(line, "delay %d", &x) == 1) g_delay = x;
    else if (sscanf(line, "repeat %d", &x) == 1) g_repeat = x;
  }
  fclose(f);
  if (g_mons.empty()) g_mons.push_back({1, 2});
  return true;
}
void Create() {
  if (!NativeCall::kDevHooksVerified) { l_warn("BattleTest: DISABLED (NativeCall::kDevHooksVerified=false)"); return; }
  GameObject* g = new GameObject("BattleTestDriver");
  g->AddComponent<BattleTestDriver>();
  Object::DontDestroyOnLoad(g);
}
}  // namespace BattleTest
