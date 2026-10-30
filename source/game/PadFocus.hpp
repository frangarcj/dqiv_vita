// game/PadFocus.hpp -- OWNER: WP3 (input).  Physical-button navigation of the MenuPortItem screens (the Android original is touch-only).
// The live screen (the highest port whose native menu is polling input this frame and has enabled buttons) gets a focus cursor drawn as a pushed button:
// D-pad / left stick (via the pad word) move the focus by geometry, Cross presses it (same message a touch release sends), Circle = the screen's cancel
// button, L / R = buttons coded as pad-left / pad-right (page arrows), Triangle / Square = buttons coded X / Y.  While a screen owns the focus, the
// directional / A / B bits are masked out of the pad word handed to libdq4core (NativeManager) so the same press does not also walk the hero / talk.
#pragma once
namespace miniunity { class Camera; }
class MenuPortItem; class MenuParts;
namespace PadFocus {
void BeginFrame();                       // MenuPortSystem, before the ports update (Circle -> TouchPad::m_bToggleESC for the previous owner's cancel button)
void Register(MenuPortItem* item);       // MenuPortItem::Update: this screen is polling input now
void EndFrame();                         // MenuPortSystem, after the ports updated: pick the owner, move / press the focus
bool Owns();                             // a screen owns the pad this frame
void Mask(int& press, int& toggle);      // remove the bits the focus consumed from the pad word handed to libdq4core (modal keyboard: everything)
void Reset();
void Modal();                            // a host-side widget (InputName keyboard) drives the pad itself this frame: the generic focus yields and the walking bits stay masked
void ShowMarker(MenuParts* b, miniunity::Camera* cam = nullptr);           // draw / hide (nullptr) the yellow focus rectangle around a button; `cam` = the camera that renders the button (default MenuCamera; InputName keys use the keyboard camera), the rect is clipped to that camera's pixel rect

extern int s_lastOwnerPort;
}
