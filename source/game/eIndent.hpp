// game/eIndent.hpp -- OWNER: WP4a.  Port of eIndent.cs: anchor ids + screen/bounds relative position helpers of the top-down UI camera (XZ plane, y = depth).
#pragma once
#include "miniunity.hpp"
using namespace miniunity;

namespace eIndent {
constexpr int LeftTop = 0, CenterTop = 1, RightTop = 2, LeftCenter = 3, Center = 4, RightCenter = 5, LeftBottom = 6, CenterBottom = 7, RightBottom = 8;
constexpr float SCREEN_HALF_WIDTH = 225.f, SCREEN_WIDTH = 450.f;
extern float SCREEN_HALF_HEIGHT, SCREEN_HEIGHT;            // 225 / Camera.main.aspect (300 / 600 on the 408x544 surface)
void Init();
Vector3 CalcLocalPosition(GameObject* reference, int indent, float x = 0.f, float y = 0.f, float z = 0.f);
Vector3 CalcPosition(GameObject* reference, int indent, float x = 0.f, float y = 0.f, float z = 0.f);
Vector3 CalcScreenPosition(int indent, float x = 0.f, float y = 0.f, float z = 0.f);
}
