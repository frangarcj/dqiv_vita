// game/FieldSymbol.cpp -- OWNER: WP5c.  Port of FieldSymbol.cs.  picid 19 (the volcano) is a `Volcano` component in the C# (WP2 owns Volcano): until game/Volcano.hpp
// exists the mountain is drawn as the plain quad.
#include "game/FieldSymbol.hpp"
#include "game/Field.hpp"

#if __has_include("game/Volcano.hpp")
#include "game/Volcano.hpp"
#define DQIV_HAVE_VOLCANO 1
#endif

const Rect FieldSymbol::s_symbols[42] = {
  Rect(0.f, 23.f / 32.f, 0.0625f, 0.75f), Rect(0.0625f, 23.f / 32.f, 0.125f, 0.77125f), Rect(0.125f, 23.f / 32.f, 0.1875f, 25.f / 32.f), Rect(0.1875f, 23.f / 32.f, 15.f / 64.f, 0.75f),
  Rect(15.f / 64.f, 23.f / 32.f, 9.f / 32.f, 0.75f), Rect(9.f / 32.f, 23.f / 32.f, 0.3125f, 0.75f), Rect(0.3125f, 23.f / 32.f, 11.f / 32.f, 0.75f), Rect(11.f / 32.f, 23.f / 32.f, 25.f / 64.f, 0.75f),
  Rect(25.f / 64.f, 23.f / 32.f, 0.4375f, 0.75f), Rect(0.4375f, 23.f / 32.f, 15.f / 32.f, 0.75f), Rect(15.f / 32.f, 23.f / 32.f, 0.5f, 0.75f), Rect(33.f / 64.f, 23.f / 32.f, 0.5625f, 0.75f),
  Rect(0.5625f, 23.f / 32.f, 19.f / 32.f, 47.f / 64.f), Rect(19.f / 32.f, 23.f / 32.f, 0.625f, 25.f / 32.f), Rect(0.625f, 23.f / 32.f, 21.f / 32.f, 25.f / 32.f), Rect(43.f / 64.f, 23.f / 32.f, 45.f / 64.f, 0.75f),
  Rect(45.f / 64.f, 23.f / 32.f, 0.75f, 0.75f), Rect(0.75f, 23.f / 32.f, 51.f / 64.f, 0.75f), Rect(51.f / 64.f, 23.f / 32.f, 27.f / 32.f, 25.f / 32.f), Rect(27.f / 32.f, 23.f / 32.f, 57.f / 64.f, 25.f / 32.f),
  Rect(57.f / 64.f, 23.f / 32.f, 29.f / 32.f, 0.75f), Rect(59.f / 64.f, 23.f / 32.f, 0.9375f, 0.75f), Rect(61.f / 64.f, 23.f / 32.f, 31.f / 32.f, 0.75f), Rect(0.f, 25.f / 32.f, 1.f / 32.f, 0.8125f),
  Rect(3.f / 64.f, 25.f / 32.f, 5.f / 64.f, 27.f / 32.f), Rect(3.f / 32.f, 25.f / 32.f, 0.125f, 0.8125f), Rect(9.f / 64.f, 25.f / 32.f, 13.f / 64.f, 27.f / 32.f), Rect(7.f / 32.f, 25.f / 32.f, 9.f / 32.f, 27.f / 32.f),
  Rect(9.f / 32.f, 25.f / 32.f, 11.f / 32.f, 27.f / 32.f), Rect(11.f / 32.f, 25.f / 32.f, 25.f / 64.f, 0.8125f), Rect(25.f / 64.f, 25.f / 32.f, 29.f / 64.f, 0.8125f), Rect(29.f / 64.f, 25.f / 32.f, 31.f / 64.f, 0.8125f),
  Rect(31.f / 64.f, 25.f / 32.f, 33.f / 64.f, 0.8125f), Rect(33.f / 64.f, 25.f / 32.f, 35.f / 64.f, 0.8125f), Rect(1.f / 64.f, 0.9375f, 1.f / 32.f, 31.f / 32.f), Rect(3.f / 64.f, 0.9375f, 0.0625f, 31.f / 32.f),
  Rect(5.f / 64.f, 0.9375f, 3.f / 32.f, 31.f / 32.f), Rect(0.f, 0.875f, 1.f / 32.f, 29.f / 32.f), Rect(1.f / 32.f, 0.875f, 0.0625f, 29.f / 32.f), Rect(0.125f, 0.875f, 0.1875f, 0.9375f),
  Rect(0.f, 29.f / 32.f, 1.f / 32.f, 0.9375f), Rect(5.f / 64.f, 57.f / 64.f, 7.f / 64.f, 59.f / 64.f)
};

static std::vector<Vector2> QuadUV(const Rect& r) { return { Vector2(r.x, r.y), Vector2(r.width, r.y), Vector2(r.width, r.height), Vector2(r.x, r.height) }; }

FieldSymbol::FieldSymbol(int picid, float x, float y, Material* baseMaterial) {
  if (picid < 0 || picid >= 42) picid = 0;
  Rect rect = s_symbols[picid];
  float num = (rect.width - rect.x) * 1024.f;
  float num2 = (rect.height - rect.y) * 1024.f;
  m_material = new Material(*baseMaterial);
  m_symbol = new GameObject();
  m_material->color = Field::m_color;
  MeshFilter* mf = m_symbol->AddComponent<MeshFilter>();
  Mesh* mesh = new Mesh(); mf->setMesh(mesh); mf->meshIsInstance = true;       // C# `.mesh` of an empty filter: an own instance
  MeshRenderer* mr = m_symbol->AddComponent<MeshRenderer>(); mr->setMaterial(m_material);
  SetColor = [this](Color c) { SetColorDefault(c); };
  if (picid == 21 || picid == 22) {
    num = num * 1.5f + 1.f;
  } else if (picid >= 37) {
    SetColor = [](Color) {};
    m_material->color = Color::white;
  } else if (picid == 19) {
#ifdef DQIV_HAVE_VOLCANO
    m_symbol->AddComponent<Volcano>()->Init(mesh, m_material, num, num2, s_symbols[19], s_symbols[40], s_symbols[41]);
    m_symbol->layer = 9;
    m_symbol->transform->localPosition = Vector3(x, -45.f, 0.f - y);
    return;
#endif
  }
  mesh->vertices = { Vector3(0.f, 0.f, 0.f), Vector3(num, 0.f, 0.f), Vector3(num, 0.f, 0.f - num2), Vector3(0.f, 0.f, 0.f - num2) };
  mesh->uv = QuadUV(rect);
  mesh->SetSubMeshCount(1);
  mesh->SetTriangles(std::vector<int>{ 0, 1, 3, 1, 2, 3 }, 0);
  m_symbol->layer = 9;
  m_symbol->transform->localPosition = Vector3(x, -45.f, 0.f - y);
}

void FieldSymbol::SetAlpha(float alpha) { Color color = m_material->color; color.a = (m_alpha = alpha); m_material->color = color; }

void FieldSymbol::Release() {
  if (!m_symbol) return;
  MeshFilter* mf = m_symbol->GetComponent<MeshFilter>();
  Object::Destroy(m_material);
  if (mf) { if (mf->sharedMesh) Object::Destroy(mf->sharedMesh); Object::Destroy(mf); }
  Object::Destroy(m_symbol->GetComponent<MeshRenderer>());
  Object::Destroy(m_symbol);
  m_symbol = nullptr; m_material = nullptr;
}

void FieldSymbol::SetUV(int picid) {
  if (picid < 0 || picid >= 42) return;
  MeshFilter* mf = m_symbol->GetComponent<MeshFilter>();
  if (mf && mf->sharedMesh) { mf->sharedMesh->uv = QuadUV(s_symbols[picid]); mf->sharedMesh->SetDirty(); }
}
