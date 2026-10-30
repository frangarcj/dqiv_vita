// game/UiCameras.hpp -- landscape: extra full-surface UI cameras next to the panel-clipped MenuCamera (re/arch/07_LANDSCAPE.md §10).
//   Dialogue camera (layer 21): MenuMessage window + the Yes/No choice window are drawn at the BOTTOM-CENTRE of the surface (same UI scale as the panel, or
//     cfg dialogue_scale = percent, default 100) instead of inside the 408 px panel. While a panel menu is open (shop, status...) it eases to the centre of the
//     world area left of the panel so the two never overlap. Touch rays for these ports come from this camera (MenuPortSystem), PadFocus too.
//   Status camera (layer 20): the enlarged battle party row (BattleStatusLayout).
// cfg: dialogue=bottom|panel (default bottom; panel = old behaviour), dialogue_scale=100..160.   Portrait / split / naive: everything here is a no-op.
#pragma once
#include "miniunity.hpp"
using namespace miniunity;

namespace UiCameras {
constexpr int kStatusLayer = 20, kDialogueLayer = 21, kBootLayer = 22;
bool DialogueEnabled();
Camera* Dialogue();                          // nullptr when disabled
Camera* Status();
Camera* PlateCamera();                       // camera the monster name plates / marks are placed with: the full-surface Status camera in landscape (Layout::Wide), else the MenuCamera
Camera* ForLayer(int layer);                 // camera that draws objects of this layer (the MenuCamera for everything else)
void Retag(Transform* root, int layer);      // layer of root and all its descendants
void RetagFrom(Transform* root, int fromLayer, int layer);   // only the objects that are on fromLayer
// Boot / start flow (TitleTouch, start menus, save slots, SaveOverwrite, name entry, licence...): the 408 px panel is CENTRED on the surface (Boot camera, layer 22)
// instead of sitting at the right, and nothing slides.  A "boot port" is counted from its creation to its close; Yes/No boxes and the message window follow the phase.
bool BootEnabled();
bool IsBootScreen(int screenId);
Camera* Boot();
Rect BootRect();
bool BootPhase();                            // a boot port exists (or one is being created)
extern bool s_creatingBoot; extern int s_bootPorts;
void Update();                               // once per frame (SceneCamera::LayoutUpdate)
}
