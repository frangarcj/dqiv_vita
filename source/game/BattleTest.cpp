#include "game/BattleTest.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "config.h"
#include "game/MainLoop.hpp"
#include "miniunity.hpp"
#include "native/native_bridge.hpp"
#include "utils/logger.h"

using namespace miniunity;

namespace {
std::string g_map = "btlca1";           // short native name (unity_load_fld builds map/btl/<btlca>/stage/<btlca1>); a longer string smashes its 16-byte stack buffer
struct Mon { int id, count; };
std::vector<Mon> g_mons; std::vector<int> g_party; int g_delay = 60, g_repeat = 0; bool g_forceParty = false;

class BattleTestDriver : public Component {
public:
  int waited = 0, sinceStart = 0; bool started = false;
  void Update() override {
    if (!MainLoop::s_ready) return;
    if (waited == 0) l_info("BattleTest: native ready at frame %d (delay %d)", Time::frameCount, g_delay);
    if (!started) { if (++waited < g_delay) return; StartBattle(); return; }
    if (g_repeat > 0 && ++sinceStart >= g_repeat) StartBattle();
  }
  void StartBattle() {
    started = true; sinceStart = 0;
    if (!NativeCall::DebugSetValue || !NativeCall::DebugSetMapName) { l_error("BattleTest: debug hooks not exported"); return; }
    std::string nm = "B-" + g_map;
    NativeCall::DebugSetMapName(nm.c_str());
    l_info("BattleTest: map name set");
    if (!g_party.empty() && !g_forceParty) {       // measured: ANY DebugSetValue(12, id) from the title state (no game initialised) makes libdq4core fault (DABT FAR=0 in PartyStatus::add); use the default party or `warp chapter N`
      l_warn("BattleTest: `party` ignored (%d ids): the party debug hook crashes before a game is loaded; add `forceparty` to the file to try anyway", (int)g_party.size());
      g_party.clear();
    }
    if (!g_party.empty()) {
      NativeCall::DebugSetValue(12, 0, 0);
      for (int id : g_party) NativeCall::DebugSetValue(12, (unsigned)id, 0);
      NativeCall::DebugSetValue(12, 0xFFFFFFFFu, 0);
    }
    l_info("BattleTest: party set");
    for (unsigned i = 0; i < 4; i++) NativeCall::DebugSetValue(8, i, i < g_mons.size() ? ((unsigned)g_mons[i].id << 16) | (unsigned)g_mons[i].count : 0);
    l_info("BattleTest: monsters set");
    l_info("BattleTest: starting battle on %s (%d monster groups, party %d)", g_map.c_str(), (int)g_mons.size(), (int)g_party.size());
    NativeCall::DebugSetValue(8, 4, 0);
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
    else if (sscanf(line, "party %d", &x) == 1) g_party.push_back(x);
    else if (sscanf(line, "delay %d", &x) == 1) g_delay = x;
    else if (!strncmp(line, "forceparty", 10)) g_forceParty = true;
    else if (sscanf(line, "repeat %d", &x) == 1) g_repeat = x;
  }
  fclose(f);
  if (g_mons.empty()) g_mons.push_back({1, 2});
  return true;
}
void Create() {
  if (!NativeCall::kDevHooksVerified) { l_warn("BattleTest: DISABLED for DQ4 (DebugSetValue ids not re-derived, see docs/TODO_DQ4.md)"); return; }
  GameObject* g = new GameObject("BattleTestDriver");
  g->AddComponent<BattleTestDriver>();
  Object::DontDestroyOnLoad(g);
}
}  // namespace BattleTest
