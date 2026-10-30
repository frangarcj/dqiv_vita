// game/Staffroll.cpp -- OWNER: WP6.  Line port of Staffroll.cs (see Staffroll.hpp).
#include "game/Staffroll.hpp"

#include "game/MenuParts.hpp"
#include "game/eIndent.hpp"

void Staffroll::Setup(Texture2D** tex) {
  MeshFilter* meshFilter = gameObject->AddComponent<MeshFilter>();
  Mesh* mesh = OwnMesh(meshFilter);
  MeshRenderer* meshRenderer = gameObject->AddComponent<MeshRenderer>();
  Vector3 array[8];
  Vector2 array2[8];
  float num = 225.f;
  float num2 = 450.f;
  m_materials[0] = new Material(MenuParts::l_pictureShader);
  m_materials[0]->mainTexture = tex[1];
  m_materials[1] = new Material(MenuParts::l_pictureShader);
  m_materials[1]->mainTexture = tex[0];
  transform()->setPosition(eIndent::CalcScreenPosition(4, 0.f, 0.f, 0.f));
  array[0].Set(0.f - num, 0.f, num2);
  array[1].Set(num, 0.f, num2);
  array[2].Set(num, 0.f, 0.f);
  array[3].Set(0.f - num, 0.f, 0.f);
  array[4].Set(0.f - num, 0.f, 0.f);
  array[5].Set(num, 0.f, 0.f);
  array[6].Set(num, 0.f, 0.f - num2);
  array[7].Set(0.f - num, 0.f, 0.f - num2);
  array2[0].Set(0.f, 0.f);
  array2[1].Set(1.f, 0.f);
  array2[2].Set(1.f, 1.f);
  array2[3].Set(0.f, 1.f);
  array2[4].Set(0.f, 0.f);
  array2[5].Set(1.f, 0.f);
  array2[6].Set(1.f, 1.f);
  array2[7].Set(0.f, 1.f);
  mesh->SetSubMeshCount(2);
  mesh->vertices.assign(array, array + 8);
  mesh->uv.assign(array2, array2 + 8);
  mesh->SetTriangles(std::vector<int>{ 2, 3, 1, 1, 3, 0 }, 0);
  mesh->SetTriangles(std::vector<int>{ 6, 7, 5, 5, 7, 4 }, 1);
  m_uol.Adds(meshFilter, mesh, meshRenderer, m_materials[0], m_materials[1]);
  meshRenderer->setMaterials(std::vector<Material*>{ m_materials[0], m_materials[1] });
  meshRenderer->enabled = false;
  gameObject->layer = 8;
  std::vector<Color32> pixels((size_t)tex[0]->height * tex[0]->width, Color32(0, 0, 0, 0));
  tex[0]->SetPixels32(pixels.data(), (int)pixels.size());
  tex[0]->Apply();
  tex[1]->SetPixels32(pixels.data(), (int)pixels.size());
  tex[1]->Apply();
}

void Staffroll::OnDestroy() { m_uol.Destroy(); }
