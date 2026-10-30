// game/BlinkMapSymbol.hpp -- OWNER: WP5c.  Port of BlinkMapSymbol.cs: red blinking squares on the small city map (MenuPortItemTable ICityMap, table code 3).
#pragma once
#include "game/MenuPort.hpp"
#include "game/MeshMaker.hpp"
#include "miniunity.hpp"
using namespace miniunity;

class BlinkMapSymbol : public Component {
public:
  void SetParameter(MenuPort::Param prm);
  void OnDestroy() override;
  void Update() override;
private:
  UnityOBJList m_uol; float m_blinkRate = 0.5f; Material* m_mat = nullptr;
};
