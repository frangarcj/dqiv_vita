// game/Volcano.hpp -- OWNER: WP6 (hand-over from WP2).  Port of Volcano.cs: the world-map volcano symbol (mountain quad + crater + 3 rising smoke quads, Custom/FieldVolcano).
// FieldSymbol (WP5c) adds it for picid 19.  C# source of truth: re/cs_out/Assembly-CSharp/Volcano.cs
#pragma once
#include "miniunity.hpp"
using namespace miniunity;

class Volcano : public Component {
public:
  void Init(Mesh* mesh, Material* mat, float w, float h, Rect mountain, Rect volcano, Rect smoke);
  void Update() override;
  void OnDestroy() override;
private:
  void SetVerticesColors(int index, float x, float y, float z, float size, float w, float h, Color color);
  Material* m_material = nullptr;
  Mesh* m_mesh = nullptr;
  Vector3 m_vertices[20];
  Color m_colors[20];
  float m_smoke_w = 0.f, m_smoke_h = 0.f;
  int m_counter = 0;
  int m_smoke[3] = { 0, 8, 16 };
};
