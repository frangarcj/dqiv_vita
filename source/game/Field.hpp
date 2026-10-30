// game/Field.hpp -- OWNER: WP5c.  Port of Field.cs: the world map = one 25 x 40 quad grid (16 units per quad, texture "Field/field") under an orthographic top-down
// "FieldCamera" (layer 9, mask 512), whose tile UVs come from the native FieldSet(), plus up to 114 symbol quads (FieldSymbol) and "floating" charas that follow the zoom.
#pragma once
#include <string>
#include <vector>
#include "miniunity.hpp"
using namespace miniunity;

class Field {
public:
  static Camera* m_camera;                     // C# Field.m_camera (Capture.cs disables/enables it)
  static Quaternion m_objRot;                  // C# Field.m_objRot (Field.Init sets eulerAngles (90,0,0)); Chara/MeshChara lay their sprites flat with it
  static Color m_color;
  static float c_uvAdjustZero;                 // DQ4: PowerVR/SGX half-texel UV bias (set by SystemInit; DQ5 took it from libdq4core's FieldSet via deviceInfo)
  static void SystemInit();                    // DQ4 Field.SystemInit (FieldAdjustDeviceModel -> c_uvAdjustZero, Custom/Field material)
  static void Init();
  static void Release();
  static void SetPosition(float x, float y);
  static void SetIndexList(int startX, int startY, int width, const std::vector<int>& tiles);
  static void SetSymbol(int id, int pic, float x, float y, float alpha);
  static void SetSymbolVisible(int id, bool show);
  static void SetSymbolScale(int id, float s);
  static void SetSymbolPrio(int id, float prio);
  static void SetFloatingObject(GameObject* o);
  static void RemoveFloatingObject(GameObject* o);
  static void UpdatePattern();                 // DQ4 Field.UpdatePattern (animated map tiles, called every frame while no FLD is loaded). TODO(dq4): Field port
  static void SetScale(float s);
  static void SetColor(Color c);
};
