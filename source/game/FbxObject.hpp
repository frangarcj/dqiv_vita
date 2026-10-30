// game/FbxObject.hpp -- OWNER: WP5b.  Port of FbxObject.cs: one skinned character / monster model (`fbx/cNNNN` prefab) with its legacy Animation ("Take 001"...).
#pragma once
#include <string>
#include <vector>
#include "miniunity.hpp"
using namespace miniunity;

class FbxObject {
public:
  Transform* m_obj = nullptr; std::vector<Material*> m_materials; float m_blend_time = 0.3f;
  explicit FbxObject(int no);
  void MaterialScan(std::vector<Material*>& list, Transform* a);
  void Release();
  void SetRotation(Vector3 r);
  void SetVisible(bool v);
  void SetScale(Vector3 s);
  void SetColor(Color c);
  void SetPosition(Vector3 p);
  void PlayAnimation(int seqNo, bool bForce = false);
  bool IsEndAnimation();
private:
  float m_defScale = 1.f; WrapMode m_mode = WrapMode::Default;
};
