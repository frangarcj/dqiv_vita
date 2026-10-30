// game/DevCheats.cpp -- OWNER: WP3.  See DevCheats.hpp.
#include "game/DevCheats.hpp"

#include <kubridge.h>
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
bool HeroPosLog() { static int v = -1; if (v < 0) v = Cfg::GetInt("heropos", 0); return v != 0; }
}

namespace {
bool PatchNoEncounter() {
  uintptr_t brew = loader_symbol("dq4core", "_ZN3btl7Encount4brewEv");
  if (!brew) { l_error("noencount: btl::Encount::brew not found"); return false; }
  if (brew & 1) { l_error("noencount: brew is Thumb code (%p), not patching", (void*)brew); return false; }
  const uint32_t code[2] = { 0xe3a00000u /* mov r0, #0 */, 0xe12fff1eu /* bx lr */ };
  l_info("noencount: patching btl::Encount::brew at %p (first word was %08x)", (void*)brew, *(volatile uint32_t*)brew);
  kuKernelCpuUnrestrictedMemcpy((void*)brew, code, sizeof(code));
  kuKernelFlushCaches((void*)brew, sizeof(code));
  return true;
}

class DevDriver : public Component {
public:
  bool patched = false, applied = false; int sinceScene = 0; std::string lastScene; int tick = 0;
  void Update() override {
    if (!MainLoop::s_ready) return;
    if (Cfg::GetInt("noencount", 0) && !patched) { patched = true; PatchNoEncounter(); }
    if (Cfg::GetInt("nodamage", 0) && (tick++ % 30) == 0) {
      auto f = reinterpret_cast<void (*)(bool)>(loader_symbol("dq4core", "_ZN6status18PartyStatusUtility17setNoDamageEnableEb"));
      if (f) f(true);
    }
    if (!applied) {
      const std::string& sc = NativeManager::s_sceneName;
      if (sc != lastScene) { lastScene = sc; sinceScene = 0; }
      bool title = sc == "(title)" || sc.find("ev17/out") != std::string::npos;
      if (!title && ++sinceScene >= 75) {
        applied = true;
        int gold = Cfg::GetInt("gold", -1), hp = Cfg::GetInt("party_hp", 0), mp = Cfg::GetInt("party_mp", 0);
        if (gold >= 0) { NativeCall::DebugSetValue(3, (unsigned)gold, 0); l_info("cheat: gold=%d", gold); }
        for (int p = 0; p < 8; p++) {
          if (hp > 0) NativeCall::DebugSetValue(0x11, (unsigned)hp, (unsigned)p);
          if (mp > 0) NativeCall::DebugSetValue(0x12, (unsigned)mp, (unsigned)p);
        }
        if (hp > 0 || mp > 0) l_info("cheat: party_hp=%d party_mp=%d (all 8 slots)", hp, mp);
      }
    }
  }
};
}  // namespace

namespace DevCheats {
void Create() {
  bool any = Cfg::GetInt("nodamage", 0) || Cfg::GetInt("noencount", 0) || Cfg::Has("gold") || Cfg::Has("party_hp") || Cfg::Has("party_mp");
  if (any) {
    GameObject* g = new GameObject("DevCheats");
    g->AddComponent<DevDriver>();
    Object::DontDestroyOnLoad(g);
    l_info("DevCheats: nodamage=%d noencount=%d gold=%s party_hp=%s party_mp=%s", Cfg::GetInt("nodamage", 0), Cfg::GetInt("noencount", 0), Cfg::Get("gold", "-").c_str(),
           Cfg::Get("party_hp", "-").c_str(), Cfg::Get("party_mp", "-").c_str());
  }
  if (Cfg::Has("warp") && !Cfg::Get("warp").empty()) WarpTest::CreateFromCfg();
}
}  // namespace DevCheats
