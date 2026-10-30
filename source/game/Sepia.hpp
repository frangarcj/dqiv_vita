// game/Sepia.hpp -- OWNER: WP6.  Port of Sepia.cs: screen sepia as an OnRenderImage component on the main camera (Custom/Sepia = CG op SEPIA_SCR).
// C# source of truth: re/cs_out/Assembly-CSharp/Sepia.cs
#pragma once
#include "miniunity.hpp"
using namespace miniunity;

class Sepia : public Component {
public:
  static void Setup();
  static void Release();
  void OnRenderImage(Texture* src, Texture* dst) override;
  bool HasRenderImage() const override { return true; }
};
