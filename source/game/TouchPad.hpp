// game/TouchPad.hpp -- OWNER: WP3.  Port of TouchPad.cs + RawPad.cs + RawPads.cs, reduced to what the Vita needs (06 §3):
//   * the pad word that reaches GameUpdate(m_press, m_toggle): physical buttons (vita_input) OR the touch "ANY" tap (0x100 toggle, native maps it to A),
//   * RawPad/RawPads verbatim (press/toggle edges of Input.touches[0], rays through a camera) -> the hit-test source for every MenuPort button,
//   * the ray API MenuPortSystem needs: GetPressRay/GetToggleRay/IsTouchPress, m_pressRay, m_camera (= MenuCamera).
// NOT ported: the on-screen virtual pad (ring, rotate bar, Map/Command/Talk/PadPos buttons, MOVE/ROT/KEY states): physical buttons replace it (06 §3).
// SetPosPadSize/SetInputMode/SetPadBlink are accepted (their state is kept: l_inputMode decides nothing here but is queryable).
#pragma once
#include "miniunity.hpp"
using namespace miniunity;

class RawPad {
public:
  int fingerId = 0; Vector2 position, deltaPosition; float deltaTime = 0; int tapCount = 0; TouchPhase phase = TouchPhase::Ended;
  bool bPress = false, bToggle = false; Vector2 beginPosition;
  void SetTouch(const Touch& t);        // C# `touch` setter
};

class RawPads {
public:
  bool bIsPress() const { return m_pad.bPress; }
  bool bIsToggle() const { return m_pad.bToggle; }
  Ray GetRay(Camera* cam) const;        // C# Ray(cam)
  Vector3 WorldDelta(Camera* cam) const;
  Vector3 BeginWorldDelta(Camera* cam) const;
  void Update();                        // once per frame from Input.touches[0]
  const RawPad& pad() const { return m_pad; }
private:
  RawPad m_pad;
};

class TouchPad : public Component {
public:
  enum ePadState { NONE, MOVE_BEGIN, MOVE, MOVE_BEGIN2, MOVE2, ROT_OR_FLICK, KEY, ROT_L, ROT_R, PRESS_WAIT, PPOS, ANY };
  static constexpr int PAD_NONE = 0, PAD_U = 1, PAD_D = 2, PAD_L = 4, PAD_R = 8, PAD_BL = 16, PAD_BR = 32, PAD_FL = 64, PAD_FR = 128, PAD_ANY = 256, PAD_A = 512,
                       PAD_B = 1024, PAD_X = 2048, PAD_Y = 4096, PAD_START = 8192, PAD_SELECT = 16384, PAD_DEBUG = 32768, PAD_UDLR = 15, PAD_BLR = 48, PAD_ALL = 63;

  static Camera* m_camera;
  static int m_press, m_toggle, m_externalToggle;   // GameUpdate(m_press, m_toggle)
  static Ray m_pressRay;
  static bool m_bVisible, m_bPressESC, m_bToggleESC;
  static RawPads m_pad;

  static void SetPadBlink(bool on);
  static void SetActivity(bool enable);
  static bool GetPressRayFor(Camera* cam, Ray& ray);   // same finger, seen through another UI camera (landscape dialogue windows)
  static bool GetPressRay(Ray& ray);                 // true while a finger is down (ray of this frame)
  static bool GetToggleRay(Ray& ray);                // true on the frame the finger went down
  static bool IsTouchPress() { return m_pad.bIsPress(); }
  static void AdjustCameraOrtho();                   // MenuCamera.orthographicSize = 225 / aspect
  static void SetPosPadSize(int pos, int size);
  static void SetInputMode(int mode);
  static int  GetInputMode();

  void Awake() override;
  void Update() override;                            // factUpdate()
private:
  static int s_prevPress;
  static ePadState l_state; static bool l_NoMove; static Vector2 l_prsStart; static bool l_bSystemLock;
  static int l_inputMode, l_padPos, l_padSize; static bool l_padBlink;
};
