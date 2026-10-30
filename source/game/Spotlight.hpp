// game/Spotlight.hpp -- OWNER: WP6.  Port of Spotlight.cs: dungeon torch/spot circle = clip-space polygon (Pre-Spotlight side quads + Post-Spotlight light texture) on the main camera.
// C# source of truth: re/cs_out/Assembly-CSharp/Spotlight.cs
#pragma once
#include "miniunity.hpp"
using namespace miniunity;

class Spotlight : public Component {
public:
  static void Create(bool torch, float x, float y, float z);
  static void SetSideColor(float r, float g, float b, float a);
  static void SetLightColor(float r, float g, float b, float a);
  static void SetSize(int radius);
  static void SetVisible(bool v);
  static void Release();
private:
  void Setup(bool torchMode, float x, float y, float z);
  Color m_LightColor = Color::white, m_SideColor = Color::white;
  GameObject* m_LightPoly = nullptr;
  Vector2 m_Pos;
  Material* m_LightMtl = nullptr; Material* m_SideMtl = nullptr;
  Texture2D* m_LightTex = nullptr;
  int m_PolySize = 64;
  bool m_IsSpot = false;
  static GameObject* RemakePolygon(GameObject*& poly, float radius, Vector2 pos, Color color);
  static void RemakeVertices(Mesh* mesh, float radius, Vector2 pos);
  static void AttachToCamera(GameObject* go);
  static void DestroyObject(GameObject* go);
  static void DestroyObject2(GameObject* go);
};
