// native/packet_replay.cpp -- OWNER: WP3. See packet_replay.hpp. Format: native/packet_recorder.hpp.
#include "native/packet_replay.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "config.h"
#include "utils/logger.h"
#include "utils/utils.h"

namespace PacketReplay {

static bool g_active, g_quit;
static std::string g_dir = TRACE_PATH;
static int g_played;

bool Init() {
  if (!file_exists(REPLAY_ON_PATH)) return false;
  FILE* f = fopen(REPLAY_ON_PATH, "r");
  if (f) {
    char line[256];
    while (fgets(line, sizeof(line), f)) {
      size_t n = strlen(line);
      while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
      if (!strncmp(line, "dir=", 4)) { g_dir = line + 4; if (!g_dir.empty() && g_dir.back() != '/') g_dir += '/'; }
      else if (!strncmp(line, "quit=1", 6)) g_quit = true;
    }
    fclose(f);
  }
  g_active = true;
  l_info("packet REPLAY ON: frames come from %s (GameUpdate is not called)", g_dir.c_str());
  return true;
}
bool Active() { return g_active; }
bool QuitAtEnd() { return g_quit; }
int FramesPlayed() { return g_played; }

static bool R32(FILE* f, uint32_t& v) { return fread(&v, 4, 1, f) == 1; }

bool LoadFrame(int frame, int* out, float* outF, char* str0, int strCap, short* menuNative) {
  char path[256];
  snprintf(path, sizeof(path), "%s%04d.pkt", g_dir.c_str(), frame);
  FILE* f = fopen(path, "rb");
  if (!f) return false;
  uint32_t magic, ver, fr, press, toggle, flagIn, n;
  bool ok = R32(f, magic) && magic == 0x4B505144u && R32(f, ver) && R32(f, fr) && R32(f, press) && R32(f, toggle) && R32(f, flagIn);
  std::vector<int32_t> tmp;
  if (ok) { ok = R32(f, n); if (ok) { tmp.resize(n); ok = fread(tmp.data(), 4, n, f) == n; } }                 // host->native input stream (ignored)
  if (ok) { ok = R32(f, n) && n <= 8192; if (ok) { ok = fread(out, 4, n, f) == n; } }
  if (ok) { ok = R32(f, n) && n <= 8192; if (ok) { ok = fread(outF, 4, n, f) == n; } }
  if (ok) { ok = R32(f, n) && (int)n <= strCap; if (ok) { ok = n == 0 || fread(str0, 1, n, f) == n; if (n == 0) str0[0] = 0; } }
  if (ok) { ok = R32(f, n); if (ok) { tmp.assign((n + 1) / 2 + 1, 0); ok = fread(tmp.data(), 2, n, f) == n; } }   // host->native menu buffer (ignored)
  if (ok) {
    ok = R32(f, n) && n <= 2048;
    if (ok) { memset(menuNative, 0, 2048 * sizeof(short)); ok = fread(menuNative, 2, n, f) == n; }
  }
  fclose(f);
  if (!ok) { l_error("PacketReplay: %s is truncated / corrupt", path); return false; }
  g_played++;
  return true;
}

}  // namespace PacketReplay
