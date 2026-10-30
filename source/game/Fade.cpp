// game/Fade.cpp -- OWNER: WP5a (first port by WP3, see Fade.hpp). Line-for-line port of Fade.cs: two full-screen fade planes (front = Custom/FrontFade on the
// MenuCamera, back = Custom/BackFade); their frame counters drive Global.Flag bit 0 (FLAG_FADE) which gates TouchPad input and the Capture effects.
#include "game/Fade.hpp"

#include "game/Global.hpp"
#include "game/Layout.hpp"
#include "game/TouchPad.hpp"

namespace {

struct FadePlane {
  GameObject* m_obj = nullptr;
  float m_endAlpha = 0.f, m_addAlpha = 0.f;
  int m_count = 0;
  Color m_color;
  Shader* m_shaders[2] = {};

  FadePlane(Transform* camera, Shader* shaders[2], int shaderNo, const std::vector<Vector3>& vtx, const std::vector<int>& indic, Color color) {
    m_shaders[0] = shaders[0]; m_shaders[1] = shaders[1];
    m_obj = new GameObject();
    m_obj->transform->SetParent(camera, false);
    m_obj->transform->localPosition = Vector3(0.f, 0.f, 1.f) * 20.f;
    m_obj->layer = 8;
    Mesh* mesh = m_obj->AddComponent<MeshFilter>()->mesh();
    mesh->vertices = vtx;
    mesh->SetTriangles(indic);
    Material* material = new Material(m_shaders[shaderNo]);
    m_obj->AddComponent<MeshRenderer>()->setMaterial(material);
    material->color = (m_color = color);
    Object::DontDestroyOnLoad(m_obj);
    m_obj->setActive(false);
  }
  void SetPriority(int shaderNo) { m_obj->renderer()->material()->shader = m_shaders[shaderNo]; }
  void CheckFade();
  void Command(int cmd, int frame) { Command(cmd, frame, m_obj->renderer()->material()->color); }
  void Command(int cmd, int frame, Color color) {
    m_color = color;
    if (frame > 0) {
      float a = m_obj->renderer()->material()->color.a;
      m_color.a = a;
      if ((cmd & 4) != 0) { m_endAlpha = 0.f; m_addAlpha = (0.f - a) / (float)frame; }
      else { m_endAlpha = 1.f; m_addAlpha = (1.f - a) / (float)frame; }
      m_count = frame;
      m_obj->setActive(true);
    } else {
      if ((cmd & 4) != 0) { m_color.a = 0.f; m_obj->setActive(false); }
      else { m_color.a = 1.f; m_obj->setActive(true); }
      m_count = 0;
      m_obj->renderer()->material()->color = m_color;
    }
    CheckFade();
  }
  void Update() {
    if (m_count <= 0 || (Global::Flag & 0x20000) != 0) return;
    m_count--;
    if (m_count > 0) {
      m_color.a += m_addAlpha;
    } else {
      m_color.a = m_endAlpha;
      if (m_endAlpha <= 0.f) m_obj->setActive(false);
      CheckFade();
    }
    m_obj->renderer()->material()->color = m_color;
  }
};

FadePlane* l_fade[2];
Camera* l_fullCam[2];
int l_targetLayer = 0;

void FadePlane::CheckFade() {
  if (l_fade[0]->m_count <= 0 && l_fade[1]->m_count <= 0) Global::Flag &= -2;
  else Global::Flag |= 1;
}

Shader* l_shader[2];

}  // namespace

void Fade::SystemInit() { (new GameObject("FadeCTRL"))->AddComponent<Fade>(); }
void Fade::SetLayer(int bBGLayer) { l_targetLayer = bBGLayer; }
void Fade::SetFadeOutColor(int frame, Color color) { l_fade[l_targetLayer]->Command(8, frame, color); }
void Fade::SetFadeOutBlack(int frame) { l_fade[l_targetLayer]->Command(8, frame, Color::black); }
void Fade::SetFadeOutWhite(int frame) { l_fade[l_targetLayer]->Command(8, frame, Color::white); }
void Fade::SetFadeInBlack(int frame) { l_fade[l_targetLayer]->Command(4, frame, Color::black); }
void Fade::SetFadeInWhite(int frame) { l_fade[l_targetLayer]->Command(4, frame, Color::white); }
void Fade::SetFadeIn(int frame) { l_fade[l_targetLayer]->Command(4, frame); }
bool Fade::IsEnd() { return l_fade[0] && l_fade[1] && l_fade[0]->m_count <= 0 && l_fade[1]->m_count <= 0; }
Camera* Fade::FullCamera(int i) { return l_fullCam[i]; }
void Fade::SetFadePriority(int layer, int priority) { l_fade[layer]->SetPriority(priority); }

Coroutine Fade::StartCo() {
  std::vector<Vector3> vtx = { Vector3(-1.f, -1.f, 0.f), Vector3(1.f, -1.f, 0.f), Vector3(1.f, 1.f, 0.f), Vector3(-1.f, 1.f, 0.f) };
  std::vector<int> indic = { 0, 2, 1, 2, 0, 3 };
  while ((Global::Flag & 0x10000) != 0) co_yield 0;
  l_shader[0] = Shader::Find("Custom/FrontFade");
  l_shader[1] = Shader::Find("Custom/BackFade");
  Transform* cam = TouchPad::m_camera->transform();
  (l_fade[0] = new FadePlane(cam, l_shader, 0, vtx, indic, Color::black))->m_obj->name = "FadeForMenu";
  (l_fade[1] = new FadePlane(cam, l_shader, 1, vtx, indic, Color::clear))->m_obj->name = "FadeForBG";
  if (Layout::Wide()) {
    // The MenuCamera only covers the UI panel: the fade planes (clip-space quads) get their own full-surface cameras, "BG" fade behind the UI
    // (depth 0.9, above the DSSA camera 0.5), "menu" fade in front of it (depth 5).
    for (int i = 0; i < 2; i++) {
      GameObject* g = new GameObject(i ? "FadeCameraBG" : "FadeCameraFront");
      Camera* c = l_fullCam[i] = g->AddComponent<Camera>();
      c->orthographic = true; c->orthographicSize = 1.f; c->nearClipPlane = -100.f; c->farClipPlane = 100.f;
      c->clearFlags = CameraClearFlags::Nothing; c->depth = i ? 0.9f : 5.f; c->cullingMask = 1 << (18 + i); c->rect = Rect(0.f, 0.f, 1.f, 1.f);
      Object::DontDestroyOnLoad(g);
      l_fade[i]->m_obj->transform->SetParent(g->transform, false);
      l_fade[i]->m_obj->layer = 18 + i;
    }
  }
}

void Fade::Update() {
  if (l_fade[1] != nullptr) {
    l_fade[0]->Update();
    l_fade[1]->Update();
  }
}
