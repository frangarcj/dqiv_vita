// game/MainLoop.cpp -- OWNER: WP3. C# source: MainLoop.cs. The SystemInit order matters (01 §1.1): DevInit, ..., ParticleSystemInit, ..., MenuPortInit, GameInit.
#include "game/MainLoop.hpp"

#include <cstdio>
#include <string>

#include "game/BackColor.hpp"
#include "game/BillboardObject.hpp"
#include "game/Capture.hpp"
#include "game/Chara.hpp"
#include "game/Fade.hpp"
#include "game/Global.hpp"
#include "game/MenuParts.hpp"
#include "game/MenuPortSystem.hpp"
#include "game/MeshChara.hpp"
#include "game/NativeManager.hpp"
#include "game/ParticleSystem.hpp"
#include "game/Riseup.hpp"
#include "game/SceneCamera.hpp"
#include "game/Sound.hpp"
#include "game/TouchPad.hpp"
#include "native/native_bridge.hpp"
#include "platform/vita_input.hpp"
#include "utils/logger.h"

Camera* MainLoop::m_camera = nullptr;
bool MainLoop::s_ready = false;

Coroutine MainLoop::StartCo() {
  Debug::Log("MainLoop.Start()");
  m_camera = camera();
  Object::DontDestroyOnLoad(gameObject);
  if (!NativeCall::DeviceInit()) l_error("DevInit returned false: libdq4core could not open its data (OBB / filelist)");
  BackColor::SystemInit();
  Fade::SystemInit();
  Capture::SystemInit();
  BillboardObject::SystemInit();
  Chara::SystemInit();
  Riseup::SystemInit();
  MeshChara::SystemInit();
  ParticleSystem::SystemInit();
  SceneCamera::SystemInit();
  TouchPad::AdjustCameraOrtho();
  MenuParts::SystemInit();
  MenuPortSystem::SystemInit();
  Sound::SystemInit();
  NativeManager::Init();
  l_success("libdq4core: DevInit + GameInit done");
  Vector3 pos(0.f, 0.f, 0.f);
  Global::InitCamera(0);
  SceneCamera::SetAngleDeg(Global::CameraInitParams[0].GetAngleVector());
  SceneCamera::SetTarget(pos);
  SceneCamera::Update();
  co_yield 0;
  Global::Flag &= ~0x10000;      // FLAG_PAUSE cleared: from now on NativeManager.Update runs each frame
  s_ready = true;
  l_success("MainLoop: running (Global.Flag=0x%x)", Global::Flag);
}

static unsigned s_loggedFrames;
static bool s_firstUpdate = true;
static int s_prevFlag = -1, s_prevPress = -1, s_prevToggle = -1;
static bool s_opSeen[NativeManager::NUM_CMDS];

void MainLoop::Update() {
  if ((Global::Flag & 0x30000) != 0) return;
  if (s_firstUpdate) { s_firstUpdate = false; vita_input::SetPreGame(false); l_info("STATE f%d: first GameUpdate (pad word %03x/%03x)", Time::frameCount, TouchPad::m_press, TouchPad::m_toggle); }
  NativeManager::Update();
  Sound::AudioTestUpdate();

  // ---- state timeline (grep "STATE" in dqivn.log): Global.Flag changes, first appearance of every packet opcode; MenuPort opens/closes are logged by MenuPortSystem ----
  const NativeManager::FrameStats& st = NativeManager::LastFrame();
  if (st.flagOut != s_prevFlag) { l_info("STATE f%d: Global.Flag %05x -> %05x", st.frame, s_prevFlag < 0 ? 0 : s_prevFlag, st.flagOut); s_prevFlag = st.flagOut; }
  for (int i = 0; i < NativeManager::NUM_CMDS; i++)
    if (st.cmdCount[i] && !s_opSeen[i]) { s_opSeen[i] = true; l_info("STATE f%d: first op%d (%s)", st.frame, i, NativeManager::OpName(i)); }

  if (TouchPad::m_press != s_prevPress || TouchPad::m_toggle != s_prevToggle) {
    l_info("STATE f%d: pad press/toggle %03x/%03x", st.frame, TouchPad::m_press, TouchPad::m_toggle);
    s_prevPress = TouchPad::m_press; s_prevToggle = TouchPad::m_toggle;
  }

  // ---- packet dumper: one line per frame (first 120 frames, then every command frame and every 30th) ----
  bool log = st.frame < 120 || st.nCmds > 0 || (st.frame % 30) == 0 || st.desync;
  if (log) {
    char ops[256]; int n = 0; ops[0] = 0;
    for (int i = 0; i < NativeManager::NUM_CMDS && n < (int)sizeof(ops) - 12; i++)
      if (st.cmdCount[i]) n += snprintf(ops + n, sizeof(ops) - n, " op%d*%d", i, st.cmdCount[i]);
    l_info("F%04d pad=%03x/%03x flag=%05x cmds=%d ints=%d floats=%d%s%s", st.frame, TouchPad::m_press, TouchPad::m_toggle, st.flagOut, st.nCmds, st.nInts, st.nFloats,
           ops, st.desync ? " DESYNC" : "");
  }
  s_loggedFrames++;
}

void MainLoop::OnApplicationPause(bool pause) {
  Sound::SystemPause(pause);
  if (!pause) { NativeManager::ResetNativePacket(); MenuPortSystem::ResetNativePacket(); ParticleSystem::ResetNativePacket(); }
}
void MainLoop::OnApplicationQuit() { Sound::SystemQuit(); }
