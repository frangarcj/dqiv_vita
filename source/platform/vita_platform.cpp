// platform/vita_platform.cpp -- OWNER: WP3. See vita_platform.hpp.
#include "platform/vita_platform.hpp"

#include <atomic>
#include <psp2/appmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/power.h>

#include "game/Sound.hpp"
#include "platform/cfg.hpp"
#include "utils/logger.h"

namespace vita_platform {

static std::atomic<int> s_powerEvents{0};
static std::atomic<unsigned> s_lastPowerInfo{0};
static uint64_t s_lastFrame, s_frameStart;
static unsigned s_frames, s_total; static double s_sumMs; static float s_maxMs;

static int PowerCb(int notifyId, int notifyCount, int powerInfo, void*) {
  s_powerEvents++;
  s_lastPowerInfo = (unsigned)powerInfo;
  l_info("power callback: notifyId=%d count=%d powerInfo=0x%08x", notifyId, notifyCount, (unsigned)powerInfo);
  return 0;
}

static int CallbackThread(SceSize, void*) {
  SceUID cb = sceKernelCreateCallback("dqivn_power", 0, PowerCb, nullptr);
  if (cb >= 0) scePowerRegisterCallback(cb);
  for (;;) sceKernelDelayThreadCB(100 * 1000);
  return 0;
}

void Init() {
  SceUID th = sceKernelCreateThread("dqivn_cb", CallbackThread, 0x10000100, 0x4000, 0, 0, nullptr);
  if (th >= 0) sceKernelStartThread(th, 0, nullptr);
  s_lastFrame = sceKernelGetProcessTimeWide();
}

static void OnResume(const char* why) {
  l_info("platform: resume (%s)", why);
  Sound::SystemPause(true);          // MainLoop.OnApplicationPause(true/false): Sound.SystemPause -> SdSoundSystem_Suspend/Resume; the packet arrays are static (no ResetNativePacket)
  Sound::SystemPause(false);
}

void PollFrame() {
  SceAppMgrSystemEvent ev;
  // (the ON_RESUME system event is only drained: Vita3K reports it on every call; real suspends freeze the process and show up as a frame gap below)
  (void)sceAppMgrReceiveSystemEvent(&ev);
  uint64_t now = sceKernelGetProcessTimeWide();
  if (s_lastFrame && now - s_lastFrame > 1500000ull && s_total > 10) OnResume("frame gap");
  s_lastFrame = now;
  s_frameStart = now;
}

void PaceFrame() {
  constexpr uint64_t PERIOD_US = 33333;
  uint64_t now = sceKernelGetProcessTimeWide();
  uint64_t el = now - s_frameStart;
#ifdef DQIV_DEV_TOOLS
  static const bool turbo = Cfg::GetInt("turbo", 0) != 0;      // tools/run_vita3k.sh TURBO=1: no 30 Hz limit, the game is frame-locked so scripted tests just finish sooner
#else
  constexpr bool turbo = false;
#endif
  if (!turbo && el < PERIOD_US - 1500) sceKernelDelayThread((SceUInt)(PERIOD_US - 1000 - el));       // vsync (interval 2) normally already did the waiting
  now = sceKernelGetProcessTimeWide();
  float ms = (float)(now - s_frameStart) / 1000.f;
  s_total++; s_frames++; s_sumMs += ms; if (ms > s_maxMs) s_maxMs = ms;
}

PaceStats TakePaceStats() {
  PaceStats p; p.frames = s_frames; p.avgMs = s_frames ? (float)(s_sumMs / s_frames) : 0.f; p.maxMs = s_maxMs;
  s_frames = 0; s_sumMs = 0; s_maxMs = 0;
  return p;
}

}  // namespace vita_platform
