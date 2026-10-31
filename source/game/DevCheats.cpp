// game/DevCheats.cpp -- OWNER: WP3.  See DevCheats.hpp.
#include "game/DevCheats.hpp"

#include <cstdint>
#include <string>

#include "game/MainLoop.hpp"
#include "game/NativeManager.hpp"
#include "game/WarpTest.hpp"
#include "loader/loader.h"
#include "miniunity.hpp"
#include "native/native_bridge.hpp"
#include "platform/cfg.hpp"
#include "utils/logger.h"

using namespace miniunity;

namespace DevCheats {
int HeroPosLog() { static int v = -1; if (v < 0) v = Cfg::GetInt("heropos", 0); return v; }
}

namespace {
class DevDriver : public Component {
public:
  bool applied = false; int sinceScene = 0; std::string lastScene; int tick = 0;
  void Update() override {
    if (!MainLoop::s_ready) return;
    // both flags live in native state that EventInit / Encount::initialize rewrite: re-arm them (cheap calls)
    if (Cfg::GetInt("nodamage", 0) && (tick % 30) == 0) DevHooks::SetNoDamage(true);
    if (Cfg::GetInt("noencount", 0) && (tick % 60) == 0) { DevHooks::SetNoEncount(true); if (tick == 0) l_info("cheat: noencount armed, encounters %s", DevHooks::NoEncountActive() ? "off" : "STILL ON"); }
    tick++;
    if (!applied) {
      const std::string& sc = NativeManager::s_sceneName;
      if (sc != lastScene) { lastScene = sc; sinceScene = 0; }
      bool title = sc == "(title)" || sc == "map/ev/ev01/stage/ev01" || sc.find("ev01") != std::string::npos;
      if (!title && ++sinceScene >= 75) {
        applied = true;
        int gold = Cfg::GetInt("gold", -1), hp = Cfg::GetInt("party_hp", 0), mp = Cfg::GetInt("party_mp", 0), lv = Cfg::GetInt("party_level", 0);
        if (lv > 0) { NativeCall::DebugSetValue(11, (unsigned)lv, 0); l_info("cheat: party_level=%d (all players)", lv); }
        if (gold >= 0) { NativeCall::DebugSetValue(3, 0, (unsigned)gold); l_info("cheat: gold=%d", gold); }
        for (int p = 1; p < 26; p++) {
          if ((0x104ec00u >> p) & 1) continue;                  // player ids the native debug code skips (not party members)
          if (hp > 0) NativeCall::DebugSetValue(0x11, (unsigned)p, (unsigned)hp);
          if (mp > 0) NativeCall::DebugSetValue(0x12, (unsigned)p, (unsigned)mp);
        }
        if (hp > 0 || mp > 0 || lv > 0) {
          NativeCall::DebugSetValue(9, 0, 0); l_info("cheat: party_hp=%d party_mp=%d (all players), healed", hp, mp);
          auto get = reinterpret_cast<unsigned (*)(int, int*)>(loader_symbol("dq4core", "DebugGetValue"));
          for (int p = 1; p < 26 && get; p++) {            // read back (DebugGetValue 0x11: [player] -> hpmax, mpmax, ...): only players whose max HP changed are interesting
            if ((0x104ec00u >> p) & 1) continue;
            alignas(16) int v[8] = {p, 0, 0, 0, 0, 0, 0, 0}; get(0x11, v);
            if (v[1] > 0 && v[1] < 100000) l_info("cheat: player %d hpmax %d mpmax %d", p, v[1], v[2]);
          }
        }
      }
    }
  }
};
}  // namespace

namespace DevCheats {
void Create() {
  if (!NativeCall::kDevHooksVerified) {
    l_warn("DevCheats: DISABLED (NativeCall::kDevHooksVerified=false)");
    return;
  }
  bool any = Cfg::GetInt("nodamage", 0) || Cfg::GetInt("noencount", 0) || Cfg::Has("gold") || Cfg::Has("party_hp") || Cfg::Has("party_mp") || Cfg::Has("party_level");
  if (any) {
    GameObject* g = new GameObject("DevCheats");
    g->AddComponent<DevDriver>();
    Object::DontDestroyOnLoad(g);
    l_info("DevCheats: nodamage=%d noencount=%d gold=%s party_hp=%s party_mp=%s party_level=%s", Cfg::GetInt("nodamage", 0), Cfg::GetInt("noencount", 0), Cfg::Get("gold", "-").c_str(),
           Cfg::Get("party_hp", "-").c_str(), Cfg::Get("party_mp", "-").c_str(), Cfg::Get("party_level", "-").c_str());
  }
  if (Cfg::Has("warp") && !Cfg::Get("warp").empty()) WarpTest::CreateFromCfg();
}
}  // namespace DevCheats
