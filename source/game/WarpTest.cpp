#include "game/WarpTest.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "config.h"
#include "game/Field.hpp"
#include "game/MainLoop.hpp"
#include "loader/loader.h"
#include "miniunity.hpp"
#include "native/native_bridge.hpp"
#include "platform/cfg.hpp"
#include "utils/logger.h"

using namespace miniunity;

namespace {
std::vector<std::string> g_lines;
int g_delay = 450;

void* G() { return (void*)loader_symbol("dq4core", "g_Global"); }
template <class F> F Sym(const char* n) { return reinterpret_cast<F>(loader_symbol("dq4core", n)); }
void CallG(const char* mangled) { auto f = Sym<void (*)(void*)>(mangled); if (f && G()) f(G()); else l_error("WarpTest: %s unavailable", mangled); }

class WarpDriver : public Component {
public:
  size_t next = 0; int waited = 0, wait = 0; bool done = false;
  void Update() override {
    if (!MainLoop::s_ready || done) return;
    if (waited < g_delay) { waited++; return; }
    if (wait > 0) { wait--; return; }
    while (next < g_lines.size()) {
      std::string line = g_lines[next++];
      if (Exec(line)) return;                 // a `wait` was set
    }
    done = true; l_info("WarpTest: script finished");
  }
  bool Exec(const std::string& line) {
    std::istringstream is(line); std::string cmd; is >> cmd;
    if (cmd.empty() || cmd[0] == '#') return false;
    l_info("WarpTest: %s", line.c_str());
    auto DSV = NativeCall::DebugSetValue;
    long a = 0, b = 0;
    if (cmd == "wait") { is >> wait; return true; }
    if (cmd == "chapter") { is >> a; std::string s = "C-" + std::to_string(a); NativeCall::DebugSetMapName(s.c_str()); }
    else if (cmd == "map") { std::string n; is >> n; NativeCall::DebugSetMapName(n.c_str()); }
    else if (cmd == "flag") { a = 0; b = 1; is >> a >> b; DSV(100, (unsigned)a, (unsigned)b); }
    else if (cmd == "gold") { is >> a; DSV(3, 0, (unsigned)a); }
    else if (cmd == "bank") { is >> a; DSV(4, 0, (unsigned)a); }
    else if (cmd == "coin") { is >> a; DSV(5, 0, (unsigned)a); }
    else if (cmd == "medal") { is >> a; DSV(7, 0, (unsigned)a); }
    else if (cmd == "item") { is >> a >> b; DSV(2, (unsigned)a, (unsigned)b); }
    else if (cmd == "monster") { is >> a; DSV(0x20, 0, (unsigned)a); }
    else if (cmd == "party") { DSV(12, 0, 0); while (is >> a) DSV(12, (unsigned)a, 0); DSV(12, 0xFFFFFFFFu, 0); }
    else if (cmd == "raw") { long k; is >> k >> a >> b; DSV((int)k, (unsigned)a, (unsigned)b); }
    else if (cmd == "town") { std::string n; is >> n; auto f = Sym<void (*)(void*, const char*)>("_ZN6Global9startTownEPKc"); if (f && G()) f(G(), n.c_str()); }
    else if (cmd == "townid") { is >> a; auto f = Sym<void (*)(void*, int)>("_ZN6Global9startTownEi"); if (f && G()) f(G(), (int)a); }
    else if (cmd == "fieldscale") { double sc = 1; is >> sc; Field::SetScale((float)sc); }      // host zoom (native does this itself in flight); >1 exercises the grid edges
    else if (cmd == "dumpmaps") {          // "id name" for every map id the game knows (log lines "MAP <id> <name>")
      auto f = Sym<void (*)(void*, char*, int)>("_ZN6Global14getMapNameByIdEPci");
      if (f && G()) for (int id = 0; id < 1100; id++) { char buf[64] = {0}; f(G(), buf, id); if (buf[0]) l_info("MAP %d %s", id, buf); }
    }
    else if (cmd == "fieldgetpos" || cmd == "fieldpos") {      // world map player position (Fix32Vector3 = 3 x s32, 16.16 per tile): log it, or set it (tile x, tile y)
      auto gs = Sym<void* (*)()>("_ZN3fld18FieldPlayerManager12getSingletonEv");
      void* fm = gs ? gs() : nullptr;
      if (!fm) { l_error("WarpTest: no FieldPlayerManager"); }
      else if (cmd == "fieldgetpos") {
        auto gp = Sym<void (*)(int*, void*)>("_ZN3fld18FieldPlayerManager11getPositionEv");
        int v[3] = {0, 0, 0}; if (gp) gp(v, fm);
        l_info("WarpTest: field position raw (%d, %d, %d) = tiles (%.2f, %.2f, %.2f)", v[0], v[1], v[2], v[0] / 65536.0, v[1] / 65536.0, v[2] / 65536.0);
      } else {
        double tx = 0, ty = 0; is >> tx >> ty;
        auto sp = Sym<void (*)(void*, int*)>("_ZN3fld18FieldPlayerManager11setPositionERN2ar12Fix32Vector3E");
        int v[3] = {(int)(tx * 65536.0), (int)(ty * 65536.0), 0};
        if (sp) sp(fm, v);
      }
    }
    else if (cmd == "firsttown") CallG("_ZN6Global14startFirstTownEv");
    else if (cmd == "debugtown") CallG("_ZN6Global14startDebugTownEv");
    else if (cmd == "field") CallG("_ZN6Global10startFieldEv");
    else if (cmd == "casino") CallG("_ZN6Global11startCasinoEv");
    else if (cmd == "slimerace") CallG("_ZN6Global14startSlimeRaceEv");
    else if (cmd == "slimetouch") CallG("_ZN6Global15startSlimeTouchEv");
    else if (cmd == "surechigai") CallG("_ZN6Global15startSurechigaiEv");
    else if (cmd == "book") CallG("_ZN6Global9startBookEv");
    else if (cmd == "opening") CallG("_ZN6Global17startOpeningStaffEv");
    else if (cmd == "staffroll") CallG("_ZN6Global14startStaffrollEv");
    else if (cmd == "fin") CallG("_ZN6Global8startFinEv");
    else if (cmd == "title") CallG("_ZN6Global10startTitleEv");
    else if (cmd == "load") CallG("_ZN6Global8loadGameEv");
    else if (cmd == "battle") {
      std::string map; is >> map; NativeCall::DebugSetMapName(("B-" + map).c_str());
      std::string g; unsigned i = 0;
      while (is >> g && i < 4) { int id = 0, n = 1; sscanf(g.c_str(), "%dx%d", &id, &n); DSV(8, i++, ((unsigned)id << 16) | (unsigned)n); }
      DSV(8, 4, 0);
    }
    else l_warn("WarpTest: unknown command '%s'", cmd.c_str());
    return false;
  }
};
}  // namespace

namespace WarpTest {
bool Requested() {
  FILE* f = fopen(DATA_PATH "warp" DQIVN_SLOT_STR ".on", "r");
  if (!f) return false;
  char line[400];
  while (fgets(line, sizeof(line), f)) {
    std::string s = line; while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    if (s.rfind("delay ", 0) == 0) g_delay = atoi(s.c_str() + 6); else g_lines.push_back(s);
  }
  fclose(f);
  return true;
}
void CreateFromCfg() {
  g_lines.clear();
  g_delay = Cfg::GetInt("warp_delay", 150);
  g_lines.push_back("chapter " + std::to_string(Cfg::GetInt("warp_chapter", 1)));
  g_lines.push_back("town " + Cfg::Get("warp"));
  l_info("WarpTest: cfg warp=%s (chapter %d, delay %d)", Cfg::Get("warp").c_str(), Cfg::GetInt("warp_chapter", 1), g_delay);
  Create();
}
void Create() {
  if (!NativeCall::kDevHooksVerified) { l_warn("WarpTest: DISABLED for DQ4 (DQ5 debug-hook ids / mangled names not re-derived, see docs/TODO_DQ4.md)"); return; }
  GameObject* g = new GameObject("WarpDriver");
  g->AddComponent<WarpDriver>();
  Object::DontDestroyOnLoad(g);
}
}  // namespace WarpTest
