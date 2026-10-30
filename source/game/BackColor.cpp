// game/BackColor.cpp -- OWNER: WP5a (written by WP2). Port of BackColor.cs: gradient quad (clip-space verts, shader "BackColor") parented to the main camera.
#include "game/BackColor.hpp"
#include "game/MainLoop.hpp"

static BackColor* l_self = nullptr;
static float l_acc = 0.f, l_now = 1.f;
static Color l_upEnd = Color::black, l_downEnd = Color::black;
static Color l_vtxColor[4] = { Color::black, Color::black, Color::black, Color::black };

static void ApplyColors() {
  Mesh* mesh = l_self->gameObject->GetComponent<MeshFilter>()->mesh();
  mesh->colors.assign(l_vtxColor, l_vtxColor + 4);
}

void BackColor::SystemInit() {
  GameObject* g = new GameObject();
  g->AddComponent<BackColor>()->name = "BackColor";
}
void BackColor::SetColor(Color all, int frame) { SetColor(all, all, frame); }
void BackColor::SetColor(Color up, Color down, int frame) {
  if (frame > 0) {
    l_now = 0.f;
    l_acc = 1.f / (float)frame;
    l_upEnd = up;
    l_downEnd = down;
  } else {
    l_now = 1.f;
    l_vtxColor[0] = up; l_vtxColor[1] = up; l_vtxColor[2] = down; l_vtxColor[3] = down;
    if (l_self) ApplyColors();
  }
}
void BackColor::Awake() {
  Mesh* mesh = gameObject->AddComponent<MeshFilter>()->mesh();
  mesh->vertices = { Vector3(1.f, 1.f, 0.f), Vector3(-1.f, 1.f, 0.f), Vector3(-1.f, -1.f, 0.f), Vector3(1.f, -1.f, 0.f) };
  mesh->SetTriangles(std::vector<int>{ 0, 2, 1, 2, 0, 3 }, 0);
  mesh->colors.assign(l_vtxColor, l_vtxColor + 4);
  l_self = this;
  gameObject->AddComponent<MeshRenderer>()->setMaterial(new Material(Shader::Find("BackColor")));
  transform()->SetParent(MainLoop::m_camera->transform());
  transform()->localScale = Vector3::one;
  transform()->localPosition = Vector3::forward * 20.f;
  Object::DontDestroyOnLoad(gameObject);
}
void BackColor::Update() {
  if (l_now < 1.f) {
    Color a = l_vtxColor[0], a2 = l_vtxColor[2];
    l_now += l_acc;
    if (l_now < 1.f) { a = Color::Lerp(a, l_upEnd, l_now); a2 = Color::Lerp(a2, l_downEnd, l_now); }
    else { a = l_upEnd; a2 = l_downEnd; }
    l_vtxColor[0] = a; l_vtxColor[1] = a; l_vtxColor[2] = a2; l_vtxColor[3] = a2;
    ApplyColors();
  }
}
