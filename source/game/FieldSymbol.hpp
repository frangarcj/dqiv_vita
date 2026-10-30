// game/FieldSymbol.hpp -- OWNER: WP5c.  Port of FieldSymbol.cs (DQ4): one textured quad (flat on XZ, layer 9) cut out of the Field/field atlas (towns, castles, caves...). 21 symbols.
#pragma once
#include "miniunity.hpp"
using namespace miniunity;

class FieldSymbol {
public:
  GameObject* m_symbol = nullptr; Material* m_material = nullptr; float m_alpha = 1.f;
  static const Rect s_symbols[21];             // (xMin, yMin, xMax, yMax) in UV space (Rect's width/height fields hold xMax/yMax, as in the C#)
  FieldSymbol(int picid, float x, float y, Material* baseMaterial);
  void SetColor(Color color);
  void SetAlpha(float alpha);
  void Release();
};
