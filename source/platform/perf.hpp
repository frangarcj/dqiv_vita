// platform/perf.hpp -- WP2.  Frame-time breakdown + memory/render counters, compiled in with -DDQIVN_MEMSTATS (CMake option DQIVN_MEMSTATS, default ON for Debug, OFF for Release).
//   Every 300 frames one "PERF" line (avg/max ms per section + backend counters per frame) and one "MEMSTATS" line (newlib heap in use / peak, vitaGL pools free, resident textures/meshes).
//   LogMemory(tag) is also called after every Resources sweep (scene change) so the per-scene peak is visible.  Output goes through _log_print(LT_INFO) so it is
//   independent of DEBUG_SOLOADER (a Release build with DQIVN_MEMSTATS still logs these lines, and only these).
#pragma once
namespace perf {
enum Sec { S_NATIVE, S_UPDATE, S_LATE, S_RENDER, S_PRESENT, S_PACE, S_COUNT };
#ifdef DQIVN_MEMSTATS
void Begin(Sec s);
void End(Sec s);
void FrameEnd(unsigned frame);
void LogMemory(const char* tag);
#else
inline void Begin(Sec) {}
inline void End(Sec) {}
inline void FrameEnd(unsigned) {}
inline void LogMemory(const char*) {}
#endif
}  // namespace perf
