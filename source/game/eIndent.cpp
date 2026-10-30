// game/eIndent.cpp -- OWNER: WP4a.  C# source: re/cs_out/Assembly-CSharp/eIndent.cs
#include "game/Layout.hpp"
#include "game/eIndent.hpp"
#include "game/MeshMaker.hpp"

namespace eIndent {

float SCREEN_HALF_HEIGHT = 0.f, SCREEN_HEIGHT = 0.f;

void Init() {
  SCREEN_HALF_HEIGHT = 225.f * (1.f / Layout::Aspect(Camera::main()->aspect()));
  SCREEN_HEIGHT = SCREEN_HALF_HEIGHT * 2.f;
}

Vector3 CalcLocalPosition(GameObject* reference, int indent, float x, float y, float z) {
  Vector3 zero = Vector3::zero;
  if (reference != nullptr) {
    Bounds bounds = OwnMesh(reference->GetComponent<MeshFilter>())->bounds;
    Vector3 extents = bounds.extents;
    switch (indent) {
      case 0: zero.Set(0.f - extents.x + x, y, extents.z + z); break;
      case 1: zero.Set(0.f + x, y, extents.z + z); break;
      case 2: zero.Set(extents.x + x, y, extents.z + z); break;
      case 3: zero.Set(0.f - extents.x + x, y, 0.f + z); break;
      case 4: zero.Set(x, y, z); break;
      case 5: zero.Set(extents.x + x, y, 0.f + z); break;
      case 6: zero.Set(0.f - extents.x + x, y, 0.f - extents.z + z); break;
      case 7: zero.Set(0.f + x, y, 0.f - extents.z + z); break;
      case 8: zero.Set(extents.x + x, y, 0.f - extents.z + z); break;
    }
    return zero + bounds.center;
  }
  return CalcScreenPosition(indent, x, y, z);
}

Vector3 CalcPosition(GameObject* reference, int indent, float x, float y, float z) {
  return CalcLocalPosition(reference, indent, x, y, z) + reference->transform->position();
}

Vector3 CalcScreenPosition(int indent, float x, float y, float z) {
  Vector3 zero = Vector3::zero;
  switch (indent) {
    case 0: zero.Set(-225.f + x, y, SCREEN_HALF_HEIGHT + z); break;
    case 1: zero.Set(0.f + x, y, SCREEN_HALF_HEIGHT + z); break;
    case 2: zero.Set(225.f + x, y, SCREEN_HALF_HEIGHT + z); break;
    case 3: zero.Set(-225.f + x, y, 0.f + z); break;
    case 4: zero.Set(x, y, z); break;
    case 5: zero.Set(225.f + x, y, 0.f + z); break;
    case 6: zero.Set(-225.f + x, y, 0.f - SCREEN_HALF_HEIGHT + z); break;
    case 7: zero.Set(0.f + x, y, 0.f - SCREEN_HALF_HEIGHT + z); break;
    case 8: zero.Set(225.f + x, y, 0.f - SCREEN_HALF_HEIGHT + z); break;
  }
  return zero;
}

}  // namespace eIndent
