// game/Fade.hpp -- OWNER: WP5a.  [WP3 wrote the first full port because FLAG_FADE (Global.Flag bit 0) gates the whole pad/touch input: hand over to WP5a as is]
// C# source of truth: re/cs_out/Assembly-CSharp/Fade.cs   (translate line by line, keep names; see README "Conventions")
#pragma once
#include <string>
#include <vector>
#include "miniunity.hpp"
using namespace miniunity;

class Fade : public Component {
public:
  static void SystemInit();
  static void SetLayer(int layer);
  static void SetFadeOutColor(int frames, Color c);
  static void SetFadeOutWhite(int frames);
  static void SetFadeOutBlack(int frames);
  static void SetFadeIn(int frames);
  static bool IsEnd();
  static void SetFadeInBlack(int frames);
  static void SetFadeInWhite(int frames);
  bool HasCoStart() const override { return true; }
  Coroutine StartCo() override;      // DQ4: IEnumerator Start() waits for FLAG_PAUSE to clear before building the planes (DQ5: Awake)
  void Update() override;
  static void SetFadePriority(int layer, int priority);
  // landscape (Layout::Wide): full-surface cameras for everything that must cover the whole surface although the MenuCamera is only the panel.
  //   [0] "front": depth 5, layer 18 (menu fade);  [1] "BG": depth 0.9 (behind the UI, above the DSSA camera), layer 19 (BG fade, battle clear plane, Capture effects)
  static Camera* FullCamera(int i);
  static int FullLayer(int i) { return 18 + i; }
};
