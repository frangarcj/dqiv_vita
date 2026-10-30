// game/ParticleSystem.hpp -- OWNER: WP5a  [HEADER-FIRST STUB: declarations are the contract, bodies in ParticleSystem.cpp are stubs]
// C# source of truth: re/cs_out/Assembly-CSharp/ParticleSystem.cs   (translate line by line, keep names; see README "Conventions")
#pragma once
#include <string>
#include <vector>
#include "miniunity.hpp"
using namespace miniunity;

class ParticleSystem {
public:
  typedef void (*FUNCS)(int param); static FUNCS s_funcs[5];
  static int s_triangles[1176]; static Vector3 s_posScale[2]; static Vector3 s_vertices[784]; static Vector2 s_uvs[784]; static Color32 s_colors[784];
  static void SystemInit();
  static void ResetNativePacket();
};
