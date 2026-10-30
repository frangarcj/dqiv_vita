// game/MenuPort.cpp -- OWNER: WP4a.  C# source: re/cs_out/Assembly-CSharp/MenuPort.cs
#include "game/MenuPort.hpp"
#include "game/MenuPortItem.hpp"
#include "game/UiCameras.hpp"
#include "game/MenuMessage.hpp"
#include "game/MenuPortItemTable.hpp"
#include "game/MenuPortSystem.hpp"
#include "utils/logger.h"
#include <cstdio>
#include <cstring>

int MenuPort::s_frameCounter = 0;

// ---- Param ----
short MenuPort::Param::operator[](int index) const { return MenuPortSystem::s_nativeBuffer[m_offset + 1 + index]; }
short MenuPort::Param::Length() const { return MenuPortSystem::s_nativeBuffer[m_offset]; }
short MenuPort::Param::Next() const { return MenuPortSystem::s_nativeBuffer[m_offset + 1 + Length()]; }

int MenuPort::Param::GetInt(int index) const {
  index <<= 1;
  return (*this)[index] | ((*this)[index + 1] << 16);        // bug-compatible: the low half is sign-extended (02 §D)
}

float MenuPort::Param::GetFloat(int index) const {
  index <<= 1;
  uint8_t array[4];
  uint16_t num = (uint16_t)(*this)[index];
  array[0] = (uint8_t)(num & 0xFF);
  array[1] = (uint8_t)(num >> 8);
  num = (uint16_t)(*this)[index + 1];
  array[2] = (uint8_t)(num & 0xFF);
  array[3] = (uint8_t)(num >> 8);
  float f;
  std::memcpy(&f, array, 4);
  return f;
}

std::string MenuPort::Param::ConvertString() const {
  std::string text;
  int len = Length();
  for (int i = 0; i < len; i++) {
    uint32_t c = (uint16_t)(*this)[i];
    if (c >= 0xD800 && c < 0xDC00 && i + 1 < len) {                // surrogate pair -> one code point
      uint32_t lo = (uint16_t)(*this)[i + 1];
      if (lo >= 0xDC00 && lo < 0xE000) { c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00); i++; }
    }
    if (c < 0x80) text += (char)c;
    else if (c < 0x800) { text += (char)(0xC0 | (c >> 6)); text += (char)(0x80 | (c & 0x3F)); }
    else if (c < 0x10000) { text += (char)(0xE0 | (c >> 12)); text += (char)(0x80 | ((c >> 6) & 0x3F)); text += (char)(0x80 | (c & 0x3F)); }
    else { text += (char)(0xF0 | (c >> 18)); text += (char)(0x80 | ((c >> 12) & 0x3F)); text += (char)(0x80 | ((c >> 6) & 0x3F)); text += (char)(0x80 | (c & 0x3F)); }
  }
  return text;
}

std::string MenuPort::Param::Dump() const {
  char buf[64];
  snprintf(buf, sizeof(buf), "Offset[%d] Next[%d] Len[%d] ", (int)m_offset, (int)Next(), (int)Length());
  std::string text = buf;
  for (int i = 0; i < Length(); i++) { snprintf(buf, sizeof(buf), "%04X ", (unsigned)(uint16_t)(*this)[i]); text += buf; }
  return text;
}

// ---- MenuPort ----
MenuPort* MenuPort::CreateMenuPort(short port, short catego, short type) {
  MenuPort* result = nullptr;
  bool boot = UiCameras::BootEnabled() && catego == 1 && UiCameras::IsBootScreen(type);       // landscape start flow: centred panel (UiCameras.hpp)
  UiCameras::s_creatingBoot = boot;
  switch (catego) {
    case 1: result = MenuPortItemTable::CreateMenuPortItem(port, type); break;
    case 2: result = new MenuMessage(port); break;
    case 3: result = dqiv_CreateEndrollPort(port, type); break;
    case 4: result = dqiv_CreateNetConnection(port); break;
    case 5: result = dqiv_CreateSlimeTouchPort(port, type); break;
    default: Debug::LogError("Error. Not Find Unity method. catego:%d", (int)catego); break;
  }
  UiCameras::s_creatingBoot = false;
  if (result) {
    int layer = 8;
    if (boot) { layer = UiCameras::kBootLayer; result->m_bootCounted = true; UiCameras::s_bootPorts++; }
    else if (catego == 1 && type == 1 && UiCameras::BootPhase()) layer = UiCameras::kBootLayer;                 // Yes/No inside the start flow (SaveOverwrite...)
    else if (UiCameras::DialogueEnabled() && (catego == 2 || (catego == 1 && type == 1))) layer = UiCameras::kDialogueLayer;   // message window / Yes/No: bottom-centre
    if (layer != 8) {
      UiCameras::Boot(); UiCameras::Dialogue();
      result->m_uiLayer = layer;
      if (catego == 2) UiCameras::Retag(MenuMessage::m_window->transform(), layer);
      else if (MenuPortItem* it = dynamic_cast<MenuPortItem*>(result)) for (MenuParts* parts : it->m_partsList) UiCameras::RetagFrom(parts->transform(), 8, layer);
    }
  }
  return result;
}

MenuPort::Param MenuPort::Receive() { return MenuPortSystem::Receive(m_port); }
void MenuPort::SendShorts(const std::vector<short>& prms) { MenuPortSystem::Send(m_port, prms); }
void MenuPort::SendIntVec(const std::vector<int>& prms) { MenuPortSystem::Send(m_port, prms); }
void MenuPort::SendByteVec(const std::vector<unsigned char>& prms) { MenuPortSystem::Send(m_port, prms); }
