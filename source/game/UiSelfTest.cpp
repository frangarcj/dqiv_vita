#ifdef DQIV_DEV_TOOLS
// game/UiSelfTest.cpp -- OWNER: WP4a (dev aid). See UiSelfTest.hpp.
#include "game/UiSelfTest.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <psp2/kernel/clib.h>
#include "config.h"
#include "game/MenuPortItemTable.hpp"
#include "game/MenuPortSystem.hpp"
#include "game/TouchPad.hpp"
#include "utils/logger.h"

namespace UiSelfTest {

struct Ev { int frame; int value; bool done; };
static std::vector<Ev> s_magic;
static std::vector<Ev> s_press, s_open, s_tap, s_name, s_cycle, s_tour, s_soak;
static int s_soakIdx = 0, s_soakT = -1; static MenuPortItem* s_soakItem; static short s_soakPort = 30;
static int s_tourIdx = 0, s_tourStart = -1; static MenuPortItem* s_tourItem; static short s_tourPort = 31;
static int s_cycleScreen = -1, s_cycleLeft = 0; static MenuPortItem* s_cycleItem; static short s_cyclePort = 31;
static bool s_loaded;
static short s_nextPort = 16;      // ports 1..15 are native's, 16..31 are ours (max 16 screens)

static void Parse(const char* p, std::vector<Ev>& out) {
  while (*p && *p != '\n' && *p != '\r') {
    int f = (int)strtol(p, (char**)&p, 10);
    if (*p != ':') break;
    int v = (int)strtol(p + 1, (char**)&p, 10);
    out.push_back({ f, v, false });
    while (*p == ',' || *p == ' ') p++;
  }
}

static void Load() {
  s_loaded = true;
  FILE* f = fopen(RUN_CFG_PATH, "r");
  if (!f) return;
  char line[512];
  while (fgets(line, sizeof(line), f)) {
    if (!strncmp(line, "ui_press=", 9)) Parse(line + 9, s_press);
    else if (!strncmp(line, "ui_open=", 8)) Parse(line + 8, s_open);
    else if (!strncmp(line, "ui_tap=", 7)) Parse(line + 7, s_tap);
    else if (!strncmp(line, "ui_magic=", 9)) Parse(line + 9, s_magic);
    else if (!strncmp(line, "ui_soak=", 8)) Parse(line + 8, s_soak);
    else if (!strncmp(line, "ui_tour=", 8)) Parse(line + 8, s_tour);
    else if (!strncmp(line, "ui_cycle=", 9)) Parse(line + 9, s_cycle);
    else if (!strncmp(line, "ui_name=", 8)) Parse(line + 8, s_name);
  }
  fclose(f);
  if (!s_press.empty() || !s_open.empty()) l_info("UiSelfTest: %d presses, %d screens scripted", (int)s_press.size(), (int)s_open.size());
}

void Tick(int frame) {
  if (!s_loaded) Load();
  for (Ev& e : s_press)
    if (!e.done && frame >= e.frame) { e.done = true; TouchPad::m_externalToggle |= e.value; l_info("UiSelfTest: press 0x%x at frame %d", e.value, frame); }
  for (Ev& e : s_tap)
    if (!e.done && frame >= e.frame) {                    // value = port * 1000 + buttonId: what MenuPortItem.Update sends on a button release
      e.done = true;
      MenuPortItem* it = dynamic_cast<MenuPortItem*>(MenuPortSystem::s_ports[e.value / 1000]);
      if (!it) { l_warn("UiSelfTest: tap %d: no item port", e.value); continue; }
      for (MenuParts* b : it->m_buttonList)
        if (b->m_buttonId == e.value % 1000) { it->Send(1, b->m_buttonId, b->m_gamepad); l_info("UiSelfTest: tap port %d button %d (pad %d) at frame %d", e.value / 1000, (int)b->m_buttonId, (int)b->m_gamepad, frame); break; }
    }
  for (Ev& e : s_name)
    if (!e.done && frame >= e.frame) {                    // stands in for InputName (WP4b): the "name confirmed" reply [2, len] + [UTF-8 bytes]
      e.done = true;
      MenuPortItem* it = dynamic_cast<MenuPortItem*>(MenuPortSystem::s_ports[e.value]);
      if (!it) { l_warn("UiSelfTest: name %d: no item port", e.value); continue; }
      static const unsigned char name[] = { 'T', 'e', 's', 't', 'e', 'r' };
      it->Send(2, (short)sizeof(name));
      it->SendBytes('T', 'e', 's', 't', 'e', 'r');
      l_info("UiSelfTest: name entry answer sent on port %d at frame %d", e.value, frame);
    }
  if (!s_soak.empty() && frame >= s_soak[0].frame) {   // soak: endless loop over every screen of the table: build + fill, 3 frames shown, close, next
    static const int ids[] = { 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,75,76,77,78,79,80 };
    const int n = (int)(sizeof(ids) / sizeof(ids[0]));
    if (s_soakT < 0) s_soakT = 0;
    int id = ids[s_soakIdx % n];
    if (s_soakT == 0) {
      s_soakItem = dynamic_cast<MenuPortItem*>(MenuPort::CreateMenuPort(s_soakPort, 1, (short)id));
      if (s_soakItem) { MenuPortSystem::s_ports[s_soakPort] = s_soakItem; s_soakItem->m_bIgnoreDrawRequset = true; s_soakItem->DebugFill(); s_soakItem->SetVisible(true); }
    } else if (s_soakT >= 3) {
      if (s_soakItem) { s_soakItem->Close(); delete s_soakItem; MenuPortSystem::s_ports[s_soakPort] = nullptr; s_soakItem = nullptr; }
      s_soakIdx++; s_soakT = -1;
      if (s_soakIdx % n == 0) l_info("UiSelfTest: soak pass %d complete", s_soakIdx / n);
    }
    if (s_soakT >= 0) s_soakT++;
  }
  if (s_tourIdx < (int)s_tour.size() && frame >= s_tour[s_tourIdx].frame) {     // tour: per screen 14 frames: build+fill, screenshot after 8, close after 12
    if (s_tourStart < 0) s_tourStart = frame;
    int t = frame - s_tourStart, id = s_tour[s_tourIdx].value;
    if (t == 0) {
      s_tourItem = dynamic_cast<MenuPortItem*>(MenuPort::CreateMenuPort(s_tourPort, 1, (short)id));
      if (s_tourItem) { MenuPortSystem::s_ports[s_tourPort] = s_tourItem; s_tourItem->m_bIgnoreDrawRequset = true; s_tourItem->DebugFill(); s_tourItem->SetVisible(true); }
    } else if (t == 8) sceClibPrintf("VITA3K_SCREENSHOT dqivn" DQIVN_SLOT_STR "_ui%03d\n", id);
    else if (t == 12) {
      if (s_tourItem) { s_tourItem->Close(); delete s_tourItem; MenuPortSystem::s_ports[s_tourPort] = nullptr; s_tourItem = nullptr; }
      s_tourIdx++; s_tourStart = -1;
    }
  }
  for (Ev& e : s_cycle)
    if (!e.done && frame >= e.frame) { e.done = true; s_cycleScreen = e.value; s_cycleLeft = 10; MenuPortSystem::LogUiMem("cycle start"); }
  if (s_cycleScreen >= 0) {                              // build on one frame, close on the next (Close() + delete, like MenuPortSystem command 2): counts must stay flat
    if (s_cycleItem) { s_cycleItem->Close(); delete s_cycleItem; MenuPortSystem::s_ports[s_cyclePort] = nullptr; s_cycleItem = nullptr; if (s_cycleLeft == 0) { s_cycleScreen = -1; l_info("UiSelfTest: cycle done"); } }
    else if (s_cycleLeft > 0) {
      s_cycleLeft--;
      s_cycleItem = dynamic_cast<MenuPortItem*>(MenuPort::CreateMenuPort(s_cyclePort, 1, (short)s_cycleScreen));
      if (s_cycleItem) { s_cycleItem->SetVisible(true); MenuPortSystem::s_ports[s_cyclePort] = s_cycleItem; }
      MenuPortSystem::LogUiMem("cycle built");
    }
  }
  for (Ev& e : s_magic)
    if (!e.done && frame >= e.frame) {                    // battle spell list (BattleIM type 0) filled like the native does: name + "MP Cost:<tab>  N" on 4 buttons
      e.done = true;
      MenuPortItem* it = dynamic_cast<MenuPortItem*>(MenuPort::CreateMenuPort(29, 1, 35));
      if (!it) continue;
      MenuPortSystem::s_ports[29] = it; it->m_bIgnoreDrawRequset = true;
      static const char* names[4] = { "Frizz", "Dazzle", "Heal", "Squelch" };
      static const char* costs[4] = { "MP Cost:\t  2", "MP Cost:\t  4", "MP Cost:\t  3", "MP Cost:\t  12" };
      for (short b = 0; b < 4; b++) { it->SetItemCode(b, 1); it->SetItemParam(b, 0, std::string(names[b])); it->SetItemParam(b, 1, std::string(costs[b])); }
      it->SetVisible(true);
    }
  for (Ev& e : s_open)
    if (!e.done && frame >= e.frame) {
      e.done = true;
      if (s_nextPort > 31) { l_warn("UiSelfTest: no spare port for screen %d", e.value); continue; }
      short port = s_nextPort++;
      MenuPort* mp = MenuPort::CreateMenuPort(port, 1, (short)e.value);
      if (MenuPortItem* it = dynamic_cast<MenuPortItem*>(mp)) {
        it->m_bIgnoreDrawRequset = true;
        it->SetVisible(true);
        MenuPortSystem::s_ports[port] = it;
        l_info("UiSelfTest: screen %d built on port %d at frame %d (%d parts)", e.value, (int)port, frame, (int)it->m_partsList.size());
      }
    }
}

}  // namespace UiSelfTest

#endif  // DQIV_DEV_TOOLS
