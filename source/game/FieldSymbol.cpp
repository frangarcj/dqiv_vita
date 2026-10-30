// game/FieldSymbol.cpp -- OWNER: WP5c.  Port of FieldSymbol.cs (DQ4: 21 symbols, no Volcano / SetUV / colour delegate).
#include "game/FieldSymbol.hpp"
#include "game/Field.hpp"

const Rect FieldSymbol::s_symbols[21] = {
  Rect(0.f, 0.5f, 0.0625f, 17.f / 32.f), Rect(3.f / 32.f, 0.5f, 5.f / 32.f, 17.f / 32.f), Rect(0.1875f, 0.5f, 0.25f, 0.5625f), Rect(9.f / 32.f, 0.5f, 11.f / 32.f, 17.f / 32.f),
  Rect(0.375f, 0.5f, 0.4375f, 17.f / 32.f), Rect(15.f / 32.f, 0.5f, 0.5f, 17.f / 32.f), Rect(17.f / 32.f, 0.5f, 0.5625f, 17.f / 32.f), Rect(19.f / 32.f, 0.5f, 21.f / 32.f, 17.f / 32.f),
  Rect(0.6875f, 0.5f, 23.f / 32.f, 17.f / 32.f), Rect(0.75f, 0.5f, 25.f / 32.f, 17.f / 32.f), Rect(0.8125f, 0.5f, 0.875f, 17.f / 32.f), Rect(29.f / 32.f, 0.5f, 0.9375f, 0.5625f),
  Rect(0.f, 0.5625f, 1.f / 32.f, 0.625f), Rect(0.0625f, 0.5625f, 3.f / 32.f, 19.f / 32.f), Rect(0.125f, 0.5625f, 0.1875f, 0.625f), Rect(7.f / 32.f, 0.5625f, 0.25f, 0.625f),
  Rect(9.f / 32.f, 0.5625f, 11.f / 32.f, 0.625f), Rect(0.375f, 0.5625f, 0.4375f, 19.f / 32.f), Rect(15.f / 32.f, 0.5625f, 0.5f, 19.f / 32.f), Rect(17.f / 32.f, 0.5625f, 19.f / 32.f, 0.625f),
  Rect(0.625f, 0.5625f, 0.6875f, 0.625f)
};

FieldSymbol::FieldSymbol(int picid, float x, float y, Material* baseMaterial) {
  if (picid < 0 || picid >= 21) picid = 0;
  Rect rect = s_symbols[picid];
  float x2 = (rect.width - rect.x) * 1024.f;
  float num = (rect.height - rect.y) * 1024.f;
  m_material = new Material(*baseMaterial);
  m_symbol = new GameObject();
  m_material->color = Field::m_color;
  MeshFilter* mf = m_symbol->AddComponent<MeshFilter>();
  Mesh* mesh = new Mesh(); mf->setMesh(mesh); mf->meshIsInstance = true;       // C# `.mesh` of an empty filter: an own instance
  MeshRenderer* mr = m_symbol->AddComponent<MeshRenderer>(); mr->setMaterial(m_material);
  mesh->vertices = { Vector3(0.f, 0.f, 0.f), Vector3(x2, 0.f, 0.f), Vector3(x2, 0.f, 0.f - num), Vector3(0.f, 0.f, 0.f - num) };
  mesh->uv = { Vector2(rect.x, rect.y), Vector2(rect.width, rect.y), Vector2(rect.width, rect.height), Vector2(rect.x, rect.height) };
  mesh->SetSubMeshCount(1);
  mesh->SetTriangles(std::vector<int>{ 0, 1, 3, 1, 2, 3 }, 0);
  m_symbol->layer = 9;
  m_symbol->transform->setPosition(Vector3(x, -45.f, 0.f - y));
}

void FieldSymbol::SetColor(Color color) { color.a = m_alpha; m_material->color = color; }
void FieldSymbol::SetAlpha(float alpha) { Color color = m_material->color; color.a = (m_alpha = alpha); m_material->color = color; }

void FieldSymbol::Release() {
  if (!m_symbol) return;
  MeshFilter* mf = m_symbol->GetComponent<MeshFilter>();
  m_material->mainTexture = nullptr;
  Object::Destroy(m_material);
  if (mf) { if (mf->sharedMesh) Object::Destroy(mf->sharedMesh); Object::Destroy(mf); }
  Object::Destroy(m_symbol->GetComponent<MeshRenderer>());
  Object::Destroy(m_symbol);
  m_symbol = nullptr; m_material = nullptr;
}
