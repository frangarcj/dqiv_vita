// game/MeshChara.hpp -- OWNER: WP5a  [HEADER-FIRST STUB: declarations are the contract, bodies in MeshChara.cpp are stubs]
// C# source of truth: re/cs_out/Assembly-CSharp/MeshChara.cs   (translate line by line, keep names; see README "Conventions")
#pragma once
#include <string>
#include <vector>
#include "miniunity.hpp"
using namespace miniunity;

class MeshChara {
public:
  GameObject* m_chara = nullptr; Mesh* m_mesh2 = nullptr; MeshFilter* m_meshFilter = nullptr; Material* m_material = nullptr;
  MeshRenderer* m_renderer = nullptr; bool m_billboard_request = true;     // [WP2] C# MeshChara.m_renderer / BillboardObject.m_billboard_request
  MeshChara(int no, int shaderGroup);
  void SetTexture(int no); void SetFieldMode(); void SetFieldPosition(Vector3 pos);
  void Release();
  void SetPosition(Vector3 p);
  void SetVisible(bool v);
  void SetShader(int idx);
  void SetColor(Color c);
  void SetTextureOffset(float u, float v);
  void SetBillboard();
  static void SystemInit();
};
