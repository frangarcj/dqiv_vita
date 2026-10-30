// game/GetLangages.cpp -- OWNER: WP4a.  Language selection (DQ4: en, ko, zh-tw, zh-cn only) (C#: GetLangages.Init() read the Android system language; the Vita reads `lang=` from the config instead).
#include "game/GetLangages.hpp"
#include <cstring>
#include <strings.h>
#include "platform/cfg.hpp"
#include "utils/logger.h"

void GetLangages::Init() {
  std::string v = Cfg::Get("lang");
  int n = m_langagesNumber;
  if (!v.empty()) {
    if (!strcasecmp(v.c_str(), "ja") || !strcasecmp(v.c_str(), "jp") || v == "0") l_warn("config: lang=%s: Japanese game text is not shipped in DQ4 (OBB msg/{en,ko,sl,tl}), keeping %d", v.c_str(), n);
    else if (!strcasecmp(v.c_str(), "en") || v == "1") n = 1;
    else if (!strcasecmp(v.c_str(), "ko") || !strcasecmp(v.c_str(), "kr") || v == "2") n = 2;
    else if (!strcasecmp(v.c_str(), "zh-tw") || !strcasecmp(v.c_str(), "tc") || !strcasecmp(v.c_str(), "zht") || v == "3") n = 3;
    else if (!strcasecmp(v.c_str(), "zh-cn") || !strcasecmp(v.c_str(), "sc") || !strcasecmp(v.c_str(), "zhs") || v == "4") n = 4;
    else l_warn("config: unknown lang=%s (use en|ko|zh-tw|zh-cn), keeping %d", v.c_str(), n);
  }
  m_langagesNumber = n;
  l_info("Language: %d (1 EN, 2 KO, 3 zh-TW, 4 zh-CN; Japanese is not shipped)", n);
}
