// game/BackColor.hpp -- OWNER: WP5a (written by WP2 for the title screen)
// C# source of truth: re/cs_out/Assembly-CSharp/BackColor.cs   (translate line by line, keep names; see README "Conventions")
#pragma once
#include <string>
#include <vector>
#include "miniunity.hpp"
using namespace miniunity;

class BackColor : public Component {
public:
  static void SystemInit();
  static void SetColor(Color up, Color down, int frame = 0);
  static void SetColor(Color all, int frame = 0);
  void Awake() override;
  void Update() override;
};
