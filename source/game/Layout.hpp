// game/Layout.hpp -- screen layout modes (re/arch/07_LANDSCAPE.md).  Runtime setting `layout=` in ux0:data/dqiv/config.txt (or dqivn<slot>.cfg):
//   landscape (default, design A)  960x544 surface: world cameras over the whole surface, UI = 408x544 panel (rect-clipped MenuCamera) at the right (`ui_side=left` -> left)
//   split     (design B)           960x544 surface: world left of the panel (552x544), panel right
//   portrait                       408x544 surface pillar-boxed in the middle of the display (the original Android layout)
// Everything the C# derives from "Camera.aspect" is split in two:
//   * UI (MenuCamera, eIndent, MenuMessage tablet flag, InputName): the panel, aspect 0.75 = the game's own "tablet" layout;
//   * world (MainCamera, FieldCamera, DSSA ortho camera): `Aspect()` keeps the VERTICAL extent of the 0.75 layout (341 world units tall) and widens the horizontal one.
// Portrait: every helper is the identity / full rect and behaviour is bit-identical to the pre-landscape build.
// `land_naive=1` (debug) keeps the old formulas on the 960x544 surface to show what breaks.  CMake -DDQIVN_LANDSCAPE=OFF = kill switch (portrait only).
#pragma once
#include "config.h"
#include "miniunity.hpp"

namespace Layout {
enum class Mode { Portrait, Landscape, Split };
Mode GetMode();                      // first call reads the cfg (Cfg::Get("layout"))
inline bool IsLandscape() { return GetMode() != Mode::Portrait; }
bool Naive();
bool PanelLeft();
int SurfaceW();                      // logical surface (Screen.width/height)
int SurfaceH();
int ViewX0();                        // where the surface sits on the 960x544 display
int ViewY0();
constexpr float kPanelW = 408.f, kPanelH = 544.f, kDesignAspect = kPanelW / kPanelH;   // 0.75
int UiW();                           // width of the UI area in surface pixels (tap slop, MenuMessage canvas): panel width, or the surface width in portrait
float UiPixelX0();                   // left edge of the UI panel in surface pixels (0 in portrait)
miniunity::Rect UiRect();            // normalised rects, origin bottom-left
miniunity::Rect WorldRect();
// aspect used for the VERTICAL extent of world/UI layouts (what the C# reads from Camera.aspect)
inline float Aspect(float cameraAspect) { return (IsLandscape() && !Naive()) ? kDesignAspect : cameraAspect; }
inline bool Wide() { return IsLandscape() && !Naive(); }

// Battle view in landscape (cfg battle_layout=large|panorama|band|panel, battle_zoom=100..175 for panorama):
//   large     whole surface, zoom auto-fitted to the monsters (measured once the battle has started, eased like DQIV-Recompiled), default
//   panorama  whole surface, zoom = battle_zoom (default 150 %)
//   panel     whole surface, no zoom (100 %)
//   band      the original DS 4:3 band (full width, black bars)
enum class BattleLayout { Large, Panorama, Panel, Band };
BattleLayout GetBattleLayout();
int BattleZoomPercent();
// Landscape (full-surface world) only: while a menu window covers the panel the world is slid to the left so the party stays in the visible part.
// 0 = centred, 1 = fully shifted (eased by Update, one call per frame).  ShiftNdc() = the NDC x offset to apply to world projections (<= 0).
void SetWorldShiftMode(int mode);                // 0 = the world never slides (default in the field: dialogues and menus do not move it); 1 = slides while a panel menu is open (cfg world_shift=1); 2 = fixed slide for the whole battle (cfg battle_shift, default 1), applied/removed instantly
float DialogueShiftNdc();                        // eased 0..-0.425: where the dialogue windows slide to while a panel menu is open
void NotePanel(const void* who, bool visible);   // MenuPortItem / MenuMessage report when their parts are shown/hidden/closed; panel is "open" while any is visible
void Update();
float ShiftNdc();
}  // namespace Layout
