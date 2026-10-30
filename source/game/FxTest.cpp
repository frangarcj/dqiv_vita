#ifdef DQIV_DEV_TOOLS
#include "game/FxTest.hpp"

#include <cstdio>
#include <cstring>
#include <string>

#include "config.h"
#include "game/BackColor.hpp"
#include "game/Capture.hpp"
#include "game/MenuParts.hpp"
#include "game/SceneCamera.hpp"
#include "game/Fade.hpp"
#include "game/Global.hpp"
#include "game/Layout.hpp"
#include "game/MainLoop.hpp"
#include "game/Sepia.hpp"
#include "game/Spotlight.hpp"
#include "game/TouchPad.hpp"
#include "miniunity.hpp"
#include "utils/logger.h"

using namespace miniunity;

namespace {
std::string g_effect = "battle";
int g_start = 40, g_arg = 0;

GameObject* FxQuad(const char* name, int layer, float x, float y, float w, float h, const char* shader, Texture* tex, Color tint, bool grad = false) {
  GameObject* g = new GameObject(name);
  g->layer = layer;
  g->transform->localPosition = Vector3(x, y, 10.f);
  Mesh* m = new Mesh();
  float hx = w * 0.5f, hy = h * 0.5f;
  m->vertices = { Vector3(-hx, -hy, 0), Vector3(hx, -hy, 0), Vector3(hx, hy, 0), Vector3(-hx, hy, 0) };
  m->uv = { Vector2(0, 0), Vector2(1, 0), Vector2(1, 1), Vector2(0, 1) };
  if (grad) m->colors32 = { Color32(255, 0, 0, 255), Color32(0, 255, 0, 255), Color32(0, 0, 255, 255), Color32(255, 255, 0, 255) };
  m->SetTriangles(std::vector<int>{ 0, 2, 1, 0, 3, 2 });
  g->AddComponent<MeshFilter>()->setMesh(m);
  g->AddComponent<MeshRenderer>();
  Material* mat = new Material(Shader::Find(shader));
  mat->mainTexture = tex; mat->color = tint;
  g->renderer()->setMaterial(mat);
  Object::DontDestroyOnLoad(g);
  return g;
}

class FxDriver : public Component {
public:
  int frame = 0;
  void Update() override {
    frame++;
    if (frame == 3) Global::Flag &= ~0x10000;           // Capture::Start waits for it
    if (frame != g_start) return;
    l_info("FxTest: start '%s' at frame %d", g_effect.c_str(), frame);
    const std::string& e = g_effect;
    if (e == "battle") Capture::ScreenEffect(4);
    else if (e == "darklamp") Capture::ScreenEffect(5);
    else if (e == "morning") Capture::ScreenEffect(5 | 0x8000);
    else if (e == "riremito") Capture::ScreenEffect(6);
    else if (e == "tabi") Capture::ScreenEffect(3);
    else if (e == "tabirev") Capture::ScreenEffect(3 | 0x8000);
    else if (e == "sepia") Sepia::Setup();
    else if (e == "buttons") {
      Texture2D* col = new Texture2D(64, 64, TextureFormat::RGBA32);
      std::vector<Color32> px(64 * 64);
      for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) px[y * 64 + x] = Color32((uint8_t)(x * 4), (uint8_t)(y * 4), 200, 255);
      col->SetPixels32(px.data(), (int)px.size()); col->Apply();
      float W = (float)Screen::width;
      const char* sh[3] = { "Custom/Picture", "Custom/ButtonGray", "Custom/ButtonReverse" };
      for (int i = 0; i < 3; i++) FxQuad("btn", 0, 0, 150.f - i * 130.f, W * 0.6f, 100.f, sh[i], col, Color::white);
    }
    else if (e == "spot" || e == "torch") {
      Spotlight::Create(e == "torch", 0.f, 0.f, 0.f);
      Spotlight::SetSideColor(0.f, 0.f, 0.1f, 0.85f);
      Spotlight::SetLightColor(1.f, 0.8f, 0.4f, 0.3f);
      Spotlight::SetSize(96);
      Spotlight::SetVisible(true);
    }
  }
};
}  // namespace

namespace FxTest {
bool Requested() {
  FILE* f = fopen(DATA_PATH "fxtest" DQIVN_SLOT_STR ".on", "r");
  if (!f) return false;
  char name[32]; int s, a;
  int n = fscanf(f, "%31s %d %d", name, &s, &a);
  if (n >= 1) g_effect = name;
  if (n >= 2) g_start = s;
  if (n >= 3) g_arg = a;
  fclose(f);
  return true;
}

void Create() {
  Global::Flag |= 0x10000;
  GameObject* mg = new GameObject("FxMain");
  mg->tag = "MainCamera";
  Camera* cam = mg->AddComponent<Camera>();
  cam->orthographic = true; cam->orthographicSize = Screen::height * 0.5f; cam->nearClipPlane = 0.3f; cam->farClipPlane = 1000.f;
  cam->clearFlags = CameraClearFlags::Color; cam->backgroundColor = Color(0.2f, 0.2f, 0.25f, 1.f); cam->depth = 0.f; cam->cullingMask = ~(1 << 8);
  mg->transform->localPosition = Vector3(0, 0, -10.f);
  if (Layout::IsLandscape()) cam->rect = Layout::WorldRect();     // landscape: full surface; split: the world area left of the panel
  Object::DontDestroyOnLoad(mg);
  MainLoop::m_camera = cam;

  GameObject* ug = new GameObject("FxMenuCamera");
  Camera* ucam = ug->AddComponent<Camera>();
  ucam->orthographic = true; ucam->orthographicSize = Screen::height * 0.5f; ucam->nearClipPlane = 0.3f; ucam->farClipPlane = 1000.f;
  ucam->clearFlags = CameraClearFlags::Depth; ucam->depth = 5.f; ucam->cullingMask = 1 << 8;
  ug->transform->localPosition = Vector3(0, 0, -10.f);
  if (Layout::IsLandscape()) ucam->rect = Layout::UiRect();        // the 408x544 UI panel
  Object::DontDestroyOnLoad(ug);
  TouchPad::m_camera = ucam;

  // checkerboard (64x64, 8px cells) over the whole screen + a gradient square and the SQEX logo: shows warping/blur/colour effects
  Texture2D* chk = new Texture2D(64, 64, TextureFormat::RGBA32);
  std::vector<Color32> px(64 * 64);
  for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) {
    bool a = ((x / 8) + (y / 8)) & 1;
    px[y * 64 + x] = a ? Color32(230, 230, 230, 255) : Color32(40, 80, 160, 255);
  }
  chk->SetPixels32(px.data(), (int)px.size()); chk->Apply();
  float W = (float)Screen::width, H = (float)Screen::height;
  if (Layout::IsLandscape()) {                           // stand-in for a menu window: a translucent panel on the MenuCamera layer
    GameObject* pq = FxQuad("panel", 8, 0, 0, 408.f, 544.f, "Custom/Picture", nullptr, Color(0.9f, 0.6f, 0.1f, 0.6f));
    (void)pq;
  }
  FxQuad("chk", 0, 0, 0, W, H, "Custom/Picture", chk, Color::white);
  FxQuad("grad", 0, -W * 0.2f, H * 0.2f, W * 0.4f, W * 0.4f, "Custom/DssaNormal", nullptr, Color::white, true);
  FxQuad("logo", 0, 0, -H * 0.25f, W * 0.8f, W * 0.8f * 0.33f, "Custom/Picture", Resources::Load<Texture>("boot/sqex"), Color::white);

  SceneCamera::SystemInit();                             
  MenuParts::SystemInit();                               // shader/font statics (Endroll uses l_fontShader)
  Fade::SystemInit();
  Capture::SystemInit();
  (new GameObject("FxDriver"))->AddComponent<FxDriver>();
}
}  // namespace FxTest

#endif  // DQIV_DEV_TOOLS
