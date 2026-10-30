// game/Layout.cpp -- see Layout.hpp.
#include "game/Layout.hpp"
#include "utils/logger.h"

#include <cmath>
#include <set>
#include <algorithm>
#include "platform/cfg.hpp"

namespace Layout {

static Mode g_mode;
static bool g_init;
static void Init() {
  if (g_init) return;
  g_init = true;
#ifdef DQIVN_NO_LANDSCAPE
  g_mode = Mode::Portrait;
#else
  std::string v = Cfg::Get("layout", "landscape");
  if (v == "portrait" || v == "tablet" || v == "phone") g_mode = Mode::Portrait;
  else if (v == "split") g_mode = Mode::Split;
  else g_mode = Mode::Landscape;
#endif
}
Mode GetMode() { Init(); return g_mode; }
bool Naive() { static int v = Cfg::GetInt("land_naive", 0); return v != 0 && IsLandscape(); }
bool PanelLeft() { static int v = Cfg::Get("ui_side", "right") == "left"; return v != 0; }
int SurfaceW() { return IsLandscape() ? DQIV_DISPLAY_W : DQIV_LOGICAL_W; }
int SurfaceH() { return IsLandscape() ? DQIV_DISPLAY_H : DQIV_LOGICAL_H; }
int ViewX0() { return (DQIV_DISPLAY_W - SurfaceW()) / 2; }
int ViewY0() { return (DQIV_DISPLAY_H - SurfaceH()) / 2; }
int UiW() { return (IsLandscape() && !Naive()) ? (int)kPanelW : SurfaceW(); }
float UiPixelX0() { return (IsLandscape() && !Naive() && !PanelLeft()) ? (float)(SurfaceW() - (int)kPanelW) : 0.f; }

miniunity::Rect UiRect() {
  if (!IsLandscape() || Naive()) return miniunity::Rect(0.f, 0.f, 1.f, 1.f);
  float w = kPanelW / SurfaceW();
  return miniunity::Rect(PanelLeft() ? 0.f : 1.f - w, 0.f, w, 1.f);
}
miniunity::Rect WorldRect() {
  if (GetMode() != Mode::Split || Naive()) return miniunity::Rect(0.f, 0.f, 1.f, 1.f);
  float w = kPanelW / SurfaceW();
  return miniunity::Rect(PanelLeft() ? w : 0.f, 0.f, 1.f - w, 1.f);
}

BattleLayout GetBattleLayout() {
  static BattleLayout v = [] {
    std::string s = Cfg::Get("battle_layout", "large");
    return s == "panorama" ? BattleLayout::Panorama : s == "panel" ? BattleLayout::Panel : s == "band" ? BattleLayout::Band : BattleLayout::Large;
  }();
  return v;
}
int BattleZoomPercent() { static int v = std::min(175, std::max(100, Cfg::GetInt("battle_zoom", 150))); return v; }

static std::set<const void*> g_visible;
static float g_shift, g_dshift; static int g_shiftMode, g_prevShiftMode;
void NotePanel(const void* who, bool visible) {
  size_t before = g_visible.size();
  if (visible) g_visible.insert(who); else g_visible.erase(who);
  if (g_visible.size() != before) l_debug("Layout: panel %s by %p -> %d visible", visible ? "shown" : "hidden", who, (int)g_visible.size());
}
void Update() {
  bool land = GetMode() == Mode::Landscape && !Naive();
  float target = (land && (g_shiftMode == 2 || (g_shiftMode == 1 && !g_visible.empty()))) ? 1.f : 0.f;
  if (g_shiftMode == 2 || g_prevShiftMode == 2) g_shift = target;       // battle: no visible motion, the slide is part of the battle layout
  else g_shift += (target - g_shift) * 0.2f;
  if (std::fabs(target - g_shift) < 0.005f) g_shift = target;
  g_prevShiftMode = g_shiftMode;
  float dt = (land && !g_visible.empty()) ? 1.f : 0.f;
  g_dshift += (dt - g_dshift) * 0.25f;
  if (std::fabs(dt - g_dshift) < 0.005f) g_dshift = dt;
}
void SetWorldShiftMode(int mode) { g_shiftMode = mode; }
float DialogueShiftNdc() {
  if (g_dshift == 0.f) return 0.f;
  float s = -(kPanelW * 0.5f) / (SurfaceW() * 0.5f) * g_dshift;
  return PanelLeft() ? -s : s;
}
float ShiftNdc() {
  if (g_shift == 0.f) return 0.f;
  float s = -(kPanelW * 0.5f) / (SurfaceW() * 0.5f) * g_shift;      // slide the picture left by half the panel width
  return PanelLeft() ? -s : s;
}

}  // namespace Layout
