// game/MeshMaker.cpp -- OWNER: WP4a.  C# source: re/cs_out/Assembly-CSharp/MeshMaker.cs
#include "game/MeshMaker.hpp"
#include "utils/logger.h"

Mesh* OwnMesh(MeshFilter* mf) {
  return mf->mesh();            // WP1's MeshFilter::mesh() creates an engine-owned empty instance for an empty filter (like Unity)
}

Vector3 MeshMaker::s_vtx[4];
Vector2 MeshMaker::s_uv[4];
int MeshMaker::s_indicBase[6] = { 0, 1, 3, 3, 1, 2 };
int MeshMaker::s_indic[6];

MeshMaker::MeshMaker(Material* material) {
  m_materials[0] = material;
  l_tryList.emplace_back();
}

void MeshMaker::Clear() {
  l_vtxList.clear();
  l_uvList.clear();
  l_tryList.clear();
  l_tryList.emplace_back();
  for (int i = 0; i < 3; i++) m_materials[i] = nullptr;
  m_owned.clear();
  m_max = Vector2(-3.402823466e+38f, 3.402823466e+38f);
}

void MeshMaker::SetVertex(Vector3 basePoint, float sizeW, float sizeH) {
  s_vtx[0] = basePoint;
  s_vtx[1] = basePoint + Vector3(sizeW, 0.f, 0.f);
  s_vtx[2] = basePoint + Vector3(sizeW, 0.f, 0.f - sizeH);
  s_vtx[3] = basePoint + Vector3(0.f, 0.f, 0.f - sizeH);
}

void MeshMaker::SetUV(float u, float v, float uSize, float vSize) {
  uSize += u;
  vSize += v;
  s_uv[0].Set(u, vSize);
  s_uv[1].Set(uSize, vSize);
  s_uv[2].Set(uSize, v);
  s_uv[3].Set(u, v);
}

void MeshMaker::Apply(bool bInBounds, int insert) {
  if (bInBounds) {
    for (int i = 0; i < 4; i++) {
      const Vector3& vector = s_vtx[i];
      if (m_max.x < vector.x) m_max.x = vector.x;
      if (m_max.y > vector.z) m_max.y = vector.z;
    }
  }
  std::vector<int>& indic = cur();
  if (insert >= 0) {
    for (int j = 0; j < 6; j++) s_indic[j] = s_indicBase[j] + insert;
    l_vtxList.insert(l_vtxList.begin() + insert, s_vtx, s_vtx + 4);
    l_uvList.insert(l_uvList.begin() + insert, s_uv, s_uv + 4);
    insert = 6 * (insert >> 2);
    for (int k = insert; k < (int)indic.size(); k++) indic[k] += 4;       // (C#: only the current index list is shifted)
    indic.insert(indic.begin() + insert, s_indic, s_indic + 6);
  } else {
    int count = (int)l_vtxList.size();
    for (int l = 0; l < 6; l++) s_indic[l] = s_indicBase[l] + count;
    l_vtxList.insert(l_vtxList.end(), s_vtx, s_vtx + 4);
    l_uvList.insert(l_uvList.end(), s_uv, s_uv + 4);
    indic.insert(indic.end(), s_indic, s_indic + 6);
  }
}

void MeshMaker::DivTriangles(Material* newMaterial) {
  m_materials[l_tryList.size()] = newMaterial;
  l_tryList.emplace_back();
}

Vector3 MeshMaker::CalcBoundsPosition(int indent, float x, float y, float z) {
  Vector3 result = Vector3::zero;
  switch (indent) {
    case 1: result = Vector3(m_max.x * 0.5f + x, y, 0.f + z); break;
    case 2: result = Vector3(m_max.x + x, y, 0.f + z); break;
    case 3: result = Vector3(0.f + x, y, m_max.y * 0.5f + z); break;
    case 4: result = Vector3(m_max.x * 0.5f + x, y, m_max.y * 0.5f + z); break;
    case 5: result = Vector3(m_max.x + x, y, m_max.y * 0.5f + z); break;
    case 6: result = Vector3(0.f + x, y, m_max.y + z); break;
    case 7: result = Vector3(m_max.x * 0.5f + x, y, m_max.y + z); break;
    case 8: result = Vector3(m_max.x + x, y, m_max.y + z); break;
  }
  return result;
}

void MeshMaker::ReBuild(GameObject* obj, int indent) {
  Mesh* mesh = OwnMesh(obj->GetComponent<MeshFilter>());
  Vector3 vector = CalcBoundsPosition(indent, 0.f, 0.f, 0.f);
  std::vector<Vector3> array = l_vtxList;
  int count = (int)l_tryList.size();
  for (size_t i = 0; i < array.size(); i++) array[i] = array[i] - vector;
  mesh->Clear();
  mesh->vertices = array;
  mesh->uv = l_uvList;
  mesh->SetSubMeshCount(count);
  for (int j = 0; j < count; j++) mesh->SetTriangles(l_tryList[j], j);
  mesh->RecalculateBounds();
}

void MeshMaker::Build(UnityOBJList& uol, GameObject* obj, int indent) {
  MeshFilter* meshFilter = obj->AddComponent<MeshFilter>();
  Mesh* mesh = OwnMesh(meshFilter);
  MeshRenderer* meshRenderer = obj->AddComponent<MeshRenderer>();
  std::vector<Vector3> array = l_vtxList;
  Vector3 vector = CalcBoundsPosition(indent, 0.f, 0.f, 0.f);
  for (size_t i = 0; i < array.size(); i++) array[i] = array[i] - vector;
  mesh->vertices = array;
  mesh->uv = l_uvList;
  mesh->SetSubMeshCount((int)l_tryList.size());
  for (size_t j = 0; j < l_tryList.size(); j++) mesh->SetTriangles(l_tryList[j], (int)j);
  switch (l_tryList.size()) {
    case 1: meshRenderer->setMaterial(m_materials[0]); break;
    case 2: meshRenderer->setMaterials({ m_materials[0], m_materials[1] }); break;
    case 3: meshRenderer->setMaterials({ m_materials[0], m_materials[1], m_materials[2] }); break;
    default: l_error("MeshMaker::Build: Logic Error"); break;
  }
  mesh->Optimize();
  mesh->RecalculateBounds();
  uol.Adds(mesh, meshFilter, meshRenderer);
  for (Material* m : m_owned) uol.Add(m);
  obj->layer = 8;
  meshRenderer->enabled = false;
}
