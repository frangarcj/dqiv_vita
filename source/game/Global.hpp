// game/Global.hpp -- OWNER: WP3.  Port of Global.cs (+ CONST/Utility when needed). Global.Flag is round-tripped with libdq4core every frame.
#pragma once
#include "game/SceneCamera.hpp"

class Global {
public:
  static constexpr int FLAG_FADE = 1, FLAG_SCREEN_EFFECT = 2, FLAG_PLAYER_LOCK = 4, FLAG_TOUCH_DIR = 8, FLAG_TOUCH_ROT = 16, FLAG_TOUCH_MAP = 32,
                       FLAG_GUI_LOCK = 64, FLAG_SYSTEM_GUI_LOCK = 128, FLAG_PAUSE = 65536, FLAG_CLEANER = 131072;
  static constexpr int INIT_CAMRA_TOWN = 0, INIT_CAMRA_BATTLE = 0;
  static int Flag;                                                // initial 65536 (paused) until MainLoop.Start finishes
  static SceneCamera::InitParam CameraInitParams[2];
  static void InitCamera(int mode);
};
