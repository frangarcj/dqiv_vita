// engine/stubs.cpp -- small engine pieces that do not deserve their own file (WP1): GameObject::CreatePrimitive.
// (colliders -> colliders.cpp, Animation/skinning -> animation.cpp, Font/Material/Mesh/Texture lifetime -> pack_glue.cpp / components.cpp)
#include "miniunity.hpp"
#include "utils/logger.h"

namespace miniunity {

#define TODO_ONCE(what) do { static bool once; if (!once) { once = true; l_warn("engine stub called: %s", what); } } while (0)

GameObject* GameObject::CreatePrimitive(int primitiveType) {
  // PrimitiveType::Quad only (Boot logo): 1x1 quad in the XY plane facing -Z, uv (0,0)-(1,1)
  (void)primitiveType;
  GameObject* g = new GameObject("Quad");
  MeshFilter* mf = g->AddComponent<MeshFilter>();
  Mesh* m = new Mesh();
  m->vertices = { Vector3(-0.5f, -0.5f, 0), Vector3(0.5f, -0.5f, 0), Vector3(0.5f, 0.5f, 0), Vector3(-0.5f, 0.5f, 0) };
  m->uv = { Vector2(0, 0), Vector2(1, 0), Vector2(1, 1), Vector2(0, 1) };
  m->SetTriangles(std::vector<int>{ 0, 2, 1, 0, 3, 2 });   // clockwise seen from the camera (Unity front face)
  mf->setMesh(m);
  g->AddComponent<MeshRenderer>();
  return g;
}

}  // namespace miniunity
