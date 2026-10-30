// game/MeshList.cpp -- OWNER: WP4a.  C# source: re/cs_out/Assembly-CSharp/MeshList.cs
#include "game/MeshList.hpp"
#include "game/MeshMaker.hpp"
#include <cstring>

std::vector<Vector3> MeshList::s_vtx;
std::vector<Vector2> MeshList::s_uv;

void MeshList::Build(GameObject* o) { Build(o->GetComponent<MeshFilter>()); }

void MeshList::SetDynamicMesh(Mesh* target) {
  target->vertices = s_vtx;
  target->uv = s_uv;
  target->MarkDynamic();
}

void MeshList::BuildDynamicMesh(MeshFilter* filter, MeshCollider* collider) {
  Mesh* mesh = OwnMesh(filter);
  std::copy(m_vtx.begin(), m_vtx.end(), s_vtx.begin());
  std::copy(m_uv.begin(), m_uv.end(), s_uv.begin());
  collider->sharedMesh = nullptr;
  mesh->vertices = s_vtx;
  mesh->uv = s_uv;
  mesh->SetTriangles(m_triangles);
  mesh->Optimize();
  mesh->RecalculateBounds();
  collider->sharedMesh = mesh;
}

void MeshList::BuildDynamicMesh(MeshFilter* filter) {
  Mesh* mesh = OwnMesh(filter);
  std::copy(m_vtx.begin(), m_vtx.end(), s_vtx.begin());
  std::copy(m_uv.begin(), m_uv.end(), s_uv.begin());
  mesh->vertices = s_vtx;
  mesh->uv = s_uv;
  mesh->SetTriangles(m_triangles);
  mesh->Optimize();
  mesh->RecalculateBounds();
}

Mesh* MeshList::Build(MeshFilter* filter) {
  Mesh* mesh = OwnMesh(filter);
  mesh->Clear();
  mesh->SetSubMeshCount(1);
  mesh->vertices = m_vtx;
  mesh->uv = m_uv;
  mesh->SetTriangles(m_triangles);
  mesh->Optimize();
  mesh->RecalculateBounds();
  return mesh;
}

std::vector<MeshList> MeshList::Load(const std::vector<uint8_t>& data) {
  // BinaryReader over little-endian data: i32 count, then per list: i32 nVtx, i32 nTri, nVtx x (x,y,z,u,v floats), nTri x i32
  const uint8_t* p = data.data();
  const uint8_t* end = p + data.size();
  auto rd32 = [&](int32_t& v) { std::memcpy(&v, p, 4); p += 4; };
  auto rdf = [&](float& v) { std::memcpy(&v, p, 4); p += 4; };
  int32_t num = 0;
  rd32(num);
  std::vector<MeshList> array((size_t)num);
  size_t vmax = 0;
  for (int i = 0; i < num && p + 8 <= end; i++) {
    int32_t nv = 0, nt = 0;
    rd32(nv); rd32(nt);
    if ((size_t)nv > vmax) vmax = (size_t)nv;
    MeshList& m = array[i];
    m.m_vtx.resize(nv); m.m_uv.resize(nv); m.m_triangles.resize(nt);
    for (int k = 0; k < nv; k++) { rdf(m.m_vtx[k].x); rdf(m.m_vtx[k].y); rdf(m.m_vtx[k].z); rdf(m.m_uv[k].x); rdf(m.m_uv[k].y); }
    for (int k = 0; k < nt; k++) rd32(*(int32_t*)&m.m_triangles[k]);
  }
  s_vtx.assign(vmax, Vector3());
  s_uv.assign(vmax, Vector2());
  return array;
}
