// game/FieldSymbol.hpp -- OWNER: WP5c.  Port of FieldSymbol.cs: one textured quad (flat on XZ, layer 9) cut out of the Field/field atlas (towns, castles, caves, the volcano...).
#pragma once
#include <functional>
#include "miniunity.hpp"
using namespace miniunity;

class FieldSymbol {
public:
  GameObject* m_symbol = nullptr; Material* m_material = nullptr; float m_alpha = 1.f;
  std::function<void(Color)> SetColor;
  static const Rect s_symbols[42];             // (xMin, yMin, xMax, yMax) in UV space (Rect's width/height fields hold xMax/yMax, as in the C#)
  FieldSymbol(int picid, float x, float y, Material* baseMaterial);
  void SetAlpha(float alpha);
  void Release();
  void SetUV(int picid);
private:
  void SetColorDefault(Color color) { color.a = m_alpha; m_material->color = color; }
};
