// platform/perf.cpp -- WP2. See perf.hpp.
#include "platform/perf.hpp"

#ifdef DQIVN_MEMSTATS
#include <malloc.h>
#include <psp2/kernel/processmgr.h>
#include <vitaGL.h>

#include "engine/backend.hpp"
#include "miniunity.hpp"
#include "utils/logger.h"

namespace perf {
namespace {
uint64_t s_t0[S_COUNT];
double s_sum[S_COUNT]; float s_max[S_COUNT];
unsigned s_n;
double s_cnt[16]; double s_why[8]; double s_shDraws[64], s_shVerts[64], s_shMul[64]; unsigned s_cntMax[2];
size_t s_heapPeak, s_arenaPeak;
const char* const kNames[S_COUNT] = { "native", "update", "late", "render", "present", "pace" };
}  // namespace

void Begin(Sec s) { s_t0[s] = sceKernelGetProcessTimeWide(); }
void End(Sec s) {
  float ms = (float)(sceKernelGetProcessTimeWide() - s_t0[s]) * 0.001f;
  s_sum[s] += ms; if (ms > s_max[s]) s_max[s] = ms;
}

void LogMemory(const char* tag) {
  struct mallinfo mi = mallinfo();
  size_t inuse = (size_t)mi.uordblks, arena = (size_t)mi.arena;
  if (inuse > s_heapPeak) s_heapPeak = inuse;
  if (arena > s_arenaPeak) s_arenaPeak = arena;
  miniunity::Resources::Stats st = miniunity::Resources::GetStats();
  const double MB = 1048576.0;
  _log_print(LT_INFO, "MEMSTATS[%s]: newlib inUse=%.1f MB (peak %.1f) arena=%.1f MB (peak %.1f) | vgl free/total MB: vram %.1f/%.1f ram %.1f/%.1f phycont %.1f/%.1f cdlg %.1f/%.1f newlib-ext %.1f/%.1f | "
             "tex %d resident %d (%.1f MB) meshes %d (%.1f MB) objects %d",
             tag, inuse / MB, s_heapPeak / MB, arena / MB, s_arenaPeak / MB,
             vglMemFree(VGL_MEM_VRAM) / MB, vglMemTotal(VGL_MEM_VRAM) / MB, vglMemFree(VGL_MEM_RAM) / MB, vglMemTotal(VGL_MEM_RAM) / MB,
             vglMemFree(VGL_MEM_PHYCONT) / MB, vglMemTotal(VGL_MEM_PHYCONT) / MB, vglMemFree(VGL_MEM_BUDGET) / MB, vglMemTotal(VGL_MEM_BUDGET) / MB,
             vglMemFree(VGL_MEM_EXTERNAL) / MB, vglMemTotal(VGL_MEM_EXTERNAL) / MB,
             st.textures, st.texturesResident, st.texBytes / MB, st.meshes, st.meshBytes / MB, st.live);
}

void FrameEnd(unsigned frame) {
  miniunity::backend::FrameStats& bs = miniunity::backend::Stats();
  s_cnt[0] += bs.draws; s_cnt[1] += bs.texBinds; s_cnt[2] += bs.progSwitches; s_cnt[3] += bs.indices; s_cnt[4] += bs.camPasses; s_cnt[5] += bs.renderables; s_cnt[6] += bs.blits;
  s_cnt[7] += bs.clientArrayBytes; s_cnt[8] += bs.cpuColorMul; s_cnt[9] += bs.idxConvert; s_cnt[10] += bs.vboDraws; s_cnt[11] += bs.tintCached;
  if ((unsigned)bs.draws > s_cntMax[0]) s_cntMax[0] = bs.draws;
  for (int i = 0; i < 64; i++) { s_shDraws[i] += bs.shDraws[i]; s_shVerts[i] += bs.shVerts[i]; s_shMul[i] += bs.shMul[i]; }
  for (int i = 0; i < 8; i++) s_why[i] += bs.vboWhy[i];
  bs = miniunity::backend::FrameStats();
  s_n++;
  if ((frame % 30) == 0) { struct mallinfo mi = mallinfo(); if ((size_t)mi.uordblks > s_heapPeak) s_heapPeak = mi.uordblks; if ((size_t)mi.arena > s_arenaPeak) s_arenaPeak = mi.arena; }
  if (s_n < 300) return;
  double n = s_n;
  double busy = s_sum[S_UPDATE] + s_sum[S_LATE] + s_sum[S_RENDER] + s_sum[S_PRESENT];
  _log_print(LT_INFO, "PERF f%u: avg ms/frame busy=%.2f [native %.2f (inside update) | host update %.2f | late %.2f | render %.2f | present %.2f] pace-wait %.2f | worst: native %.1f update %.1f render %.1f present %.1f",
             frame, busy / n, s_sum[S_NATIVE] / n, (s_sum[S_UPDATE] - s_sum[S_NATIVE]) / n, s_sum[S_LATE] / n, s_sum[S_RENDER] / n, s_sum[S_PRESENT] / n, s_sum[S_PACE] / n,
             s_max[S_NATIVE], s_max[S_UPDATE], s_max[S_RENDER], s_max[S_PRESENT]);
  _log_print(LT_INFO, "PERF f%u: per frame draws=%.1f (max %u) texBinds=%.1f progSwitches=%.2f indices=%.0f camPasses=%.1f renderables=%.0f blits=%.2f clientArrayKB=%.1f cpuColorMul=%.1f idxConvert=%.1f vboDraws=%.1f (tint-cached %.1f)",
             frame, s_cnt[0] / n, s_cntMax[0], s_cnt[1] / n, s_cnt[2] / n, s_cnt[3] / n, s_cnt[4] / n, s_cnt[5] / n, s_cnt[6] / n, s_cnt[7] / n / 1024.0, s_cnt[8] / n, s_cnt[9] / n, s_cnt[10] / n, s_cnt[11] / n);
  _log_print(LT_INFO, "PERF f%u: non-VBO draws/frame by reason: no-mesh %.1f, not-pack %.1f, instance %.1f, dynamic %.1f, ST %.1f, build-failed %.1f, tint+vcol %.1f", frame, s_why[0] / n, s_why[1] / n, s_why[2] / n, s_why[3] / n, s_why[4] / n, s_why[5] / n, s_why[6] / n);
  for (double& w : s_why) w = 0;
  // top shaders by client-array vertices (what is still copied into vitaGL's pool every frame)
  for (int k = 0; k < 3; k++) {
    int best = -1; double bv = 0;
    for (int i = 0; i < 64; i++) if (s_shVerts[i] > bv) { bv = s_shVerts[i]; best = i; }
    if (best < 0) break;
    const miniunity::backend::ShaderDesc* d = miniunity::backend::ShaderAt(best);
    _log_print(LT_INFO, "PERF f%u: top client-array shader #%d %s: %.1f draws/frame, %.0f verts/frame, %.1f cpu colour muls/frame", frame, k + 1, d ? d->name : "(default)", s_shDraws[best] / n, s_shVerts[best] / n, s_shMul[best] / n);
    s_shVerts[best] = 0;
  }
  for (int i = 0; i < 64; i++) { s_shDraws[i] = s_shVerts[i] = s_shMul[i] = 0; }
  LogMemory("periodic");
  for (int i = 0; i < S_COUNT; i++) { s_sum[i] = 0; s_max[i] = 0; }
  for (double& c : s_cnt) c = 0;
  s_cntMax[0] = 0; s_n = 0;
  (void)kNames;
}
}  // namespace perf
#endif
