#include "native/packet_recorder.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "config.h"
#include "utils/logger.h"
#include "utils/utils.h"

namespace PacketRecorder {

static bool g_enabled;
static int MAX_FRAMES = 600;      // trace.on may contain a frame count (`frames=3000` or just `3000`)

bool Init() {
  g_enabled = file_exists(TRACE_ON_PATH);
  if (g_enabled) {
    if (FILE* f = fopen(TRACE_ON_PATH, "r")) { char b[64] = {}; if (fgets(b, sizeof(b), f)) { const char* p = strchr(b, '=') ? strchr(b, '=') + 1 : b; int n = atoi(p); if (n > 0) MAX_FRAMES = n; } fclose(f); } file_mkpath(TRACE_PATH, 0777); l_info("packet recorder ON (%s, first %d frames)", TRACE_PATH, MAX_FRAMES); }
  return g_enabled;
}
bool Enabled() { return g_enabled; }

void PrepareFrame(int32_t* out, int nOut, float* outF, int nOutF) {
  if (!g_enabled) return;
  for (int i = 1; i < nOut; i++) out[i] = SENTINEL_I;
  uint32_t bits = SENTINEL_F_BITS;
  float f; memcpy(&f, &bits, 4);
  for (int i = 0; i < nOutF; i++) outF[i] = f;
}

int UsedInts(const int32_t* out, int cap) {
  for (int i = cap - 1; i >= 0; i--) if (out[i] != SENTINEL_I) return i + 1;
  return 0;
}
int UsedFloats(const float* outF, int cap) {
  for (int i = cap - 1; i >= 0; i--) { uint32_t b; memcpy(&b, &outF[i], 4); if (b != SENTINEL_F_BITS) return i + 1; }
  return 0;
}

static void W32(FILE* f, uint32_t v) { fwrite(&v, 4, 1, f); }

void Record(const FrameView& v) {
  if (!g_enabled || v.frame >= MAX_FRAMES) return;
  char path[128];
  snprintf(path, sizeof(path), TRACE_PATH "%04d.pkt", v.frame);
  FILE* f = fopen(path, "wb");
  if (!f) return;
  W32(f, 0x4B505144u /* 'DQPK' */); W32(f, 1); W32(f, (uint32_t)v.frame); W32(f, (uint32_t)v.padPress); W32(f, (uint32_t)v.padToggle); W32(f, (uint32_t)v.flagIn);
  W32(f, (uint32_t)v.nIn);  fwrite(v.in, 4, v.nIn, f);
  W32(f, (uint32_t)v.nOut); fwrite(v.out, 4, v.nOut, f);
  W32(f, (uint32_t)v.nOutF); fwrite(v.outF, 4, v.nOutF, f);
  uint32_t sl = v.str0 ? (uint32_t)strlen(v.str0) + 1 : 0;
  W32(f, sl); if (sl) fwrite(v.str0, 1, sl, f);
  W32(f, (uint32_t)v.nMenuUnity);  fwrite(v.menuUnity, 2, v.nMenuUnity, f);
  W32(f, (uint32_t)v.nMenuNative); fwrite(v.menuNative, 2, v.nMenuNative, f);
  fclose(f);
}

}  // namespace PacketRecorder
