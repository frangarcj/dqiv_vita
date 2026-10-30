// game/Capture.hpp -- OWNER: WP6.  Port of Capture.cs: screen transition effects (battle shatter, dark lamp, Riremito, Tabi/travel warp) built from a RenderTexture capture of the
// main/field camera + a blurred copy of the menu camera, drawn as a deformed grid by the MenuCamera ("CaptureCTRL" object on layer 8).  Also routes SCREEN_EFFECT_SPOTLIGHT/SEPIA.
// C# source of truth: re/cs_out/Assembly-CSharp/Capture.cs
#pragma once
#include <string>
#include <vector>
#include "miniunity.hpp"
using namespace miniunity;

class Capture : public Component {
public:
  static void SystemInit();
  static void ScreenEffect(int effect);        // reads extra floats with NativeManager::GetFloatEx for the spotlight sub-commands
  static void Snap();
  static bool IsEndSnap();
  static void SetVisible(bool show);
  bool HasCoStart() const override { return true; }
  Coroutine StartCo() override;
  void Awake() override;
  void LateUpdate() override;
private:
  static Coroutine DummyFunc(), BattleEffect(), DarkLamp(bool morning), Riremito(), Tabi(bool reverse);
  static void SetMesh(std::vector<Vector3>& vertices, std::vector<Color32>& colors);
  static void SetBlur(bool b);
  void Caputer();
  void SnapShot();
  bool m_ready = false;
};
