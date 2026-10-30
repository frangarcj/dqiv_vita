// game/Riseup.hpp -- OWNER: WP5a. Port of Riseup.cs: rise-up icon billboard (menu-parts texture quad).
#pragma once
#include <string>
#include <vector>
#include "miniunity.hpp"
#include "game/BillboardObject.hpp"
using namespace miniunity;

class Riseup : public BillboardObject {
public:
  Transform* m_riseup = nullptr; Material* m_material = nullptr;
  explicit Riseup(int no);
  void Release() override;
  void SetTexture(int no) override;
  void SetVisible(bool v) override;
  void SetColor(Color c) override;
  void SetPosition(Vector3 p) override;
  void SetBillboard() override;
  void SetFieldMode() override;
  void SetFieldPosition(Vector3 p) override;
  static void SystemInit();
};
