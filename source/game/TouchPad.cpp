// game/TouchPad.cpp -- OWNER: WP3. See TouchPad.hpp. C# source: TouchPad.cs (factUpdate), RawPad.cs, RawPads.cs.
#include "game/TouchPad.hpp"

#include <cmath>

#include "config.h"
#include "game/Global.hpp"
#include "platform/vita_input.hpp"
#include "utils/logger.h"

// ------------------------------------------------------------------------------------------------ RawPad / RawPads (verbatim)
void RawPad::SetTouch(const Touch& value) {
  fingerId = value.fingerId; position = value.position; deltaPosition = value.deltaPosition; deltaTime = value.deltaTime; tapCount = value.tapCount; phase = value.phase;
  bToggle = false;
  if (phase == TouchPhase::Ended || phase == TouchPhase::Canceled) {
    bPress = false;
  } else if (!bPress) {
    beginPosition = position;
    bPress = (bToggle = true);
  }
}

Ray RawPads::GetRay(Camera* cam) const {
  Vector3 position(m_pad.position.x, m_pad.position.y, 0.f);
  position.z = cam->transform()->position().z;
  return cam->ScreenPointToRay(position);
}
Vector3 RawPads::WorldDelta(Camera* cam) const {
  Vector3 vector(m_pad.position.x, m_pad.position.y, 0.f);
  Vector3 vector2(m_pad.deltaPosition.x, m_pad.deltaPosition.y, 0.f);
  vector2 = vector - vector2;
  vector.z = (vector2.z = cam->transform()->position().z);
  vector = cam->ScreenPointToRay(vector).origin;
  vector2 = cam->ScreenPointToRay(vector2).origin;
  return vector - vector2;
}
Vector3 RawPads::BeginWorldDelta(Camera* cam) const {
  Vector3 position(m_pad.position.x, m_pad.position.y, 0.f);
  Vector3 position2(m_pad.beginPosition.x, m_pad.beginPosition.y, 0.f);
  position.z = (position2.z = cam->transform()->position().z);
  position = cam->ScreenPointToRay(position).origin;
  position2 = cam->ScreenPointToRay(position2).origin;
  return position2 - position;
}
void RawPads::Update() {
  if (Input::touchCount > 0) { m_pad.SetTouch(Input::touches[0]); return; }
  m_pad.bPress = false;
  m_pad.bToggle = false;
}

// ------------------------------------------------------------------------------------------------ TouchPad
Camera* TouchPad::m_camera = nullptr;
int TouchPad::m_press = 0, TouchPad::m_toggle = 0, TouchPad::m_externalToggle = 0, TouchPad::s_prevPress = 0;
Ray TouchPad::m_pressRay;
bool TouchPad::m_bVisible = true, TouchPad::m_bPressESC = false, TouchPad::m_bToggleESC = false;
RawPads TouchPad::m_pad;
TouchPad::ePadState TouchPad::l_state = TouchPad::NONE;
bool TouchPad::l_NoMove = false, TouchPad::l_bSystemLock = false, TouchPad::l_padBlink = false;
Vector2 TouchPad::l_prsStart;
int TouchPad::l_inputMode = 0, TouchPad::l_padPos = 1, TouchPad::l_padSize = 1;

void TouchPad::SetPadBlink(bool b) { l_padBlink = b; }
void TouchPad::SetActivity(bool) { m_press = 0; m_toggle = 0; }
void TouchPad::SetPosPadSize(int pos, int size) { l_padPos = pos; l_padSize = size; }
void TouchPad::SetInputMode(int mode) { l_inputMode = mode; }
int TouchPad::GetInputMode() { return l_inputMode; }
bool TouchPad::GetPressRayFor(Camera* cam, Ray& ray) {
  if (!cam || !m_pad.bIsPress()) return false;
  ray = m_pad.GetRay(cam);
  return true;
}
bool TouchPad::GetPressRay(Ray& ray) {
  if (m_pad.bIsPress()) { ray = m_pressRay; return true; }
  return false;
}
bool TouchPad::GetToggleRay(Ray& ray) {
  if (m_pad.bIsToggle()) { ray = m_pressRay; return true; }
  return false;
}
void TouchPad::AdjustCameraOrtho() {
  GameObject* g = GameObject::Find("MenuCamera");
  Camera* c = g ? g->camera() : nullptr;
  if (c) c->orthographicSize = 225.f * (1.f / c->aspect());
}
void TouchPad::Awake() {
  m_camera = camera();
  Object::DontDestroyOnLoad(gameObject);
}

// factUpdate(): the physical pad word replaces the virtual pad; the touch machine keeps only NONE -> ANY (a tap = ANY, released without moving => 0x100).
void TouchPad::Update() {
  bool lock = (Global::Flag & 0x30003) != 0;      // fade / screen effect / pause / cleaning
  m_pad.Update();
  if (l_bSystemLock != lock) {
    if (l_bSystemLock = lock) { m_toggle = m_press = 0; s_prevPress = 0; }
  }
  if (lock) return;                               // C#: returns before touching m_press/m_toggle/m_externalToggle

  const vita_input::State& s = vita_input::GetState();
  int prs = s.held;                               // physical buttons (0 while the pointer cursor owns the pad)
  bool tap = false;
  if (m_pad.bIsPress()) {
    m_pressRay = m_pad.GetRay(m_camera ? m_camera : camera());
    switch (l_state) {
      case NONE:
        l_NoMove = true;
        l_state = ANY;
        l_prsStart.x = m_pressRay.origin.x; l_prsStart.y = m_pressRay.origin.z;
        break;
      case ANY: {
        float dx = m_pressRay.origin.x - l_prsStart.x, dz = m_pressRay.origin.z - l_prsStart.y;
        if (l_NoMove && std::sqrt(dx * dx + dz * dz) > 8.f) l_NoMove = false;     // s_AnyThreshold (8 UI units)
        break;
      }
      default: break;
    }
  } else {
    bool keyDown = Input::GetKeyDown(KeyCode::Escape);
    m_bToggleESC = !m_bPressESC & keyDown;
    m_bPressESC = keyDown;
    if (l_state == ANY && l_NoMove) tap = true;
    l_state = NONE;
  }
  m_press = prs | (tap ? PAD_ANY : 0);
  m_toggle = ((s_prevPress ^ m_press) & m_press);
  m_toggle |= m_externalToggle;
  m_externalToggle = 0;
  s_prevPress = m_press;
}
