// game/MenuPortSystem.hpp -- OWNER: WP4a.  Port of MenuPortSystem.cs (+ MenuPortSystemEx, which only moves internalUpdate to FixedUpdate: never used, Boot.s_useFixedUpdate == false).
// The MenuPort mailbox (02 §D): SystemInit / ResetNativePacket / Send / Receive are the C# verbatim; LateUpdate = internalUpdate():
//   UnityRenderEvent(1) (text-canvas upload) -> [if not paused] port-0 system commands -> push-state machine -> per-port NoticeReceive*/Update.
#pragma once
#include <vector>
#include "game/MenuPort.hpp"
#include "miniunity.hpp"
using namespace miniunity;

class MenuPortSystem : public Component {
public:
  static short s_unityBuffer[129];        // host -> native
  static short s_nativeBuffer[1089];      // native -> host
  static MenuPort* s_ports[32];
  static void SystemInit();               // NativeCall.MenuPortInit(32, unity, 128, native, 2048) + spawns the component
  static void ResetNativePacket();
  static void Send(short port, const std::vector<short>& prms);
  static void Send(short port, const std::vector<int>& prms);
  static void Send(short port, const std::vector<unsigned char>& prms);
  static MenuPort::Param Receive(short port);   // invalid Param (operator bool == false) when the port has no message
  void LateUpdate() override { internalUpdate(); }
  static int s_framesRendered;
  static void LogUiMem(const char* tag);     // live GameObjects/components/materials/meshes/textures (logged after every port close: must stay flat over open/close cycles)
private:
  void internalUpdate();
  static int s_pushState;
  static Ray s_ray;
};
