// game/GameLoop.cpp -- OWNER: WP3. Engine bring-up and the 30 Hz frame loop (docs 01 §1, 05 §0):
//   input poll -> Update (TouchPad, MainLoop: GameUpdate) -> coroutines -> LateUpdate (MenuPortSystem: UnityRenderEvent) -> render -> present (swap interval 2).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <psp2/kernel/clib.h>
#include "config.h"
#include <chrono>
#include <string>
#include "game/DemoOverlay.hpp"
#include "game/SoakStats.hpp"
#include "game/FldTest.hpp"
#include "game/Layout.hpp"
#include "game/FieldTest.hpp"
#include "game/BattleTest.hpp"
#include "game/WarpTest.hpp"
#include "game/CharaTest.hpp"
#include "game/FxTest.hpp"
#include "game/Levels.hpp"
#include "game/DevCheats.hpp"
#include "game/NativeManager.hpp"
#include "game/Sound.hpp"
#include "miniunity.hpp"
#include "native/packet_recorder.hpp"
#include "native/packet_replay.hpp"
#include "platform/cfg.hpp"
#include "platform/perf.hpp"
#include "platform/vita_input.hpp"
#include "platform/vita_platform.hpp"
#include "utils/logger.h"

using namespace miniunity;

// ux0:data/dqiv/dqivn<slot>.cfg  (written by tools/run_vita3k.sh, see platform/cfg.hpp)   shot_frames=300,600,900  -> ask Vita3K for a screenshot of those frames
static std::vector<unsigned> g_shotFrames;
static int g_battleSpeed = 1;
static void LoadCfg() {
  g_battleSpeed = std::min(4, std::max(1, Cfg::GetInt("battle_speed", 1)));
  std::string v = Cfg::Get("shot_frames");
  for (const char* p = v.c_str(); *p;) { g_shotFrames.push_back((unsigned)strtoul(p, (char**)&p, 10)); while (*p == ',' || *p == ' ') p++; }
}

int dqiv_game_run() {
  if (!Engine::Init(Layout::SurfaceW(), Layout::SurfaceH(), Layout::ViewX0(), Layout::ViewY0(), false)) return -1;
  l_success("Engine::Init: layout %d, logical surface %dx%d at (%d,%d)", (int)Layout::GetMode(), Layout::SurfaceW(), Layout::SurfaceH(), Layout::ViewX0(), Layout::ViewY0());
  Resources::Init(PACK_PATH);
  vita_platform::Init();
#ifdef DQIV_DEV_TOOLS
  Engine::RunSelfTestsIfRequested();                 // WP1 debug aid: ux0:data/dqiv/enginetest.on -> engine self tests (results in the log)
#endif
  vita_input::Init();
  PacketRecorder::Init();
  PacketReplay::Init();
  LoadCfg();

  Engine::SetLevelBuilder(Levels::Build);
#ifdef DQIV_DEV_TOOLS      // test scenes / scripted UI: compiled in only with -DDQIV_DEV_TOOLS=ON (default for Debug builds, OFF for Release)
  if (CharaTest::Requested()) CharaTest::Create();   // WP5a dev aid: ux0:data/dqiv/charatest<slot>.on -> billboard sprite test scene
  else if (WarpTest::Requested()) { WarpTest::Create(); DemoOverlay::Create(); Levels::Build("mainData"); }   // dev aid: ux0:data/dqiv/warp<slot>.on -> script of native debug hooks / Global::start* (see game/WarpTest.hpp)
  else if (BattleTest::Requested()) { BattleTest::Create(); DemoOverlay::Create(); Levels::Build("mainData"); }   // dev aid: ux0:data/dqiv/battletest<slot>.on -> force a battle through the native debug hooks
  else if (FieldTest::Requested()) { FieldTest::Create(); DemoOverlay::Create(); Levels::Build("mainData"); }   // WP5c dev aid: ux0:data/dqiv/fieldtest<slot>.on -> world-map viewer on top of the normal boot
  else if (FxTest::Requested()) FxTest::Create();         // WP6 dev aid: ux0:data/dqiv/fxtest<slot>.on -> screen effect test scene
  else if (FldTest::Requested()) FldTest::Create();       // WP5b dev aid: ux0:data/dqiv/fldtest<slot>.on -> field-map viewer instead of the title
  else Levels::Build("mainData");                     // scene 0: Main Camera + Boot

#else
  Levels::Build("mainData");                         // scene 0: Main Camera + Boot
#endif

  DevCheats::Create();                              // WP3: cfg-driven tester aids (nodamage / noencount / gold / party_hp / warp / heropos), inert without keys
  perf::LogMemory("boot");
  unsigned frame = 0;
  std::string perfScene = NativeManager::s_sceneName; double perfSum = 0; unsigned perfN = 0; double perfWorst = 0;
  auto perfLast = std::chrono::steady_clock::now();
  while (!Application::quitRequested) {
    // battle_speed=1..4 (cfg): during battles (battle stage loaded) run the whole frame (GameUpdate, coroutines, MenuPort, fades) N times per displayed frame,
    // like DQIV-Recompiled's battleBoost: the extra iterations skip Render/Present (no vsync wait) and advance Time by 1/30 s each so time-based host effects stay in step.
    for (int extra = (NativeManager::InBattleStage() && g_battleSpeed > 1) ? g_battleSpeed - 1 : 0; extra > 0; extra--) {
      vita_input::Poll();
      Engine::BeginFrame();
      Time::deltaTime = 1.f / 30.f; Time::time += 1.f / 30.f;
      Engine::RunUpdate();
      Engine::RunLateUpdate();
      Engine::EndFrame();
    }
    vita_platform::PollFrame();
    vita_input::Poll();
    Engine::BeginFrame();
    perf::Begin(perf::S_UPDATE); Engine::RunUpdate(); perf::End(perf::S_UPDATE);
    perf::Begin(perf::S_LATE); Engine::RunLateUpdate(); perf::End(perf::S_LATE);
    perf::Begin(perf::S_RENDER); Engine::Render(); perf::End(perf::S_RENDER);
    perf::Begin(perf::S_PRESENT); Engine::Present(); perf::End(perf::S_PRESENT);
    Engine::EndFrame();
    perf::Begin(perf::S_PACE); vita_platform::PaceFrame(); perf::End(perf::S_PACE);
    SoakStats::Tick(frame);
    ++frame;
    {                                                  // per-scene frame time (perf log): flushed when the scene changes and at quit
      auto now = std::chrono::steady_clock::now(); double ms = std::chrono::duration<double, std::milli>(now - perfLast).count(); perfLast = now;
      if (NativeManager::s_sceneName != perfScene) {
        if (perfN) l_info("PERF scene %s: %u frames  avg %.1f ms (%.1f Hz), worst %.0f ms", perfScene.c_str(), perfN, perfSum / perfN, 1000.0 * perfN / perfSum, perfWorst);
        perfScene = NativeManager::s_sceneName; perfSum = 0; perfN = 0; perfWorst = 0;
      }
      perfSum += ms; perfN++; if (ms > perfWorst) perfWorst = ms;
    }
    perf::FrameEnd(frame);
    if ((frame % 60) == 0) Resources::EnforceTextureBudget((size_t)DQIV_TEX_BUDGET_MB * 1024 * 1024);
    for (unsigned sf : g_shotFrames)
      if (sf == frame) { char msg[64]; snprintf(msg, sizeof(msg), "VITA3K_SCREENSHOT dqivn" DQIVN_SLOT_STR "_f%05u", frame); sceClibPrintf("%s\n", msg); l_info("screenshot requested at frame %u", frame); }
    if ((frame % 300) == 0) {
      vita_platform::PaceStats ps = vita_platform::TakePaceStats();
      l_info("frame %u (%.1f s): %u frames in window, avg %.1f ms (%.1f Hz), worst %.1f ms", frame, Time::time, ps.frames, ps.avgMs, ps.avgMs > 0 ? 1000.f / ps.avgMs : 0.f, ps.maxMs);
      SoakStats::Log(frame, ps);
    }
  }
  if (perfN) l_info("PERF scene %s: %u frames  avg %.1f ms (%.1f Hz), worst %.0f ms", perfScene.c_str(), perfN, perfSum / perfN, 1000.0 * perfN / perfSum, perfWorst);
  l_info("quit requested after %u frames (scene %s)", frame, perfScene.c_str());
  Sound::SystemQuit();                                 // MainLoop.OnApplicationQuit
  Engine::Shutdown();
  return 0;
}
