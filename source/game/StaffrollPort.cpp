// game/StaffrollPort.cpp -- OWNER: WP6.  Line port of StaffrollPort.cs (see StaffrollPort.hpp).
#include "game/StaffrollPort.hpp"

#include "utils/logger.h"

StaffrollPort::StaffrollPort(short port) : MenuPort(port) {
  m_textures[0] = new Texture2D(512, 512, TextureFormat::ARGB32, false);
  m_textures[1] = new Texture2D(512, 512, TextureFormat::ARGB32, false);
  m_drawInstance = (new GameObject())->AddComponent<Staffroll>();
  m_drawInstance->Setup(m_textures);
  SendInts(512, m_textures[0]->GetNativeTextureID(), m_textures[1]->GetNativeTextureID());
  l_info("StaffrollPort: port %d canvases gl=%u/%u sent to native", (int)port, m_textures[0]->GetNativeTextureID(), m_textures[1]->GetNativeTextureID());
}

void StaffrollPort::Close() {
  Object::Destroy(m_drawInstance);
  m_drawInstance = nullptr;
  Object::Destroy(m_textures[1]);
  Object::Destroy(m_textures[0]);
  m_textures[0] = m_textures[1] = nullptr;
}

void StaffrollPort::SetVisible(bool bShow) { if (m_drawInstance) m_drawInstance->renderer()->enabled = bShow; }

void StaffrollPort::NoticeReceive(Param prm) {
  short num = prm[0];
  if (num == 1) {
    prm = Receive();
    Vector2 offset(0.f, prm.GetFloat(0));
    m_drawInstance->m_materials[1]->SetTextureOffset("_MainTex", offset);
    m_drawInstance->m_materials[0]->SetTextureOffset("_MainTex", offset);
  }
}

MenuPort* dqiv_CreateStaffrollPort(short port) { return new StaffrollPort(port); }
