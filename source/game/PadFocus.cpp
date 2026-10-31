// game/PadFocus.cpp -- OWNER: WP3.  See PadFocus.hpp.
#include "game/PadFocus.hpp"
#include "game/UiCameras.hpp"
#include <cmath>
#include <string>
#include <vector>
#include "game/MenuPortItem.hpp"
#include "game/Sound.hpp"
#include "game/TouchPad.hpp"
#include "platform/vita_input.hpp"
#include "pack/dqiv_pack.h"
#include "utils/logger.h"

namespace PadFocus {
int s_lastOwnerPort = -1;
static std::vector<MenuPortItem*> s_live;
static MenuPortItem* s_owner = nullptr; static MenuParts* s_focus = nullptr;
static int s_hold = 0; static int s_holdFrame = -1;
static bool s_ownsNow = false;

// ---- focus marker: a yellow rectangle outline drawn by its own overlay camera (layer 14) in logical pixels, around the focused button's collider
static GameObject* s_bar[4];
static int s_modalFrame = -10;
void Modal() { s_modalFrame = Time::frameCount; }
static bool ModalRecent() { return Time::frameCount - s_modalFrame <= 1; }
void ShowMarker(MenuParts* b, Camera* camArg) {
  if (!s_bar[0]) {
    GameObject* camGo = new GameObject("PadFocusCamera");
    Camera* cam = camGo->AddComponent<Camera>();
    cam->orthographic = true; cam->orthographicSize = Screen::height * 0.5f; cam->nearClipPlane = 0.3f; cam->farClipPlane = 1000.f;
    cam->clearFlags = CameraClearFlags::Depth; cam->depth = 999.f; cam->cullingMask = 1 << 14;
    Object::DontDestroyOnLoad(camGo);
    static const uint8_t px[16] = { 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255 };
    Texture2D* white = Texture2D::FromPack("padfocus_white", 2, 2, DQIV_TF_RGBA8888, 1, 0, 4, px);
    for (int i = 0; i < 4; i++) {
      GameObject* q = GameObject::CreatePrimitive(PrimitiveType::Quad);
      q->name = "PadFocusBar"; q->layer = 14;
      Material* m = new Material(Shader::Find("Custom/Picture"));
      m->mainTexture = white; m->color = Color(1.f, 0.85f, 0.1f, 1.f);
      q->renderer()->setMaterial(m);
      Object::DontDestroyOnLoad(q);
      s_bar[i] = q;
    }
  }
  bool on = b != nullptr;
  float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  if (b) {
    Camera* cam = camArg ? camArg : UiCameras::ForLayer(b->gameObject->layer);       // camArg = InputName keyboard camera; otherwise the camera that actually draws this button (UiCameras)
    BoxCollider* bc = b->gameObject->GetComponent<BoxCollider>();
    if (bc && cam) {
      Vector3 c = bc->center, e(bc->size.x * 0.5f, 0.f, bc->size.z * 0.5f);
      Transform* t = b->transform();
      Vector3 p0 = cam->WorldToScreenPoint(t->TransformPoint(c - e)), p1 = cam->WorldToScreenPoint(t->TransformPoint(c + e));
      x0 = std::fmin(p0.x, p1.x); x1 = std::fmax(p0.x, p1.x); y0 = std::fmin(p0.y, p1.y); y1 = std::fmax(p0.y, p1.y);
      if (camArg) {                                                   // clip to the camera's viewport (the keyboard list scrolls under its frame)
        Rect pr = cam->pixelRect();
        float cx0 = std::fmax(x0, pr.x), cx1 = std::fmin(x1, pr.x + pr.width), cy0 = std::fmax(y0, pr.y), cy1 = std::fmin(y1, pr.y + pr.height);
        if (cx1 - cx0 < 6.f || cy1 - cy0 < 6.f) on = false; else { x0 = cx0; x1 = cx1; y0 = cy0; y1 = cy1; }
      }
    } else on = false;
  }
  const float th = 3.f;
  float cx[4] = { (x0 + x1) * 0.5f, (x0 + x1) * 0.5f, x0, x1 }, cy[4] = { y1, y0, (y0 + y1) * 0.5f, (y0 + y1) * 0.5f };
  float sw[4] = { x1 - x0 + th, x1 - x0 + th, th, th }, sh[4] = { th, th, y1 - y0 + th, y1 - y0 + th };
  for (int i = 0; i < 4; i++) {
    s_bar[i]->renderer()->enabled = on;
    if (!on) continue;
    s_bar[i]->transform->localScale = Vector3(sw[i], sh[i], 1.f);
    s_bar[i]->transform->localPosition = Vector3(cx[i] - Screen::width * 0.5f, cy[i] - Screen::height * 0.5f, 10.f);
  }
}

static bool Usable(MenuParts* b) {
  if (!b || !b->m_button || !b->m_button->m_bEnable || !b->gameObject || !b->gameObject->activeInHierarchy()) return false;
  Renderer* r = b->renderer();
  if (r && !r->enabled) return false;
  return true;
}
static Vector3 Pos(MenuParts* b) { Vector3 p = b->transform()->position(); return Vector3(p.x, p.z, 0.f); }     // UI plane = XZ, screen y = world z

static bool HasButtons(MenuPortItem* it) { for (MenuParts* b : it->m_buttonList) if (Usable(b)) return true; return false; }

static bool TapButton(MenuParts* b);
void BeginFrame() {
  std::vector<MenuPortItem*> prevLive; prevLive.swap(s_live);
  s_ownsNow = false;
  if (s_owner && (TouchPad::m_toggle & 0x400)) {                   // Circle: the owner's cancel button (MenuPortItem::Update handles m_bToggleESC)
    // DQ4 CityMap / WorldLargeMap: the owner is the whole-window tap button and the Back button (external toggle, code 1024) sits on a second live port: look there too
    auto tryCancel = [&](MenuPortItem* it) {
      for (MenuParts* b : it->m_buttonList) if ((b->m_gamepad == 32 || b->m_gamepad == 1024) && Usable(b)) {
        if (!(b->m_externalToggle && TapButton(b))) TouchPad::m_bToggleESC = true;        // external Back: a synthetic finger tap (see TapButton); others: the ESC path
        return true;
      }
      return false;
    };
    if (!tryCancel(s_owner)) for (MenuPortItem* it : prevLive) if (it != s_owner && tryCancel(it)) break;
  }
}
void Register(MenuPortItem* item) { s_live.push_back(item); }
bool Owns() { return s_ownsNow || (s_owner && s_focus) || ModalRecent(); }
void Mask(int& press, int& toggle) {
  if (ModalRecent()) { press = 0; toggle = 0; return; }
  if (s_ownsNow || (s_owner && s_focus)) { press &= ~0x660F; toggle &= ~0x660F; }          // d-pad, A, B, Start, Select
}
void Reset() { s_focus = nullptr; s_owner = nullptr; }

static MenuParts* Initial(MenuPortItem* it) {
  if (it->m_active >= 0) for (MenuParts* b : it->m_buttonList) if (b->m_buttonId == it->m_active && Usable(b)) return b;
  MenuParts* best = nullptr;
  for (MenuParts* b : it->m_buttonList) {
    if (!Usable(b)) continue;
    if (b->m_gamepad == 32 || b->m_gamepad == 1024) { if (!best) best = nullptr; continue; }      // not the cancel button as the first focus
    if (!best) { best = b; continue; }
    Vector3 a = Pos(b), c = Pos(best);
    if (a.y > c.y + 4.f || (std::fabs(a.y - c.y) <= 4.f && a.x < c.x)) best = b;                  // top-left first
  }
  if (!best) for (MenuParts* b : it->m_buttonList) if (Usable(b)) { best = b; break; }
  return best;
}

static MenuParts* Move(MenuPortItem* it, MenuParts* from, int dx, int dy) {      // dx/dy in screen axes (y up)
  Vector3 f = Pos(from);
  MenuParts* best = nullptr; float bestScore = 1e30f;
  for (MenuParts* b : it->m_buttonList) {
    if (b == from || !Usable(b)) continue;
    if (b->m_gamepad == 32 || b->m_gamepad == 1024) { /* cancel buttons are reachable too (Back) */ }
    Vector3 p = Pos(b);
    float ddx = p.x - f.x, ddy = p.y - f.y;
    float along = ddx * dx + ddy * dy, perp = std::fabs(dx ? ddy : ddx);
    if (along <= 1.f) continue;
    if (perp > along * 1.8f) continue;                                           // inside a ~60 degree cone
    float score = along + perp * 2.f;
    if (score < bestScore) { bestScore = score; best = b; }
  }
  if (best) return best;
  // wrap around: the button farthest on the opposite side, closest in the perpendicular axis
  float bestW = 1e30f;
  for (MenuParts* b : it->m_buttonList) {
    if (b == from || !Usable(b)) continue;
    Vector3 p = Pos(b);
    float along = (p.x - f.x) * dx + (p.y - f.y) * dy, perp = std::fabs(dx ? p.y - f.y : p.x - f.x);
    if (along >= -1.f) continue;
    float score = -along * -0.5f + perp * 4.f + (-along) * -1.f * -0.0f;           // prefer small perp, then far side
    score = perp * 4.f - (-along) * 0.5f;
    if (score < bestW) { bestW = score; best = b; }
  }
  return best;
}

// Centre of a button in logical surface pixels (bottom-left origin), seen through the camera that draws it: where a finger would have to be.
static bool ScreenCentre(MenuParts* b, float& x, float& y) {
  Camera* cam = UiCameras::ForLayer(b->gameObject->layer);
  BoxCollider* bc = b->gameObject->GetComponent<BoxCollider>();
  if (!cam || !bc) return false;
  Transform* t = b->transform();
  Vector3 h(bc->size.x * 0.5f, 0.f, bc->size.z * 0.5f);
  Vector3 p0 = cam->WorldToScreenPoint(t->TransformPoint(bc->center - h)), p1 = cam->WorldToScreenPoint(t->TransformPoint(bc->center + h));
  x = (p0.x + p1.x) * 0.5f; y = (p0.y + p1.y) * 0.5f;
  return true;
}
// External-toggle buttons (CityMap / WorldLargeMap Back) hand their key to libdq4core only when pressed the way the original does: a real finger tap (ANY toggle + the external toggle one frame
// later).  A bare B from the pad does not close the map, so the pad presses them with a synthetic tap at the button, like the pointer cursor does (same fix as the DQ6 port).
static bool TapButton(MenuParts* b) {
  float x, y;
  if (!ScreenCentre(b, x, y)) return false;
  vita_input::InjectTap(x, y, 4);
  return true;
}
static void Press(MenuPortItem* it, MenuParts* b) {
  if ((b->m_gamepad & (b->m_externalToggle ? 2560 : 2064)) != 0) Sound::PlaySE(300);
  if (b->m_externalToggle) { if (!TapButton(b)) TouchPad::m_externalToggle |= b->m_gamepad; }
  else it->Send(1, b->m_buttonId, b->m_gamepad);
}
static MenuParts* ByGamepad(MenuPortItem* it, int code) {
  for (MenuParts* b : it->m_buttonList) if (b->m_gamepad == code && Usable(b)) return b;
  return nullptr;
}

static void DebugLive() {
  static std::string last;
  std::string cur;
  for (MenuPortItem* it : s_live) { int n = 0; std::string g; for (MenuParts* b : it->m_buttonList) if (Usable(b)) { n++; char gb[24]; snprintf(gb, sizeof(gb), " i%d:g%d", (int)b->m_buttonId, (int)b->m_gamepad); g += gb; } char buf[96]; snprintf(buf, sizeof(buf), "[p%d t%d pad%d can%d act%d btn%d%s]", (int)it->m_port, (int)it->m_type, (int)it->m_bPad, (int)it->m_bCancel, (int)it->m_bActive, n, g.c_str()); cur += buf; }
  if (cur != last) { last = cur; l_debug("PadFocus live: %s", cur.c_str()); }
}
void EndFrame() {
  DebugLive();
  if (ModalRecent()) { s_owner = nullptr; s_focus = nullptr; s_ownsNow = true; return; }          // InputName drives the pad itself
  MenuPortItem* owner = nullptr;
  for (MenuPortItem* it : s_live)                                    // pad-capable screens (native setup msg bPad) first, then the highest port number (= opened last)
    if (HasButtons(it) && (!owner || (it->m_bPad && !owner->m_bPad) || (it->m_bPad == owner->m_bPad && it->m_port > owner->m_port))) owner = it;
  if (owner != s_owner) {
    s_focus = nullptr; s_owner = owner; s_hold = 0;
    if (owner) { s_focus = Initial(owner); l_debug("PadFocus: owner port %d type %d, focus button %d", (int)owner->m_port, (int)owner->m_type, s_focus ? (int)s_focus->m_buttonId : -1); }
    s_lastOwnerPort = owner ? owner->m_port : -1;
  }
  if (!s_owner) { if (s_bar[0]) ShowMarker(nullptr); return; }
  if (s_focus && !Usable(s_focus)) s_focus = Initial(s_owner);
  if (!s_focus) s_focus = Initial(s_owner);
  s_ownsNow = true;
  int t = TouchPad::m_toggle & ~0x100, h = TouchPad::m_press;
  int dx = 0, dy = 0;
  bool dirNow = false;
  if (t & 1) dy = 1; else if (t & 2) dy = -1; else if (t & 4) dx = -1; else if (t & 8) dx = 1;
  if (h & 0xF) { s_hold++; if (!(dx | dy) && s_hold > 16 && (s_hold % 5) == 0) { dirNow = true; dy = (h & 1) ? 1 : (h & 2) ? -1 : 0; dx = dy ? 0 : ((h & 4) ? -1 : 1); } } else s_hold = 0;
  (void)dirNow;
  if ((dx || dy) && s_focus) {
    MenuParts* n = Move(s_owner, s_focus, dx, dy);
    if (n && n != s_focus) { s_focus = n; Sound::PlaySE(300); }
  }
  if (t & 0x10) { if (MenuParts* b = ByGamepad(s_owner, MenuPortItem::PAD_L)) Press(s_owner, b); }   // L: page-left arrow (DQ4: PAD_L = 8, PAD_R = 4)
  if (t & 0x20) { if (MenuParts* b = ByGamepad(s_owner, MenuPortItem::PAD_R)) Press(s_owner, b); }   // R: page-right arrow
  if (t & 0x800) { if (MenuParts* b = ByGamepad(s_owner, 64)) Press(s_owner, b); }           // Triangle: X-coded button
  if (t & 0x1000) { if (MenuParts* b = ByGamepad(s_owner, 128)) Press(s_owner, b); }         // Square: Y-coded button
  if ((t & 0x200) && s_focus) Press(s_owner, s_focus);                                       // Cross
  ShowMarker(s_focus);
}
}  // namespace PadFocus
