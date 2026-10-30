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

constexpr uint32_t kGlobalVaddr = 0x1dcfa8;        // g_Global (.so vaddr); other globals are addressed relative to it
constexpr uint32_t kStageStatusVaddr = 0x1dff70;   // status::StageStatus instance (GOT entry used by DebugSetValue, decoded from the relocation table)
constexpr uint32_t kStageBtlMapName = 0x28;        // StageStatus::setBtlMapName writes strcpy_s(this + 0x28, 0x20, name) and this+0x48 = this+0x28
constexpr uint32_t kStoryStatusVaddr = 0x1e0224;   // status::StoryStatus (chapter at +4, female flag at +8)

void* G() { return (void*)loader_symbol("dq4core", "g_Global"); }
template <class F> F Sym(const char* n) { return reinterpret_cast<F>(loader_symbol("dq4core", n)); }
void CallG(const char* mangled) { auto f = Sym<void (*)(void*)>(mangled); if (f && G()) f(G()); else l_error("WarpTest: %s unavailable", mangled); }
}  // namespace

namespace DevHooks {
void* Obj(uint32_t vaddr) { uintptr_t g = (uintptr_t)G(); return g ? (void*)(g - kGlobalVaddr + vaddr) : nullptr; }

const uint8_t* EventRec(int id) {
  auto f = Sym<const uint8_t* (*)(unsigned)>("_ZN5param5Event11getFileDataEj");
  return f && id >= 0 ? f((unsigned)id) : nullptr;
}
int ChapterEvent(int chapter) {
  static const int ev[] = {0, 1, 24, 59, 93, 109, 310};      // 0 prologue, 1..5 the five chapters, 6 the "after the ending" world
  return chapter >= 0 && chapter <= 6 ? ev[chapter] : -1;
}
bool EventInit(int id, bool female) {
  if (!NativeCall::DebugSetValue) { l_error("DevHooks: DebugSetValue not bound"); return false; }
  const uint8_t* r = EventRec(id);
  if (!r) { l_error("DevHooks: event %d has no row", id); return false; }
  NativeCall::DebugSetValue(1, (unsigned)id, female ? 1u : 0u);
  l_info("DevHooks: event %d loaded (chapter %d, map %s, female %d)", id, r[0x9b] & 7, (const char*)(r + 0x93), female ? 1 : 0);
  return true;
}
bool StartTown(const char* map) {
  auto f = Sym<void (*)(void*, const char*)>("_ZN6Global9startTownEPKc");
  if (!f || !G()) { l_error("DevHooks: Global::startTown unavailable"); return false; }
  f(G(), map); return true;
}
void SetNoDamage(bool on) {
  auto f = Sym<void (*)(bool)>("_ZN6status11PartyStatus17setNoDamageEnableEb");
  if (f) f(on); else l_error("DevHooks: setNoDamageEnable unavailable");
}
void SetNoEncount(bool on) { if (NativeCall::DebugSetValue) NativeCall::DebugSetValue(10, 0, on ? 2u : 0u); }
bool NoEncountActive() {
  auto f = Sym<unsigned (*)(int, void*)>("DebugGetValue");
  return f && (f(1, nullptr) & 0xff) == 0;
}
bool StartBattle(const char* stageMap, const int* mon, const int* count, int groups) {
  if (!NativeCall::DebugSetValue) return false;
  for (unsigned i = 0; i < 4; i++) NativeCall::DebugSetValue(8, i, (int)i < groups ? (((unsigned)mon[i] << 16) | (unsigned)count[i]) : 0u);
  NativeCall::DebugSetValue(8, 4, 0);                      // setBtlMapName("btl_pl_d") + Encount type 0 + Global::startBattle()
  char* st = (char*)Obj(kStageStatusVaddr);
  if (st && stageMap && *stageMap && strncmp(st + kStageBtlMapName, "btl_pl_d", 8) == 0) {
    strncpy(st + kStageBtlMapName, stageMap, 31); st[kStageBtlMapName + 31] = 0;       // same effect as StageStatus::setBtlMapName (pointer at +0x48 already points here)
    l_info("DevHooks: battle stage set to %s", st + kStageBtlMapName);
  } else l_warn("DevHooks: battle stage left as is (%s)", st ? st + kStageBtlMapName : "no StageStatus");
  return true;
}
}  // namespace DevHooks

namespace {
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
    if (cmd == "chapter" || cmd == "event" || cmd == "eventinit") {
      is >> a; b = 0; is >> b;
      int id = cmd == "chapter" ? DevHooks::ChapterEvent((int)a) : (int)a;
      if (id < 0 || !DevHooks::EventInit(id, b != 0)) return false;
      if (cmd != "eventinit") DevHooks::StartTown((const char*)(DevHooks::EventRec(id) + 0x93));
    }
    else if (cmd == "dumpevents") {
      for (int id = 0; id < 347; id++) {
        const uint8_t* r = DevHooks::EventRec(id);
        if (r) l_info("EVENT %d ch %d map %s gold %d lv %d party %d %d %d %s", id, r[0x9b] & 7, (const char*)(r + 0x93), (int)(r[2] | (r[3] << 8)), r[0xf], r[0x10], r[0x11], r[0x12], (const char*)(r + 0x13));
      }
    }
    else if (cmd == "map") { std::string n; is >> n; NativeCall::DebugSetMapName(n.c_str()); }
    else if (cmd == "gold") { is >> a; DSV(3, 0, (unsigned)a); }
    else if (cmd == "bank") { is >> a; DSV(4, 0, (unsigned)a); }
    else if (cmd == "coin") { is >> a; DSV(5, 0, (unsigned)a); }
    else if (cmd == "medal") { is >> a; DSV(7, 0, (unsigned)a); }
    else if (cmd == "pmedal") { is >> a; DSV(6, 0, (unsigned)a); }
    else if (cmd == "item") { is >> a >> b; DSV(2, (unsigned)a, (unsigned)b); }
    else if (cmd == "level") { a = 99; b = 0; is >> a >> b; DSV(11, (unsigned)a, (unsigned)b); }
    else if (cmd == "hpmax") { is >> a >> b; DSV(0x11, (unsigned)a, (unsigned)b); }
    else if (cmd == "mpmax") { is >> a >> b; DSV(0x12, (unsigned)a, (unsigned)b); }
    else if (cmd == "heal") { a = 0; is >> a; DSV(9, 0, (unsigned)a); }
    else if (cmd == "status") { is >> a >> b; DSV(9, (unsigned)a, (unsigned)b); }
    else if (cmd == "timezone") { is >> a; DSV(12, (unsigned)a, 0); }
    else if (cmd == "nodamage") { a = 1; is >> a; DevHooks::SetNoDamage(a != 0); }
    else if (cmd == "noencount") { a = 1; is >> a; DevHooks::SetNoEncount(a != 0); l_info("WarpTest: encounters %s", DevHooks::NoEncountActive() ? "off" : "on"); }
    else if (cmd == "raw") { long k; is >> k >> a >> b; DSV((int)k, (unsigned)a, (unsigned)b); }
    else if (cmd == "town") { std::string n; is >> n; DevHooks::StartTown(n.c_str()); }
    else if (cmd == "fieldscale") { double sc = 1; is >> sc; Field::SetScale((float)sc); }      // host zoom (native does this itself in flight); >1 exercises the grid edges
    else if (cmd == "fieldgetpos" || cmd == "fieldpos") {      // world map player position (Fix32Vector3 = 3 x s32, 16.16 per tile): log it, or set it (tile x, tile y)
      auto gs = Sym<void* (*)()>("_ZN18FieldPlayerManager12getSingletonEv");
      void* fm = gs ? gs() : nullptr;
      if (!fm) { l_error("WarpTest: no FieldPlayerManager"); }
      else if (cmd == "fieldgetpos") {
        auto gp = Sym<void (*)(int*, void*)>("_ZN18FieldPlayerManager11getPositionEv");
        int v[3] = {0, 0, 0}; if (gp) gp(v, fm);
        l_info("WarpTest: field position raw (%d, %d, %d) = tiles (%.2f, %.2f, %.2f)", v[0], v[1], v[2], v[0] / 65536.0, v[1] / 65536.0, v[2] / 65536.0);
      } else {
        double tx = 0, ty = 0; is >> tx >> ty;
        auto sp = Sym<void (*)(void*, int*)>("_ZN18FieldPlayerManager11setPositionERN3dss12Fix32Vector3E");
        int v[3] = {(int)(tx * 65536.0), (int)(ty * 65536.0), 0};
        if (sp) sp(fm, v);
      }
    }
    else if (cmd == "firsttown") CallG("_ZN6Global14startFirstTownEv");
    else if (cmd == "debugtown") CallG("_ZN6Global14startDebugTownEv");
    else if (cmd == "field") CallG("_ZN6Global10startFieldEv");
    else if (cmd == "casino") {
      std::string k; is >> k; int mg = k == "slot" ? 1 : (k.empty() || k == "poker") ? 0 : atoi(k.c_str());
      auto sm = Sym<void (*)(void*, int)>("_ZN6Global11setMinigameEi"); if (sm && G()) sm(G(), mg);
      CallG("_ZN6Global11startCasinoEv");
    }
    else if (cmd == "book") CallG("_ZN6Global9startBookEv");
    else if (cmd == "title") CallG("_ZN6Global10startTitleEv");
    else if (cmd == "staffroll") {       // LunchStaffroll(): StoryStatus::setChapter(9) + DebugSetMapName("mlb1a") (the latter is ignored on the title), so start the town ourselves
      auto sc = Sym<void (*)(void*, int)>("_ZN6status11StoryStatus10setChapterEi");
      void* ss = DevHooks::Obj(kStoryStatusVaddr);
      if (sc && ss) sc(ss, 9); else l_error("WarpTest: StoryStatus::setChapter unavailable");
      DevHooks::StartTown("mlb1a");
    }
    else if (cmd == "battle") {
      std::string map; is >> map;
      std::string g; int mon[4] = {0, 0, 0, 0}, cnt[4] = {0, 0, 0, 0}; int n = 0;
      while (is >> g && n < 4) { int id = 0, c = 1; sscanf(g.c_str(), "%dx%d", &id, &c); mon[n] = id; cnt[n] = c; n++; }
      DevHooks::StartBattle(map.c_str(), mon, cnt, n);
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
  g_lines.push_back("eventinit " + std::to_string(DevHooks::ChapterEvent(Cfg::GetInt("warp_chapter", 1))));
  g_lines.push_back("town " + Cfg::Get("warp"));
  l_info("WarpTest: cfg warp=%s (chapter %d, delay %d)", Cfg::Get("warp").c_str(), Cfg::GetInt("warp_chapter", 1), g_delay);
  Create();
}
void Create() {
  if (!NativeCall::kDevHooksVerified) { l_warn("WarpTest: DISABLED (NativeCall::kDevHooksVerified=false)"); return; }
  GameObject* g = new GameObject("WarpDriver");
  g->AddComponent<WarpDriver>();
  Object::DontDestroyOnLoad(g);
}
}  // namespace WarpTest
