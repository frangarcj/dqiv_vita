// game/Riseup.cpp -- OWNER: WP5a. Port of Riseup.cs.
#include "game/Riseup.hpp"
#include "game/MenuParts.hpp"
#include "game/SceneCamera.hpp"

static const int s_offsetExhibit = 2000;
static Mesh* s_mesh = nullptr;
static Shader* s_shader[2] = { nullptr, nullptr };

Riseup::Riseup(int no) {
  GameObject* go = new GameObject();
  m_riseup = go->transform;
  m_riseup->localScale = Vector3::one * 1.f;
  go->AddComponent<MeshFilter>()->sharedMesh = s_mesh;
  if (s_offsetExhibit <= no) { no -= s_offsetExhibit; m_material = new Material(s_shader[1]); }
  else m_material = new Material(s_shader[0]);
  SetTexture(no);
  go->AddComponent<MeshRenderer>()->setMaterial(m_material);
}
void Riseup::Release() {
  if (!m_riseup) return;
  Object::Destroy(m_riseup->gameObject);       // components die with the GameObject
  Object::Destroy(m_material);
  m_riseup = nullptr; m_material = nullptr;
}
void Riseup::SetTexture(int no) { MenuParts::SetRiseupMaterial(m_material, 0, no); }
void Riseup::SetVisible(bool v) { m_riseup->renderer()->enabled = v; }
void Riseup::SetColor(Color c) { m_material->color = c; }
void Riseup::SetPosition(Vector3 pos) { m_riseup->setPosition(pos); }
void Riseup::SetBillboard() { m_riseup->setRotation(SceneCamera::s_billboardRot); m_billboard_request = false; }
void Riseup::SetFieldMode() { m_riseup->gameObject->layer = 9; m_riseup->localScale = Vector3::one * 28.8f; }
void Riseup::SetFieldPosition(Vector3 pos) { m_riseup->setPosition(Vector3(pos.x, pos.z, 0.f - pos.y)); }
void Riseup::SystemInit() {
  static const int tri[6] = { 0, 1, 3, 3, 1, 2 };
  s_shader[0] = Shader::Find("Custom/Riseup");
  s_shader[1] = Shader::Find("Custom/Chara");
  s_mesh = new Mesh();
  s_mesh->vertices = { Vector3(-0.48f, 0.f, 0.f), Vector3(0.48f, 0.f, 0.f), Vector3(0.48f, 0.96f, 0.48f), Vector3(-0.48f, 0.96f, 0.48f) };
  s_mesh->uv = { Vector2(1.f / 32.f, -1.f / 32.f), Vector2(0.f, -1.f / 32.f), Vector2(0.f, 0.f), Vector2(1.f / 32.f, 0.f) };
  s_mesh->SetSubMeshCount(1);
  s_mesh->SetTriangles(tri, 6, 0);
  Object::DontDestroyOnLoad(s_shader[0]); Object::DontDestroyOnLoad(s_shader[1]); Object::DontDestroyOnLoad(s_mesh);
}
