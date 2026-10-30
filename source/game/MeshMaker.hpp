// game/MeshMaker.hpp -- OWNER: WP4a.  Ports of MeshMaker.cs (quad batcher for the XZ-plane UI meshes) and UnityOBJList.cs.
#pragma once
#include <vector>
#include "miniunity.hpp"
using namespace miniunity;

// C# `meshFilter.mesh` getter: creates an (empty) instance mesh when the filter has none, else the instance copy. The mini-engine's MeshFilter::mesh()
// returns null for a filter without mesh, so every UI-side `filter.mesh` goes through this.
Mesh* OwnMesh(MeshFilter* mf);

// UnityOBJList: List<Object> that destroys everything it holds (MenuParts.OnDestroy).
struct UnityOBJList : std::vector<Object*> {
  void DontDestroyOnLoad() { for (Object* o : *this) Object::DontDestroyOnLoad(o); }
  template <class... T> void Adds(T*... p) { (push_back(p), ...); }
  void Add(Object* o) { push_back(o); }
  void Destroy() { for (Object* o : *this) Object::Destroy(o); clear(); }
};

class MeshMaker {
public:
  static Vector3 s_vtx[4];
  static Vector2 s_uv[4];
  Material* m_materials[3] = { nullptr, nullptr, nullptr };
  std::vector<Material*> m_owned;                 // clones made for this part: handed to the UnityOBJList in Build() so they are destroyed with it (C#: left to the GC)
  Vector2 m_max = Vector2(-3.402823466e+38f, 3.402823466e+38f);          // (float.MinValue, float.MaxValue)

  explicit MeshMaker(Material* material);
  void Clear();
  int GetDivNum() { return (int)l_tryList.size() - 1; }
  int GetNowVertexIndex() { return (int)l_vtxList.size(); }
  Material* GetNowMaterial() { return m_materials[GetDivNum()]; }
  void SetVertex(Vector3 basePoint, float sizeW, float sizeH);
  void SetUV(float u, float v, float uSize, float vSize);
  void Apply(bool bInBounds, int insert = -1);
  void DivTriangles(Material* newMaterial);
  Vector3 CalcBoundsPosition(int indent, float x = 0.f, float y = 0.f, float z = 0.f);
  void ReBuild(GameObject* obj, int indent);
  void Build(UnityOBJList& uol, GameObject* obj, int indent);
private:
  static int s_indicBase[6];
  static int s_indic[6];
  std::vector<Vector3> l_vtxList;
  std::vector<Vector2> l_uvList;
  std::vector<std::vector<int>> l_tryList;   // sub-mesh index lists; C# `l_indicList` (the list Apply() appends to) == l_tryList.back()
  std::vector<int>& cur() { return l_tryList.back(); }
};
