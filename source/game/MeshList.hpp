// game/MeshList.hpp -- OWNER: WP4a.  Port of MeshList.cs: the 365 pre-baked window frames of Menu/frameData (FRAME_ENUM ids).
// Difference from the C#: Build(Mesh) there swaps a shared "work mesh" around (a Unity mesh-allocation trick); here the frame is written into the
// filter's own instance mesh in place, which has the same observable result.
#pragma once
#include <cstdint>
#include <vector>
#include "miniunity.hpp"
using namespace miniunity;

class MeshList {
public:
  std::vector<Vector3> m_vtx;
  std::vector<Vector2> m_uv;
  std::vector<int> m_triangles;
  static std::vector<Vector3> s_vtx;      // dynamic-mesh scratch arrays (sized to the biggest frame)
  static std::vector<Vector2> s_uv;

  void Build(GameObject* o);
  static void SetDynamicMesh(Mesh* target);
  void BuildDynamicMesh(MeshFilter* filter, MeshCollider* collider);
  void BuildDynamicMesh(MeshFilter* filter);
  Mesh* Build(MeshFilter* filter);                    // in-place version of `Mesh Build(Mesh m)`; returns the filter's mesh
  static std::vector<MeshList> Load(const std::vector<uint8_t>& data);
};
