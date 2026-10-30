// platform/cfg.cpp -- OWNER: WP3. See cfg.hpp.
#include "platform/cfg.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include "config.h"

namespace Cfg {
static std::map<std::string, std::string> g_kv;
static bool g_loaded;

void Load() {
  if (g_loaded) return;
  g_loaded = true;
  // the user-facing config (ux0:data/dqiv/config.txt, shared by all slots) is read first, the per-slot dqivn<slot>.cfg (test runs) overrides it
  for (const char* path : { DATA_PATH "config.txt", RUN_CFG_PATH }) {
    FILE* f = fopen(path, "r");
    if (!f) continue;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
      char* eq = strchr(line, '=');
      if (!eq || line[0] == '#') continue;
      *eq = 0;
      char* v = eq + 1;
      size_t n = strlen(v);
      while (n && (v[n - 1] == '\n' || v[n - 1] == '\r' || v[n - 1] == ' ')) v[--n] = 0;
      g_kv[line] = v;
    }
    fclose(f);
  }
}
std::string Get(const char* key, const char* def) { Load(); auto it = g_kv.find(key); return it == g_kv.end() ? std::string(def) : it->second; }
int GetInt(const char* key, int def) { Load(); auto it = g_kv.find(key); return it == g_kv.end() ? def : atoi(it->second.c_str()); }
bool Has(const char* key) { Load(); return g_kv.count(key) != 0; }
}  // namespace Cfg
