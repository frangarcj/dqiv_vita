// game/SoakStats.cpp -- OWNER: WP4 (dev aid). See SoakStats.hpp.
#include "game/SoakStats.hpp"
#include <malloc.h>
#include <sys/stat.h>
#include <vitaGL.h>
#include "game/MenuPortSystem.hpp"
#include "miniunity.hpp"
#include "utils/logger.h"

using namespace miniunity;

namespace SoakStats {

static size_t s_peakUsed;

static size_t HeapUsed() { struct mallinfo mi = mallinfo(); return (size_t)mi.uordblks + (size_t)mi.hblkhd; }

void Tick(unsigned frame) {
  if ((frame % 30) != 0) return;
  size_t u = HeapUsed();
  if (u > s_peakUsed) s_peakUsed = u;
}

static int OpenFds() {
  int n = 0;
  for (int fd = 3; fd < 128; fd++) { struct stat st; if (fstat(fd, &st) == 0) n++; }
  return n;
}

void Log(unsigned frame, const vita_platform::PaceStats& ps) {
  struct mallinfo mi = mallinfo();
  size_t used = HeapUsed();
  if (used > s_peakUsed) s_peakUsed = used;
  const double MB = 1048576.0;
  Resources::Stats st = Resources::GetStats();
  int ports = 0;
  for (int i = 1; i < 32; i++) if (MenuPortSystem::s_ports[i]) ports++;
  l_info("SOAK f=%u t=%.0f ms=%.1f/%.0f heap=%.1f/%.1f/%.1f(arena) vgl_free_MB vram=%.1f/%.1f ram=%.1f/%.1f phy=%.1f/%.1f budget=%.1f/%.1f ext=%.1f/%.1f "
         "go=%d comp=%d mat=%d mesh=%d(%.2fMB) tex=%d(res %d, %.1fMB) live=%d fds=%d ports=%d",
         frame, (double)Time::time, ps.avgMs, ps.maxMs, used / MB, s_peakUsed / MB, mi.arena / MB,
         vglMemFree(VGL_MEM_VRAM) / MB, vglMemTotal(VGL_MEM_VRAM) / MB, vglMemFree(VGL_MEM_RAM) / MB, vglMemTotal(VGL_MEM_RAM) / MB,
         vglMemFree(VGL_MEM_PHYCONT) / MB, vglMemTotal(VGL_MEM_PHYCONT) / MB, vglMemFree(VGL_MEM_BUDGET) / MB, vglMemTotal(VGL_MEM_BUDGET) / MB,
         vglMemFree(VGL_MEM_EXTERNAL) / MB, vglMemTotal(VGL_MEM_EXTERNAL) / MB,
         st.gameObjects, st.components, st.materials, st.meshes, st.meshBytes / MB, st.textures, st.texturesResident, st.texBytes / MB, st.live, OpenFds(), ports);
}

}  // namespace SoakStats
