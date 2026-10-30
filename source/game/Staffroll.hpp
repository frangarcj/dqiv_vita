// game/Staffroll.hpp -- OWNER: WP6.  Line port of Staffroll.cs (DQ4 staff roll renderer): two stacked 450 x 450 quads (sub-meshes 0/1) textured with the two 512 x 512 ARGB32
// canvases that the native core writes the credits text into; StaffrollPort scrolls them through _MainTex's offset.  Layer 8 = MenuCamera (the right panel in landscape).
#pragma once
#include "game/MeshMaker.hpp"
#include "miniunity.hpp"
using namespace miniunity;

class Staffroll : public Component {
public:
  Material* m_materials[2] = { nullptr, nullptr };
  void Setup(Texture2D** tex);
  void OnDestroy() override;
private:
  UnityOBJList m_uol;
};
