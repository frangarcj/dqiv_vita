// platform/vita_platform.hpp -- OWNER: WP3. Power / suspend / resume handling and 30 Hz frame pacing.
//   Init():       power callback thread (logs the SceKernel power notifications; they tell us the suspend/resume bits on real hardware)
//   PollFrame():  once per frame, before the Update pass: sceAppMgrReceiveSystemEvent (ON_RESUME) + wall-clock gap detection (> 1.5 s between frames =
//                 the process was frozen by the OS) -> OnApplicationPause(true/false) equivalent: Sound.SystemPause, input latch reset
//   PaceFrame():  after Present: software 30 Hz limiter (vglSwapInterval 2 already paces real hardware; Vita3K may not honour it)
#pragma once
namespace vita_platform {
void Init();
void PollFrame();
void PaceFrame();
struct PaceStats { unsigned frames = 0; float avgMs = 0, maxMs = 0; };
PaceStats TakePaceStats();       // frames since the last call, average and worst frame time in ms
}
