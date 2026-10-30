// game/StaffrollPort.hpp -- OWNER: WP6.  Line port of StaffrollPort.cs: MenuPort category 3 (DQ4; DQ5 had EndrollPort).  Two 512 x 512 ARGB32 canvases whose GL names are handed to
// the native core (SendInts(512, id0, id1)); the native draws the credits into them and scrolls with port message [1] + float offset.
#pragma once
#include "game/MenuPort.hpp"
#include "game/Staffroll.hpp"

class StaffrollPort : public MenuPort {
public:
  static constexpr int m_texSize = 512;
  Texture2D* m_textures[2] = { nullptr, nullptr };
  Staffroll* m_drawInstance = nullptr;
  explicit StaffrollPort(short port);
  void Close() override;
  void SetVisible(bool bShow) override;
  void NoticeReceive(Param prm) override;
};
