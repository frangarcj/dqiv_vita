// game/BillboardObject.hpp -- OWNER: WP5a. Port of BillboardObject.cs (abstract base of Chara / Riseup).
#pragma once
#include <string>
#include <vector>
#include "miniunity.hpp"
using namespace miniunity;

class BillboardObject {
public:
  bool m_billboard_request = true;
  virtual ~BillboardObject() {}
  virtual void SetBillboard() = 0;
  virtual void SetPosition(Vector3 pos) = 0;
  virtual void SetVisible(bool bVisible) = 0;
  virtual void SetTexture(int no) = 0;
  virtual void SetColor(Color color) = 0;
  virtual void Release() = 0;
  virtual void SetFieldMode() = 0;
  virtual void SetFieldPosition(Vector3 pos) = 0;
  static void SystemInit();
  static Quaternion s_shadowRot; static Transform* s_billboardTarget;
};
