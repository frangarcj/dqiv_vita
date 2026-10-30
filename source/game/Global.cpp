#include "game/Global.hpp"

int Global::Flag = 65536;
SceneCamera::InitParam Global::CameraInitParams[2] = {
  SceneCamera::InitParam(9.5f, -47.f, 0.f, 0.f, 39.55f, 5.f, 100.f, 0.0625f, true),
  SceneCamera::InitParam(48.f, 0.f, 0.f, 0.f, 39.55f, 0.25f, 500.f, 0.0625f, false, 0.275f, 0.42f, 0.04f, true),
};
void Global::InitCamera(int mode) { CameraInitParams[mode].Apply(); }
