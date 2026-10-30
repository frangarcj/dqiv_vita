// game/Chara.hpp -- OWNER: WP5a. Port of Chara.cs: billboard character sprite (+ blob shadow, "lie" (sleep) meshes).
// The C# delegates (l_setColor/l_setPosition/...) became flags; semantics are identical.
#pragma once
#include <string>
#include <vector>
#include "miniunity.hpp"
#include "game/BillboardObject.hpp"
using namespace miniunity;

class Chara : public BillboardObject {
public:
  GameObject* m_chara = nullptr; Transform* m_charaT = nullptr; GameObject* m_shadow = nullptr; Material* m_material = nullptr;
  int m_patternYmax = 0; bool m_bSeparateShadow = false, m_bVisibleShadow = true;
  explicit Chara(int no);
  void Release() override;
  void SetPosition(Vector3 p) override;
  void SetColor(Color c) override;
  void SetTexture(int pose) override;
  void SetVisible(bool v) override;
  void SetFieldPosition(Vector3 p) override;
  void SetFieldMode() override;
  void SetBillboard() override;
  void SetScale(float s);
  void SetPattern(int x, int y);
  void SetShadowVisible(bool v);
  void SetSeparateShadow(bool v);
  void SetShadowPosition(Vector3 p);
  void SetShadowAlpha(float a);
  void SetFieldShadowPosition(Vector3 p);
  void SetSleepPose(bool v);
  void SetSleepDir(int dir);
  static void SystemInit();
  static void SetSleepNear(int defNear, int offset);
  static void CalcLieBillboard(Quaternion camRot);
  static void SetPriorityShader(int prio = 0);
  static void UpdateBillboardShadows();
private:
  int l_sizeType = 0, l_patternYmax = 0; Material* l_shadowMat = nullptr;
  bool l_colorWithShadow = false, l_shadowMatMade = false, l_bSleep = false;
  int l_sleepDigitalDir = 0; Quaternion l_sleepRotation = Quaternion::identity; Vector3 l_sleepPosition;
  void normalBillboard(); void lieBillboard(); void SetColorWithShadow(Color c); void SetShadowAlphaThereafter(float a);
};
