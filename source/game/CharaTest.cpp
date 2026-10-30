#ifdef DQIV_DEV_TOOLS
#include "game/CharaTest.hpp"

#include <cmath>
#include <cstdio>

#include "config.h"
#include "game/BackColor.hpp"
#include "game/BillboardObject.hpp"
#include "game/Chara.hpp"
#include "game/Global.hpp"
#include "game/MainLoop.hpp"
#include "game/SceneCamera.hpp"
#include "miniunity.hpp"
#include "utils/logger.h"

using namespace miniunity;

namespace {

int g_first = 1, g_count = 12;

class CharaTestDriver : public Component {
public:
  std::vector<Chara*> charas; int frame = 0;

  void Awake() override {
    BillboardObject::SystemInit();
    Chara::SystemInit();
    SceneCamera::SystemInit();
    Global::InitCamera(0);
    SceneCamera::SetAngleDeg(Global::CameraInitParams[0].GetAngleVector());
    Chara::SetSleepNear(0, 0);
    const int cols = 4;
    for (int i = 0; i < g_count; i++) {
      int tex = (g_first + i) | (4 << 16);                       // CharaTex/hNNN, 4 pattern rows, size type 0 (32x64 sprite)
      Chara* c = new Chara(tex);
      int cx = i % cols, cz = i / cols;
      Vector3 p((cx - (cols - 1) * 0.5f) * 1.6f, 0.f, (1.f - cz) * 1.6f);
      c->SetPosition(p); c->SetPattern(0, i & 3); c->SetColor(Color(1.f, 1.f, 1.f, 1.f)); c->SetVisible(true); c->SetShadowVisible(true);
      c->m_billboard_request = true;
      charas.push_back(c);
    }
    if (g_count > 2) {                                           // the last one lies down (sleep pose)
      Chara* c = charas.back(); c->SetSleepPose(true); c->SetSleepDir(0);
      charas[1]->SetColor(Color(1.f, 1.f, 1.f, 0.5f));          // translucent: shadow alpha follows
    }
    l_info("CharaTest: %d sprites from CharaTex/h%03d", (int)charas.size(), g_first);
  }
  void Update() override {
    frame++;
    Vector3 target(0.f, 0.f, 0.f);
    float dist = 39.55f, ang = 47.f * 0.0174533f, yaw = std::sin(frame * 0.01f) * 0.6f;
    SceneCamera::SetTarget(target);
    SceneCamera::SetPosition(target + Vector3(std::sin(yaw) * dist * std::cos(ang), dist * std::sin(ang), -std::cos(yaw) * dist * std::cos(ang)));
    SceneCamera::Update();
    Chara::UpdateBillboardShadows();
    for (size_t i = 0; i < charas.size(); i++) {
      if (frame % 12 == 0) charas[i]->SetPattern((frame / 12) & 3, (int)(i & 3));
      charas[i]->SetBillboard();
    }
  }
};

}  // namespace

namespace CharaTest {
bool Requested() {
  FILE* f = fopen(DATA_PATH "charatest" DQIVN_SLOT_STR ".on", "r");
  if (!f) return false;
  int a, b; if (fscanf(f, "%d %d", &a, &b) == 2) { g_first = a; g_count = b; }
  fclose(f);
  return true;
}
void Create() {
  GameObject* camGo = new GameObject("CharaTestCamera");
  camGo->tag = "MainCamera";
  Camera* cam = camGo->AddComponent<Camera>();
  cam->clearFlags = CameraClearFlags::Color; cam->backgroundColor = Color(0.25f, 0.45f, 0.25f, 1.f); cam->depth = -1.f; cam->cullingMask = -1;
  MainLoop::m_camera = cam;
  Object::DontDestroyOnLoad(camGo);
  GameObject* drv = new GameObject("CharaTestDriver");
  drv->AddComponent<CharaTestDriver>();
  Object::DontDestroyOnLoad(drv);
}
}  // namespace CharaTest

#endif  // DQIV_DEV_TOOLS
