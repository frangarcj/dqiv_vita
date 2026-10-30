// game/Spotlight.cpp -- OWNER: WP6. Line-by-line port of Spotlight.cs.
#include "game/Layout.hpp"
#include "game/Spotlight.hpp"

#include "game/MainLoop.hpp"

namespace { Spotlight* l_self = nullptr; }

void Spotlight::Create(bool bTouchMode, float x, float y, float z) {
  if (l_self == nullptr) {
    l_self = (new GameObject())->AddComponent<Spotlight>();
    l_self->Setup(bTouchMode, x, y, z);
  }
}

void Spotlight::Release() {
  if (l_self != nullptr) {
    DestroyObject(l_self->m_LightPoly);
    Object::DestroyImmediate(l_self->m_LightMtl);
    Object::DestroyImmediate(l_self->m_SideMtl);
    l_self->m_IsSpot = false;
    Object::Destroy(l_self->gameObject);
    l_self = nullptr;
  }
}

void Spotlight::Setup(bool bTorchMode, float x, float y, float z) {
  Vector3 vector = Camera::main()->WorldToViewportPoint(Vector3(x, y, 0.f - z));
  if (!m_IsSpot) {
    m_IsSpot = true;
    m_LightMtl = new Material(Shader::Find("Custom/Post-Spotlight"));
    m_SideMtl = new Material(Shader::Find("Custom/Pre-Spotlight"));
    m_LightTex = dynamic_cast<Texture2D*>(Resources::Load<Texture>(bTorchMode ? "etc/en02" : "etc/en01"));
    m_LightMtl->mainTexture = m_LightTex;
    m_SideMtl->mainTexture = m_LightTex;
    m_Pos = Vector2(vector.x * 2.f - 1.f, (vector.y + 0.04f) * 2.f - 1.f);
    m_LightPoly = RemakePolygon(m_LightPoly, (float)m_PolySize, m_Pos, m_LightColor);
    m_LightPoly->renderer()->setMaterials(std::vector<Material*>{ m_SideMtl, m_LightMtl });
    AttachToCamera(m_LightPoly);
    m_LightMtl->color = m_LightColor;
    m_SideMtl->color = m_SideColor;
  }
}

void Spotlight::AttachToCamera(GameObject* go) {
  go->transform->SetParent(MainLoop::m_camera->transform(), false);
  go->transform->localScale = Vector3::one;
  go->transform->localPosition = Vector3::forward * 20.f;
  go->layer = 0;
}

void Spotlight::SetLightColor(float r, float g, float b, float a) {
  if (l_self != nullptr && l_self->m_IsSpot) l_self->m_LightMtl->color = (l_self->m_LightColor = Color(r, g, b, a));
}
void Spotlight::SetSideColor(float r, float g, float b, float a) {
  if (l_self != nullptr && l_self->m_IsSpot) l_self->m_SideMtl->color = (l_self->m_SideColor = Color(r, g, b, a));
}
void Spotlight::SetSize(int size) {
  if (l_self != nullptr && l_self->m_IsSpot) {
    l_self->m_PolySize = size;
    l_self->m_LightPoly = RemakePolygon(l_self->m_LightPoly, (float)l_self->m_PolySize, l_self->m_Pos, l_self->m_LightColor);
  }
}
void Spotlight::SetVisible(bool b) {
  if (l_self != nullptr && l_self->m_IsSpot) l_self->m_LightPoly->setActive(b);
}

GameObject* Spotlight::RemakePolygon(GameObject*& poly, float radius, Vector2 pos, Color color) {
  (void)color;
  if (poly == nullptr) {
    GameObject* gameObject = new GameObject();
    Mesh* mesh = gameObject->AddComponent<MeshFilter>()->mesh();
    MeshRenderer* meshRenderer = gameObject->AddComponent<MeshRenderer>();
    RemakeVertices(mesh, radius, pos);
    mesh->SetSubMeshCount(2);
    mesh->SetTriangles(std::vector<int>{ 0, 1, 5, 0, 5, 4, 5, 1, 2, 5, 2, 6, 7, 2, 3, 7, 6, 2, 0, 4, 7, 0, 7, 3, 8, 9, 10, 8, 10, 11 }, 0);
    mesh->SetTriangles(std::vector<int>{ 8, 9, 10, 8, 10, 11 }, 1);
    mesh->uv = { Vector2(), Vector2(), Vector2(), Vector2(), Vector2(), Vector2(), Vector2(), Vector2(), Vector2(0.f, 1.f), Vector2(1.f, 1.f), Vector2(1.f, 0.f), Vector2(0.f, 0.f) };
    meshRenderer->castShadows = false;
    meshRenderer->receiveShadows = false;
    Object::DontDestroyOnLoad(gameObject);
    poly = gameObject;
  } else {
    RemakeVertices(poly->GetComponent<MeshFilter>()->mesh(), radius, pos);
  }
  return poly;
}

void Spotlight::RemakeVertices(Mesh* mesh, float radius, Vector2 pos) {
  float num = (radius + 10.f) * (1.f / 256.f);
  float camAspect = MainLoop::m_camera->aspect();
  float num2 = num * Layout::Aspect(camAspect);                // half height (clip): the design-aspect height
  if (Layout::Wide()) num *= Layout::kDesignAspect / camAspect;   // half width: same pixel radius as the portrait layout, i.e. the spot stays circular on the 960x544 surface
  float x = pos.x, y = pos.y;
  mesh->vertices = {
    Vector3(-1.f, 1.f, 0.f), Vector3(1.f, 1.f, 0.f), Vector3(1.f, -1.f, 0.f), Vector3(-1.f, -1.f, 0.f),
    Vector3(0.f - num + x, num2 + y, 0.f), Vector3(num + x, num2 + y, 0.f), Vector3(num + x, 0.f - num2 + y, 0.f), Vector3(0.f - num + x, 0.f - num2 + y, 0.f),
    Vector3(0.f - num + x, num2 + y, 0.f), Vector3(num + x, num2 + y, 0.f), Vector3(num + x, 0.f - num2 + y, 0.f), Vector3(0.f - num + x, 0.f - num2 + y, 0.f) };
}

void Spotlight::DestroyObject(GameObject* go) {
  for (int i = 0; i < go->transform->childCount(); i++) DestroyObject2(go->transform->GetChild(i)->gameObject);
  if (MeshFilter* c = go->GetComponent<MeshFilter>()) Object::DestroyImmediate(c);
  go->transform->SetParent(nullptr);
  Object::DestroyImmediate(go);
}
void Spotlight::DestroyObject2(GameObject* go) {
  for (int i = 0; i < go->transform->childCount(); i++) DestroyObject2(go->transform->GetChild(i)->gameObject);
  if (MeshFilter* c = go->GetComponent<MeshFilter>()) Object::DestroyImmediate(c);
  Object::DestroyImmediate(go);
}
