// game/MenuPortSystem.cpp -- OWNER: WP4a. See MenuPortSystem.hpp. C# source: MenuPortSystem.cs
#include "game/MenuPortSystem.hpp"
#include "game/ApplicationQuit.hpp"
#include "game/Boot.hpp"
#include "game/Global.hpp"
#include "game/SceneCamera.hpp"
#include "game/UiCameras.hpp"
#include "game/PadFocus.hpp"
#include "game/Sound.hpp"
#include "game/TouchPad.hpp"
#include "game/UiSelfTest.hpp"
#include "native/native_bridge.hpp"
#include "utils/logger.h"

short MenuPortSystem::s_unityBuffer[128];
short MenuPortSystem::s_nativeBuffer[2048];
MenuPort* MenuPortSystem::s_ports[32];
int MenuPortSystem::s_pushState = 0;
Ray MenuPortSystem::s_ray;
int MenuPortSystem::s_framesRendered = 0;
static int s_memLogPort = -1;       // set when a port closed: next frame (after the deferred Destroy ran) log the live object counts

void MenuPortSystem::LogUiMem(const char* tag) {
  Resources::Stats st = Resources::GetStats();
  l_info("UI-MEM %s: go=%d comp=%d mat=%d mesh=%d tex=%d(resident %d) live=%d", tag, st.gameObjects, st.components, st.materials, st.meshes, st.textures, st.texturesResident, st.live);
}

short* dqiv_menu_unity_buffer() { return MenuPortSystem::s_unityBuffer; }
short* dqiv_menu_native_buffer() { return MenuPortSystem::s_nativeBuffer; }

void MenuPortSystem::SystemInit() {
  NativeCall::MenuPortInit(32, s_unityBuffer, 128, s_nativeBuffer, 2048);
  GameObject* gameObject = new GameObject();
  gameObject->AddComponent<MenuPortSystem>();            // (MenuPortSystemEx / FixedUpdate variant: never used)
  MenuPort::s_frameCounter = 0;
}

void MenuPortSystem::ResetNativePacket() { NativeCall::MenuPortResetNativePacket(s_unityBuffer, s_nativeBuffer); }

void MenuPortSystem::Send(short port, const std::vector<short>& prms) {
  short num = s_unityBuffer[64];
  short num2 = s_unityBuffer[32 + port];
  if (num + (int)prms.size() + 2 > 128) { l_error("MenuPortSystem::Send: host->native buffer full (port %d, %d shorts, cursor %d)", (int)port, (int)prms.size(), (int)num); return; }
  s_unityBuffer[(s_unityBuffer[port] != 0 && num2 != 0) ? num2 : port] = num;
  s_unityBuffer[num++] = (short)prms.size();
  for (size_t i = 0; i < prms.size(); i++) s_unityBuffer[num++] = prms[i];
  s_unityBuffer[32 + port] = num;
  s_unityBuffer[num++] = 0;
  s_unityBuffer[64] = num;
  static int s_logged;
  if (s_logged++ < 400)
    l_debug("MenuPort host->native: port %d len %d [%d %d %d %d %d]", (int)port, (int)prms.size(), prms.size() > 0 ? prms[0] : 0, prms.size() > 1 ? prms[1] : 0,
          prms.size() > 2 ? prms[2] : 0, prms.size() > 3 ? prms[3] : 0, prms.size() > 4 ? prms[4] : 0);
}

void MenuPortSystem::Send(short port, const std::vector<int>& prms) {
  std::vector<short> array(prms.size() << 1);
  for (size_t i = 0; i < prms.size(); i++) {
    size_t num = i << 1;
    int num2 = prms[i];
    array[num] = (short)(num2 & 0xFFFF);
    array[num + 1] = (short)(num2 >> 16);
  }
  Send(port, array);
}

void MenuPortSystem::Send(short port, const std::vector<unsigned char>& prms) {
  size_t num = (prms.size() >> 1) + (prms.size() & 1);
  std::vector<short> array(num);
  size_t i = 0, num2 = 0;
  for (; i < prms.size(); i++) {
    if ((i & 1) == 0) { array[num2] = prms[i]; continue; }
    array[num2] |= (short)(prms[i] << 8);
    num2++;
  }
  Send(port, array);
}

MenuPort::Param MenuPortSystem::Receive(short port) {
  MenuPort::Param param;
  if (s_nativeBuffer[port] != 0) {
    short offset = s_nativeBuffer[port];
    if (offset < 65 || offset >= 2047) l_error("MenuPortSystem::Receive(%d): bad head offset %d (frame %d)", (int)port, (int)offset, Time::frameCount);
    param = MenuPort::Param(offset);
    s_nativeBuffer[port] = param.Next();
  }
  return param;
}

void MenuPortSystem::internalUpdate() {
  bool pressRay = TouchPad::GetPressRay(s_ray);
  SceneCamera::LayoutUpdate();
  NativeCall::UnityRenderEvent(1);          // "ALWAYS first (even when paused)" (01 §1): uploads the text canvas rasterised by native
  s_framesRendered++;
  if (s_memLogPort >= 0) { char t[40]; snprintf(t, sizeof(t), "after close of port %d", s_memLogPort); LogUiMem(t); s_memLogPort = -1; }
#ifdef DQIV_DEV_TOOLS
  UiSelfTest::Tick(Time::frameCount);
#endif
  if ((Global::Flag & 0x30000) != 0) {
    s_pushState = pressRay ? 4 : 0;
    return;
  }
  for (MenuPort::Param param = Receive(0); param; param = Receive(0)) {
    short num = param[0];
    MenuPort* menuPort = (num >= 0 && num < 32) ? s_ports[num] : nullptr;
    static int s_logged;
    if (s_logged++ < 400)
      l_debug("MenuPort native->host port0: [%d %d %d %d] len %d (frame %d)", (int)param[0], (int)param[1], param.Length() > 2 ? (int)param[2] : 0, param.Length() > 3 ? (int)param[3] : 0,
            (int)param.Length(), Time::frameCount);
    switch (param[1]) {
      case 1:
        if (menuPort != nullptr) Debug::LogError("Port%d is allready Opened.", (int)num);
        s_ports[num] = MenuPort::CreateMenuPort(num, param[2], param[3]);
        l_info("MenuPort: open port %d category %d type %d -> %s (frame %d)", (int)num, (int)param[2], (int)param[3], s_ports[num] ? "created" : "NULL", Time::frameCount);
        break;
      case 2:
        if (menuPort != nullptr) {
          menuPort->Close();
          if (menuPort->m_bootCounted) UiCameras::s_bootPorts--;
          delete menuPort;                    // (C#: garbage collected; the parts' GameObjects are destroyed by Close() at end of frame)
          s_ports[num] = nullptr;
          s_memLogPort = num;
          l_info("MenuPort: close port %d (frame %d)", (int)num, Time::frameCount);
        }
        break;
      case 3:
        if (menuPort) menuPort->SetVisible(param[2] != 0);
        break;
      case 4: Sound::SetVolumeSE((float)(param[2] / 25 - 1)); break;
      case 5: Sound::SetVolumeBGM((float)(param[2] / 25 - 1)); break;
      case 8: TouchPad::SetPosPadSize(param[2], param[3]); break;
      case 9: TouchPad::SetInputMode(param[2]); break;
      case 999: ApplicationQuit::Create(); break;
      default: Debug::LogError("Port%d Unknown CMD:%d", (int)num, (int)param[1]); break;
    }
  }
  switch (s_pushState) {
    case 0: if (pressRay) s_pushState = 1; break;
    case 1: s_pushState = !pressRay ? 3 : 2; break;
    case 2: if (!pressRay) s_pushState = 3; break;
    case 3: s_pushState = 0; break;
    case 4: if (!pressRay) s_pushState = 0; break;
  }
  PadFocus::BeginFrame();                    // [WP3] physical-button focus: Circle -> cancel button, see PadFocus.cpp
  for (short num2 = 1; num2 < 32; num2++) {
    MenuPort* menuPort2 = s_ports[num2];
    for (MenuPort::Param param2 = Receive(num2); param2; param2 = Receive(num2)) {
      if (menuPort2) menuPort2->NoticeReceive(param2);
    }
    if (menuPort2) {
      Ray r = s_ray;                         // landscape: ports drawn by another UI camera get the ray of the same finger through THAT camera
      if (menuPort2->m_uiLayer != 8) TouchPad::GetPressRayFor(UiCameras::ForLayer(menuPort2->m_uiLayer), r);
      menuPort2->Update(s_pushState, r);
    }
  }
  PadFocus::EndFrame();
  if (TouchPad::m_bToggleESC) {
    ApplicationQuit::Create();
    TouchPad::m_bToggleESC = false;
  }
}
