// game/UiCameras.cpp -- see UiCameras.hpp.
#include "game/UiCameras.hpp"

#include <algorithm>
#include "game/Layout.hpp"
#include "game/MenuMessage.hpp"
#include "game/MenuPortItem.hpp"
#include "game/MenuPortItemTable.hpp"
#include "game/MenuPortSystem.hpp"
#include "game/TouchPad.hpp"
#include "platform/cfg.hpp"

namespace UiCameras {
bool s_creatingBoot = false; int s_bootPorts = 0;
namespace {
Camera* g_dialogue; Camera* g_status; Camera* g_boot; float g_scale = 1.f;

Camera* Make(const char* name, int layer, float depth) {
  GameObject* g = new GameObject(name);
  g->transform->localPosition = Vector3(0.f, 100.f, 0.f);
  g->transform->localRotation = Quaternion::Euler(90.f, 0.f, 0.f);          // like the MenuCamera: UI plane = XZ, x right, z up
  Camera* c = g->AddComponent<Camera>();
  c->orthographic = true; c->orthographicSize = 300.f; c->nearClipPlane = 1.f; c->farClipPlane = 200.f;   // 600 UI units over the surface height (0.907 px / unit)
  c->clearFlags = CameraClearFlags::Nothing; c->depth = depth; c->cullingMask = 1 << layer; c->rect = Rect(0.f, 0.f, 1.f, 1.f);
  Object::DontDestroyOnLoad(g);
  return c;
}
}  // namespace

bool DialogueEnabled() {
  static int v = (Layout::GetMode() == Layout::Mode::Landscape && !Layout::Naive() && Cfg::Get("dialogue", "bottom") != "panel") ? 1 : 0;
  return v != 0;
}
Camera* Dialogue() {
  if (!DialogueEnabled()) return nullptr;
  if (!g_dialogue) {
    g_scale = std::min(160, std::max(100, Cfg::GetInt("dialogue_scale", 100))) / 100.f;
    g_dialogue = Make("DialogueCamera", kDialogueLayer, 1.05f);
  }
  return g_dialogue;
}
bool BootEnabled() {
  static int v = (Layout::GetMode() == Layout::Mode::Landscape && !Layout::Naive() && Cfg::GetInt("boot_centre", 1) != 0) ? 1 : 0;
  return v != 0;
}
bool IsBootScreen(int id) { return (id >= 52 && id <= 56) || id == 72 || id == 73 || id == 74 || (id >= 75 && id <= 80); }   // StartMenu*, StartNameEntry, SaveOverwrite, ViewVolume, TitleTouch, Portal, Network*, License
Rect BootRect() { float w = Layout::kPanelW / (float)Layout::SurfaceW(); return Rect((1.f - w) * 0.5f, 0.f, w, 1.f); }
Camera* Boot() {
  if (!g_boot) { g_boot = Make("BootCamera", kBootLayer, 1.02f); g_boot->rect = BootRect(); }       // rect aspect 0.75 -> half width 225 units like the MenuCamera
  return g_boot;
}
bool BootPhase() { return BootEnabled() && (s_creatingBoot || s_bootPorts > 0); }
Camera* Status() { if (!g_status) g_status = Make("BattleStatusCamera", kStatusLayer, 0.95f); return g_status; }
Camera* PlateCamera() { return Layout::Wide() ? Status() : TouchPad::m_camera; }
Camera* ForLayer(int layer) { return layer == kBootLayer && g_boot ? g_boot : layer == kDialogueLayer && g_dialogue ? g_dialogue : layer == kStatusLayer && g_status ? g_status : TouchPad::m_camera; }

void Retag(Transform* t, int layer) {
  t->gameObject->layer = layer;
  for (Transform* c : t->children()) Retag(c, layer);
}

void RetagFrom(Transform* t, int from, int layer) {
  if (t->gameObject->layer == from) t->gameObject->layer = layer;
  for (Transform* c : t->children()) RetagFrom(c, from, layer);
}

void Update() {
  if (MenuMessage::m_window && BootEnabled()) {                  // the message window follows the phase: boot = centre panel (nominal portrait layout), game = bottom-centre dialogue
    int want = s_bootPorts > 0 ? kBootLayer : (g_dialogue ? kDialogueLayer : 8);
    if (want == kBootLayer) Boot();
    if (MenuMessage::m_window->gameObject->layer != want) Retag(MenuMessage::m_window->transform(), want);
  }
  {   // the Yes/No box is a singleton port opened once at start-up: its camera must follow the phase too (boot = centre panel, game = bottom-centre dialogue)
    int want = BootEnabled() && s_bootPorts > 0 ? kBootLayer : (g_dialogue ? kDialogueLayer : 8);
    if (MenuPortItemTable::s_yesNoPort > 0 && MenuPortItemTable::s_yesNoPort < 32)
      if (MenuPortItem* yn = dynamic_cast<MenuPortItem*>(MenuPortSystem::s_ports[MenuPortItemTable::s_yesNoPort]))
        if (yn->m_uiLayer != want && (BootEnabled() || g_dialogue)) {
          if (want == kBootLayer) Boot();
          for (MenuParts* parts : yn->m_partsList) RetagFrom(parts->transform(), yn->m_uiLayer, want);
          yn->m_uiLayer = want;
        }
  }
  if (!g_dialogue) return;
  // zoom about the bottom edge: half-height 300/s, centre z = -300 + 300/s; x: eased slide to the centre of the world area while a panel menu is open
  float H = 300.f / g_scale, halfW = H * (float)Layout::SurfaceW() / (float)Layout::SurfaceH();
  float x = -Layout::DialogueShiftNdc() * halfW;
  g_dialogue->orthographicSize = H;
  float margin = 2.f;                                  // UI units between the window and the bottom edge (the original windows keep a small gap too)
  float z = MenuMessage::s_bottomZ - margin + H;      // screen bottom (z - H) = window bottom - margin
  g_dialogue->transform()->setPosition(Vector3(x, 100.f, z));
}
}  // namespace UiCameras
