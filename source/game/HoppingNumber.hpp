// game/HoppingNumber.hpp -- OWNER: WP4a.  Port of HoppingNumber.cs (+HoppingNumberEx = FixedUpdate variant, unused): damage digits that hop and vanish.
#pragma once
#include "game/MenuParts.hpp"

class HoppingNumber : public MenuParts {
public:
  static constexpr int c_firstColom = 1000, c_maxColom = 4;
  static constexpr float c_scale = 0.8f;
  static void Create(Vector3 dispPos, int number);
  void Update() override { internalUpdate(); }
protected:
  void internalUpdate();
private:
  void Setup(Vector3 dispPos, int number);
  int m_num = 0;
  int m_waits[4] = { 0, 0, 0, 0 };
  MeshFilter* m_meshFilter = nullptr;
  Mesh* m_mesh = nullptr;
  std::vector<Vector3> m_vertices;
  std::vector<Vector2> m_uv;
};
